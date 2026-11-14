#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <time.h>

#include "Config.h"

// Without a password anyone on the LAN can flash this relay controller.
#if !defined(OTA_PASSWORD)
#error "Define OTA_PASSWORD in Credentials.h (see Credentials.h.example)"
#endif
static_assert(sizeof(OTA_PASSWORD) > 8, "OTA_PASSWORD must be at least 8 characters");
#include "SystemLogger.h"
#include "TemperatureManager.h"
#include "RelayController.h"
#include "ConfigManager.h"
#include "WebInterface.h"
#include "ApiClient.h"

// ---- Global Variables ----
int updateFrequency = DEFAULT_UPDATE_FREQUENCY;
bool useFahrenheit = false;
unsigned long lastTempUpdate = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastCommandPoll = 0;
unsigned long lastRegisterAttempt = 0;
int consecutiveSensorFails = 0;
unsigned long lastGoodReadMs = 0;   // millis() of the last good reading
bool everHadGoodRead = false;

static bool tryRegister();

// ---- Module Instances ----
TemperatureManager tempManager;
RelayController relayController;
ConfigManager configManager(relayController);
ApiClient apiClient(API_URL);
WebInterface webInterface(tempManager, relayController, configManager, apiClient, updateFrequency, useFahrenheit);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== Thermostat " FIRMWARE_VERSION " ===");

  // Initialize filesystem and load configuration
  configManager.begin();
  configManager.loadSettings(updateFrequency, useFahrenheit);

  // Initialize relay controller
  relayController.begin();

  // Initialize temperature sensor
  tempManager.begin();

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  logger.addLog("Connecting to WiFi...");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_MAX_ATTEMPTS) {
    delay(WIFI_RETRY_DELAY);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    logger.addLog("WiFi connected: " + WiFi.localIP().toString());
  } else {
    logger.addLog("ERROR: WiFi connection failed!");
  }

  // Setup OTA
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    logger.addLog("OTA Update Starting...");
  });
  ArduinoOTA.onEnd([]() {
    logger.addLog("OTA Update Complete!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total > 0) Serial.printf("Progress: %u%%\r", (unsigned)((uint64_t)progress * 100 / total));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    logger.addLog("OTA Error: " + String(error));
  });
  ArduinoOTA.begin();
  logger.addLog("OTA ready");

  // Setup NTP time sync
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  logger.addLog("NTP time sync started");

  // Get initial temperature and apply relay logic BEFORE the web server
  // starts so /status returns sensible values immediately.
  // If this fails there is NO trustworthy temperature: start in sensor-fault
  // mode (AUTO relays OFF) instead of inventing a value.
  TempReadResult initial = tempManager.readBlocking();
  if (initial.ok) {
    tempManager.setCurrentTemp(initial.temp);
    logger.addLog("Initial temp: " + String(initial.temp, 1) + "C");
    lastGoodReadMs = millis();
    everHadGoodRead = true;
  } else {
    logger.addLog("WARN: Initial sensor read failed (" + String(initial.lastFailReason) + ")");
    relayController.setSensorFault(true);
    consecutiveSensorFails = 1;
  }

  logger.addLog("Applying relay settings...");
  for (int i = 0; i < 4; i++) {
    String msg = "R" + String(i + 1) + ": " + RelayController::modeToString(relayController.getRelayMode(i));
    msg += " (ON=" + String(relayController.getTempOn(i), 1) + "C, OFF=" + String(relayController.getTempOff(i), 1) + "C)";
    logger.addLog(msg);
  }
  relayController.applyRelayLogic(tempManager.getCurrentTemp());

  // Start the local web server BEFORE talking to the backend so the device is
  // reachable on the LAN even if the backend is slow or down.
  apiClient.begin();
  webInterface.begin();

  // Register with backend (one short blocking call). If it fails, the loop
  // retries every REGISTER_RETRY_INTERVAL; the local UI works regardless.
  tryRegister();

  // Queue initial relay state sync — drained one per loop iteration so we
  // never block the web server with a startup burst of HTTP calls.
  for (int i = 0; i < 4; i++) {
    apiClient.markRelayDirty(i);
  }

  logger.addLog("=== SETUP COMPLETE ===");
  logger.addLog("Web: http://" + WiFi.localIP().toString());
}

// Attempt backend registration. Returns true if registered.
static bool tryRegister() {
  lastRegisterAttempt = millis();
  if (WiFi.status() != WL_CONNECTED) return false;
  if (apiClient.registerDevice(WiFi.getHostname(), WiFi.macAddress(),
                               WiFi.localIP().toString(), FIRMWARE_VERSION)) {
    logger.addLog("Device registered with Laravel backend");
    return true;
  }
  logger.addLog("WARNING: Failed to register with backend, will retry");
  return false;
}

// Process a single backend command. The caller pops it from the queue after
// this returns; for the "restart" case we drain the rest of the queue here.
static void processCommand(ApiClient::Command& cmd) {
  logger.addLog("Processing command: " + cmd.type);
  apiClient.updateCommandStatus(cmd.id, "acknowledged");

  bool success = false;
  String result = "";

  if (cmd.type == "set_relay_mode") {
    JsonDocument paramsDoc;
    DeserializationError error = deserializeJson(paramsDoc, cmd.params);
    if (!error) {
      int relayNum = paramsDoc["relay_number"];
      String mode = paramsDoc["mode"].as<String>();
      Mode newMode;
      if (RelayController::parseMode(mode, newMode) &&
          relayController.setRelayMode(relayNum - 1, newMode)) {
        relayController.applyRelayLogic(tempManager.getCurrentTemp());
        configManager.saveSettings(updateFrequency, useFahrenheit);
        apiClient.markRelayDirty(relayNum - 1);
        success = true;
        result = "Relay " + String(relayNum) + " mode set to " + mode;
        logger.addLog(result);
      }
    }
  }
  else if (cmd.type == "set_thresholds") {
    JsonDocument paramsDoc;
    DeserializationError error = deserializeJson(paramsDoc, cmd.params);
    if (!error) {
      int relayNum = paramsDoc["relay_number"];
      // Missing/non-numeric values become NAN and are rejected by the setter.
      float tempOn = paramsDoc["temp_on"].is<float>() ? paramsDoc["temp_on"].as<float>() : NAN;
      float tempOff = paramsDoc["temp_off"].is<float>() ? paramsDoc["temp_off"].as<float>() : NAN;
      if (relayController.setTempThresholds(relayNum - 1, tempOn, tempOff)) {
        relayController.applyRelayLogic(tempManager.getCurrentTemp());
        configManager.saveSettings(updateFrequency, useFahrenheit);
        apiClient.markRelayDirty(relayNum - 1);
        success = true;
        result = "Relay " + String(relayNum) + " thresholds updated";
        logger.addLog(result);
      }
    }
  }
  else if (cmd.type == "set_relay_type") {
    JsonDocument paramsDoc;
    DeserializationError error = deserializeJson(paramsDoc, cmd.params);
    if (!error) {
      int relayNum = paramsDoc["relay_number"];
      String type = paramsDoc["relay_type"].as<String>();
      RelayType newType;
      if (RelayController::parseType(type, newType) &&
          relayController.setRelayType(relayNum - 1, newType)) {
        relayController.applyRelayLogic(tempManager.getCurrentTemp());
        configManager.saveSettings(updateFrequency, useFahrenheit);
        apiClient.markRelayDirty(relayNum - 1);
        success = true;
        result = "Relay " + String(relayNum) + " type set to " + type;
        logger.addLog(result);
      }
    }
  }
  else if (cmd.type == "set_frequency") {
    JsonDocument paramsDoc;
    DeserializationError error = deserializeJson(paramsDoc, cmd.params);
    if (!error) {
      int requested = paramsDoc["frequency"] | 0;
      if (requested >= 1 && requested <= MAX_UPDATE_FREQUENCY) {
        // A validated read takes ~2.3s, so clamp to the firmware minimum and
        // say so in the result, so the backend can see the applied value.
        int freq = requested < MIN_UPDATE_FREQUENCY ? MIN_UPDATE_FREQUENCY : requested;
        updateFrequency = freq;
        configManager.saveSettings(updateFrequency, useFahrenheit);
        success = true;
        result = "Update frequency set to " + String(freq) + "s";
        if (freq != requested) {
          result += " (requested " + String(requested) + "s, minimum is " + String(MIN_UPDATE_FREQUENCY) + "s)";
        }
        logger.addLog(result);
      }
    }
  }
  else if (cmd.type == "set_unit") {
    JsonDocument paramsDoc;
    DeserializationError error = deserializeJson(paramsDoc, cmd.params);
    if (!error && paramsDoc["use_fahrenheit"].is<bool>()) {
      bool useFahr = paramsDoc["use_fahrenheit"];
      useFahrenheit = useFahr;
      configManager.saveSettings(updateFrequency, useFahrenheit);
      success = true;
      result = "Temperature unit set to " + String(useFahr ? "Fahrenheit" : "Celsius");
      logger.addLog(result);
    }
  }
  else if (cmd.type == "restart") {
    success = true;
    result = "Restarting device...";
    logger.addLog(result);
    apiClient.updateCommandStatus(cmd.id, "completed", result);
    // Mark any remaining queued commands as failed so the server doesn't think
    // they're still being executed across the reboot.
    apiClient.popNextCommand();
    while (apiClient.hasPendingCommands()) {
      ApiClient::Command* next = apiClient.peekNextCommand();
      if (next) apiClient.updateCommandStatus(next->id, "failed", "Device restarting");
      apiClient.popNextCommand();
    }
    delay(1000);
    ESP.restart();
    return; // not reached
  }

  if (success) {
    apiClient.updateCommandStatus(cmd.id, "completed", result);
  } else {
    logger.addLog("Command rejected: " + cmd.type + " (invalid or unknown parameters)");
    apiClient.updateCommandStatus(cmd.id, "failed", "Invalid or unknown command parameters");
  }
}

void loop() {
  ArduinoOTA.handle();
  webInterface.handleClient();

  unsigned long now = millis();

  // ---- WiFi reconnect ----
  static unsigned long lastWifiCheck = 0;
  if (WiFi.status() != WL_CONNECTED && now - lastWifiCheck >= WIFI_RECONNECT_INTERVAL) {
    lastWifiCheck = now;
    logger.addLog("WiFi disconnected, reconnecting...");
    WiFi.reconnect();
  }

  // ---- Periodic temperature read ----
  // Non-blocking: start a reading on schedule, then poll() advances it each
  // pass so the web server and OTA never stall behind sensor conversions.
  // While the sensor is failing, retry at the minimum interval so the
  // fail-safe engages promptly even with a long configured interval.
  unsigned long readIntervalMs = (unsigned long)(consecutiveSensorFails > 0 ? MIN_UPDATE_FREQUENCY : updateFrequency) * 1000;
  if (!tempManager.isReading() && now - lastTempUpdate >= readIntervalMs) {
    lastTempUpdate = now;
    tempManager.startReading();
  }

  TempReadResult r;
  if (tempManager.poll(r)) {
    if (r.ok) {
      // Surface a retry-recovery so the user can see flaky-bus events without
      // having to read between successful reads.
      if (r.attemptsTaken > 1 && r.lastFailReason) {
        logger.addLog(String("Sensor recovered after retry (") + r.lastFailReason + ")");
      }
      // End-of-streak summary: only log if we'd been actually failing (>=1
      // both-attempts-failed event), not just retrying.
      if (consecutiveSensorFails > 0) {
        logger.addLog("Sensor recovered after " + String(consecutiveSensorFails) + " failed read(s)");
        consecutiveSensorFails = 0;
      }

      tempManager.setCurrentTemp(r.temp);
      lastGoodReadMs = millis();
      everHadGoodRead = true;
      relayController.setSensorFault(false);
      Serial.print("Temp: ");
      Serial.print(r.temp, 1);
      Serial.print("C | Relays: ");

      bool previousStates[4];
      for (int i = 0; i < 4; i++) previousStates[i] = relayController.getRelayState(i);

      relayController.applyRelayLogic(r.temp);

      for (int i = 0; i < 4; i++) {
        Serial.print(i + 1);
        Serial.print(":");
        Serial.print(relayController.getRelayState(i) ? "ON" : "OFF");
        Serial.print("(");
        Serial.print(RelayController::modeToString(relayController.getRelayMode(i)));
        Serial.print(") ");
        if (relayController.getRelayState(i) != previousStates[i]) {
          apiClient.markRelayDirty(i);
        }
      }
      Serial.println();

      tempManager.logTemperature(r.temp, 0);
      apiClient.markTempDirty();
    } else {
      consecutiveSensorFails++;
      String msg = String("Sensor read failed (") + (r.lastFailReason ? r.lastFailReason : "unknown") + ")";
      if (tempManager.hasValidTemp()) msg += ", last good " + String(tempManager.getCurrentTemp(), 1) + "C";
      if (consecutiveSensorFails > 1) {
        msg += " [" + String(consecutiveSensorFails) + " consecutive]";
      }
      logger.addLog(msg);

      // Fail safe: a dead/loose sensor must not leave heating or cooling
      // latched ON. Time-based (not count-based) so the delay doesn't scale
      // with the configured interval; the min-fails guard stops one glitch
      // after a long interval from tripping it.
      bool stale = !everHadGoodRead || millis() - lastGoodReadMs >= SENSOR_FAILSAFE_MS;
      if (stale && consecutiveSensorFails >= SENSOR_FAILSAFE_MIN_FAILS && !relayController.hasSensorFault()) {
        bool previousStates[4];
        for (int i = 0; i < 4; i++) previousStates[i] = relayController.getRelayState(i);
        relayController.setSensorFault(true);
        relayController.applyRelayLogic(NAN);
        for (int i = 0; i < 4; i++) {
          if (relayController.getRelayState(i) != previousStates[i]) apiClient.markRelayDirty(i);
        }
      }
    }
  }

  // ---- Drain pending API work, AT MOST ONE blocking call per loop iteration ----
  // Order: temperature first (most time-sensitive), then any one dirty relay,
  // then heartbeat, then a single pending command. The web server gets a turn
  // between each one.
  if (!apiClient.isRegistered() && WiFi.status() == WL_CONNECTED &&
      now - lastRegisterAttempt >= REGISTER_RETRY_INTERVAL) {
    tryRegister();
  } else if (apiClient.isRegistered() && WiFi.status() == WL_CONNECTED) {
    static bool lastReportedFault = false;
    if (apiClient.isTempDirty() && tempManager.hasValidTemp()) {
      if (apiClient.sendTemperatureReading(tempManager.getCurrentTemp(), 0)) {
        apiClient.clearTempDirty();
      }
    } else {
      int dirtyRelay = apiClient.nextDirtyRelay();
      if (dirtyRelay >= 0) {
        bool  state   = relayController.getRelayState(dirtyRelay);
        String mode   = RelayController::modeToString(relayController.getRelayMode(dirtyRelay));
        float tempOn  = relayController.getTempOn(dirtyRelay);
        float tempOff = relayController.getTempOff(dirtyRelay);

        // Skip the network round trip if the backend already has this exact
        // state — covers boot-time blast, no-op web clicks, and command
        // handlers that re-mark dirty without actually changing anything.
        if (apiClient.relayStateMatchesLastSent(dirtyRelay, state, mode, tempOn, tempOff)) {
          apiClient.clearRelayDirty(dirtyRelay);
        } else if (apiClient.sendRelayState(
              dirtyRelay + 1, state, mode, tempOn, tempOff,
              "Relay " + String(dirtyRelay + 1))) {
          apiClient.recordRelaySent(dirtyRelay, state, mode, tempOn, tempOff);
          apiClient.clearRelayDirty(dirtyRelay);
        }
        // If the send failed, leave dirty=true so the next loop iteration retries.
      } else if (now - lastHeartbeat >= API_HEARTBEAT_INTERVAL ||
                 (relayController.hasSensorFault() != lastReportedFault && now - lastHeartbeat >= 10000)) {
        // Heartbeat carries the sensor-fault flag; a fault change is sent
        // straight away instead of waiting for the next interval.
        lastHeartbeat = now;
        long age = everHadGoodRead ? (long)((millis() - lastGoodReadMs) / 1000) : -1;
        if (apiClient.sendHeartbeat(relayController.hasSensorFault(), age)) {
          lastReportedFault = relayController.hasSensorFault();
        }
      } else if (apiClient.hasPendingCommands()) {
        ApiClient::Command* cmd = apiClient.peekNextCommand();
        if (cmd) {
          processCommand(*cmd);
          apiClient.popNextCommand();
        }
      } else if (now - lastCommandPoll >= API_COMMAND_POLL_INTERVAL) {
        lastCommandPoll = now;
        apiClient.pollCommands();
      }
    }
  }

  // ---- Periodic heap snapshot for debugging fragmentation ----
  static unsigned long lastHeapLog = 0;
  if (now - lastHeapLog >= HEAP_LOG_INTERVAL) {
    lastHeapLog = now;
    logger.addLog("Heap: " + String(ESP.getFreeHeap()) + "B free, frag " + String(ESP.getHeapFragmentation()) + "%");
  }

  delay(10);
}
