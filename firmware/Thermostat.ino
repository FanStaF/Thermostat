#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <time.h>

#include "Config.h"
#include "SystemLogger.h"
#include "TemperatureManager.h"
#include "RelayController.h"
#include "ConfigManager.h"
#include "WebInterface.h"

// ---- Global Variables ----
int updateFrequency = DEFAULT_UPDATE_FREQUENCY;
bool useFahrenheit = false;
unsigned long lastTempUpdate = 0;

// ---- Module Instances ----
TemperatureManager tempManager;
RelayController relayController;
ConfigManager configManager(relayController);
WebInterface webInterface(tempManager, relayController, configManager, updateFrequency, useFahrenheit);

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

  logger.addLog("=== SETUP COMPLETE ===");
  logger.addLog("Web: http://" + WiFi.localIP().toString());
}

void loop() {
  ArduinoOTA.handle();
  webInterface.handleClient();

  // Read temperature periodically
  unsigned long now = millis();
  if (now - lastTempUpdate >= (unsigned long)updateFrequency * 1000) {
    lastTempUpdate = now;

    float newTemp;
    if (tempManager.readTemperatureWithValidation(newTemp)) {
      // Valid temperature reading
      tempManager.setCurrentTemp(newTemp);
      Serial.print("Temp: ");
      Serial.print(newTemp, 1);
      Serial.print("C | Relays: ");
      for (int i = 0; i < 4; i++) {
        Serial.print(i + 1);
        Serial.print(":");
        Serial.print(relayController.getRelayState(i) ? "ON" : "OFF");
        Serial.print("(");
        Serial.print(RelayController::modeToString(relayController.getRelayMode(i)));
        Serial.print(") ");
      }
      Serial.println();
      relayController.applyRelayLogic(newTemp);
      tempManager.logTemperature(newTemp, 0);
    } else {
      // Failed to read valid temperature, keep using last known good value
      logger.addLog("Using last known temp: " + String(tempManager.getCurrentTemp(), 1) + "C");
    }
  }

  delay(10);
}
