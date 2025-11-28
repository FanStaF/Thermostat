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

  // Initialize API client and register device
  apiClient.begin();
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

  // Setup web server
  webInterface.begin();

  // Get initial temperature with validation
  float initialTemp;
  if (tempManager.readTemperatureWithValidation(initialTemp)) {
    tempManager.setCurrentTemp(initialTemp);
    logger.addLog("Initial temp: " + String(initialTemp, 1) + "C");
  } else {
    tempManager.setCurrentTemp(20.0); // Safe default if sensor not working
    logger.addLog("WARNING: Sensor read failed, using default 20.0C");
  }

  // Apply loaded settings to relays
  logger.addLog("Applying relay settings...");
  for (int i = 0; i < 4; i++) {
    String msg = "R" + String(i + 1) + ": " + RelayController::modeToString(relayController.getRelayMode(i));
    msg += " (ON=" + String(relayController.getTempOn(i), 1) + "C, OFF=" + String(relayController.getTempOff(i), 1) + "C)";
    logger.addLog(msg);
  }
  relayController.applyRelayLogic(tempManager.getCurrentTemp());

  // Send initial relay states to Laravel API
  if (apiClient.isRegistered() && WiFi.status() == WL_CONNECTED) {
    logger.addLog("Sending initial relay states...");
    for (int i = 0; i < 4; i++) {
      apiClient.sendRelayState(
        i + 1,
        relayController.getRelayState(i),
        RelayController::modeToString(relayController.getRelayMode(i)),
        relayController.getTempOn(i),
        relayController.getTempOff(i),
        "Relay " + String(i + 1)
      );
    }
  }

  logger.addLog("=== SETUP COMPLETE ===");
  logger.addLog("Web: http://" + WiFi.localIP().toString());
}

void loop() {
  ArduinoOTA.handle();
  webInterface.handleClient();

  unsigned long now = millis();

  // Read temperature periodically
  if (now - lastTempUpdate >= (unsigned long)updateFrequency * 1000) {
    lastTempUpdate = now;

    float newTemp;
    if (tempManager.readTemperatureWithValidation(newTemp)) {
      // Valid temperature reading
      tempManager.setCurrentTemp(newTemp);
      Serial.print("Temp: ");
      Serial.print(newTemp, 1);
      Serial.print("C | Relays: ");

      // Track if any relay states changed
      bool relayStatesChanged = false;
      bool previousStates[4];
      for (int i = 0; i < 4; i++) {
        previousStates[i] = relayController.getRelayState(i);
      }

      relayController.applyRelayLogic(newTemp);

      // Print relay states
      for (int i = 0; i < 4; i++) {
        Serial.print(i + 1);
        Serial.print(":");
        Serial.print(relayController.getRelayState(i) ? "ON" : "OFF");
        Serial.print("(");
        Serial.print(RelayController::modeToString(relayController.getRelayMode(i)));
        Serial.print(") ");

        if (relayController.getRelayState(i) != previousStates[i]) {
          relayStatesChanged = true;
        }
      }
      Serial.println();

      tempManager.logTemperature(newTemp, 0);

      // Send temperature to Laravel API
      if (apiClient.isRegistered() && WiFi.status() == WL_CONNECTED) {
        apiClient.sendTemperatureReading(newTemp, 0);

        // Send relay states if changed
        if (relayStatesChanged) {
          for (int i = 0; i < 4; i++) {
            apiClient.sendRelayState(
              i + 1,
              relayController.getRelayState(i),
              RelayController::modeToString(relayController.getRelayMode(i)),
              relayController.getTempOn(i),
              relayController.getTempOff(i),
              "Relay " + String(i + 1)
            );
          }
        }
      }
    } else {
      // Failed to read valid temperature, keep using last known good value
      logger.addLog("Using last known temp: " + String(tempManager.getCurrentTemp(), 1) + "C");
    }
  }

  // Send heartbeat to API periodically
  if (apiClient.isRegistered() && WiFi.status() == WL_CONNECTED) {
    if (now - lastHeartbeat >= API_HEARTBEAT_INTERVAL) {
      lastHeartbeat = now;
      apiClient.sendHeartbeat();
    }

    // Poll for pending commands
    if (now - lastCommandPoll >= API_COMMAND_POLL_INTERVAL) {
      lastCommandPoll = now;
      if (apiClient.pollCommands()) {
        int commandCount = apiClient.getPendingCommandCount();
        for (int i = 0; i < commandCount; i++) {
          ApiClient::Command cmd = apiClient.getPendingCommands()[i];
          logger.addLog("Processing command: " + cmd.type);

          // Acknowledge command immediately
          apiClient.updateCommandStatus(cmd.id, "acknowledged");

          // Process command based on type
          bool success = false;
          String result = "";

          if (cmd.type == "set_relay_mode") {
            // Parse JSON params
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

                // Send updated state back to server
                int relayIdx = relayNum - 1;
                apiClient.sendRelayState(
                  relayNum,
                  relayController.getRelayState(relayIdx),
                  RelayController::modeToString(relayController.getRelayMode(relayIdx)),
                  relayController.getTempOn(relayIdx),
                  relayController.getTempOff(relayIdx),
                  "Relay " + String(relayNum)
                );

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

                // Send updated state back to server
                int relayIdx = relayNum - 1;
                apiClient.sendRelayState(
                  relayNum,
                  relayController.getRelayState(relayIdx),
                  RelayController::modeToString(relayController.getRelayMode(relayIdx)),
                  relayController.getTempOn(relayIdx),
                  relayController.getTempOff(relayIdx),
                  "Relay " + String(relayNum)
                );

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

                // Send updated state back to server
                int relayIdx = relayNum - 1;
                apiClient.sendRelayState(
                  relayNum,
                  relayController.getRelayState(relayIdx),
                  RelayController::modeToString(relayController.getRelayMode(relayIdx)),
                  relayController.getTempOn(relayIdx),
                  relayController.getTempOff(relayIdx),
                  "Relay " + String(relayNum)
                );

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
            success = true;
            result = "Restarting device...";
            logger.addLog(result);
            apiClient.updateCommandStatus(cmd.id, "completed", result);
            delay(1000);
            ESP.restart();
          }

          // Mark command as completed or failed
          if (success) {
            apiClient.updateCommandStatus(cmd.id, "completed", result);
          } else {
            apiClient.updateCommandStatus(cmd.id, "failed", "Command execution failed");
          }
        }
      }
    }
  }

  delay(10);
}
