#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <time.h>

#include "Config.h"
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

// ---- Module Instances ----
TemperatureManager tempManager;
RelayController relayController;
ConfigManager configManager(relayController);
ApiClient apiClient(API_URL);
WebInterface webInterface(tempManager, relayController, configManager, apiClient, updateFrequency, useFahrenheit);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== DEBUG1 - 4 Relay Test ===");

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
  ArduinoOTA.onStart([]() {
    logger.addLog("OTA Update Starting...");
  });
  ArduinoOTA.onEnd([]() {
    logger.addLog("OTA Update Complete!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
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
  TempReadResult initial = tempManager.readTemperatureWithValidation();
  if (initial.ok) {
    tempManager.setCurrentTemp(initial.temp);
    logger.addLog("Initial temp: " + String(initial.temp, 1) + "C");
  } else {
    tempManager.setCurrentTemp(20.0); // Safe default if sensor not working
    logger.addLog("WARN: Initial sensor read failed (" + String(initial.lastFailReason) + "), using default 20.0C");
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

  // Register with backend (one short blocking call). Even if this fails the
  // local UI keeps working; the loop will retry sync via dirty flags.
  if (WiFi.status() == WL_CONNECTED) {
    String hostname = WiFi.getHostname();
    String macAddress = WiFi.macAddress();
    String ipAddress = WiFi.localIP().toString();

    if (apiClient.registerDevice(hostname, macAddress, ipAddress, FIRMWARE_VERSION)) {
      logger.addLog("Device registered with Laravel backend");
    } else {
      logger.addLog("WARNING: Failed to register with backend");
    }
  }

  // Queue initial relay state sync — drained one per loop iteration so we
  // never block the web server with a startup burst of HTTP calls.
  for (int i = 0; i < 4; i++) {
    apiClient.markRelayDirty(i);
  }

  logger.addLog("=== SETUP COMPLETE ===");
  logger.addLog("Web: http://" + WiFi.localIP().toString());
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
      if (relayNum >= 1 && relayNum <= 4) {
        Mode newMode = Mode::AUTO;
        if (mode == "MANUAL_ON") newMode = Mode::MANUAL_ON;
        else if (mode == "MANUAL_OFF") newMode = Mode::MANUAL_OFF;
        relayController.setRelayMode(relayNum - 1, newMode);
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
      float tempOn = paramsDoc["temp_on"];
      float tempOff = paramsDoc["temp_off"];
      if (relayNum >= 1 && relayNum <= 4) {
        relayController.setTempThresholds(relayNum - 1, tempOn, tempOff);
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
      if (relayNum >= 1 && relayNum <= 4) {
        RelayType newType = RelayType::HEATING;
        if (type == "COOLING") newType = RelayType::COOLING;
        else if (type == "GENERIC") newType = RelayType::GENERIC;
        else if (type == "MANUAL_ONLY") newType = RelayType::MANUAL_ONLY;
        relayController.setRelayType(relayNum - 1, newType);
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
      int freq = paramsDoc["frequency"];
      if (freq >= 1 && freq <= 60) {
        updateFrequency = freq;
        configManager.saveSettings(updateFrequency, useFahrenheit);
        success = true;
        result = "Update frequency set to " + String(freq) + "s";
        logger.addLog(result);
      }
    }
  }
  else if (cmd.type == "set_unit") {
    JsonDocument paramsDoc;
    DeserializationError error = deserializeJson(paramsDoc, cmd.params);
    if (!error) {
      bool useFahr = paramsDoc["use_fahrenheit"];
      useFahrenheit = useFahr;
      configManager.saveSettings(updateFrequency, useFahrenheit);
      success = true;
      result = "Temperature unit set to " + String(useFahr ? "Fahrenheit" : "Celsius");
      logger.addLog(result);
    }
  }
  else if (cmd.type == "restart") {
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
    apiClient.updateCommandStatus(cmd.id, "failed", "Command execution failed");
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
  if (now - lastTempUpdate >= (unsigned long)updateFrequency * 1000) {
    lastTempUpdate = now;

    static int consecutiveSensorFails = 0;
    TempReadResult r = tempManager.readTemperatureWithValidation();

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
      String msg = String("Sensor read failed (") + (r.lastFailReason ? r.lastFailReason : "unknown")
                 + "), kept last " + String(tempManager.getCurrentTemp(), 1) + "C";
      if (consecutiveSensorFails > 1) {
        msg += " [" + String(consecutiveSensorFails) + " consecutive]";
      }
      logger.addLog(msg);
    }
  }

  // ---- Drain pending API work, AT MOST ONE blocking call per loop iteration ----
  // Order: temperature first (most time-sensitive), then any one dirty relay,
  // then heartbeat, then a single pending command. The web server gets a turn
  // between each one.
  if (apiClient.isRegistered() && WiFi.status() == WL_CONNECTED) {
    if (apiClient.isTempDirty()) {
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
      } else if (now - lastHeartbeat >= API_HEARTBEAT_INTERVAL) {
        lastHeartbeat = now;
        apiClient.sendHeartbeat();
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
