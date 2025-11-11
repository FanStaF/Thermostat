#include "ConfigManager.h"

ConfigManager::ConfigManager(RelayController& relayCtrl)
  : relayController(relayCtrl) {
}

void ConfigManager::begin() {
  if (!LittleFS.begin()) {
    logger.addLog("ERROR: LittleFS mount failed!");
  } else {
    logger.addLog("LittleFS mounted OK");
  }
}

void ConfigManager::saveSettings(int updateFrequency, bool useFahrenheit) {
  logger.addLog("Saving settings...");

  StaticJsonDocument<1024> doc;

  JsonArray modes = doc.createNestedArray("modes");
  for (int i = 0; i < 4; i++) {
    modes.add((int)relayController.getRelayMode(i));
  }

  JsonArray onArr = doc.createNestedArray("tempOn");
  for (int i = 0; i < 4; i++) {
    onArr.add(relayController.getTempOn(i));
  }

  JsonArray offArr = doc.createNestedArray("tempOff");
  for (int i = 0; i < 4; i++) {
    offArr.add(relayController.getTempOff(i));
  }

  doc["updateFrequency"] = updateFrequency;
  doc["useFahrenheit"] = useFahrenheit;

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) {
    logger.addLog("ERROR: Failed to open config file for writing");
    return;
  }

  size_t bytesWritten = serializeJson(doc, f);
  f.close();

  logger.addLog("Settings saved (" + String(bytesWritten) + " bytes)");

  // Print what was saved for debugging
  String jsonStr;
  serializeJson(doc, jsonStr);
  logger.addLog("JSON: " + jsonStr);
}

void ConfigManager::loadSettings(int& updateFrequency, bool& useFahrenheit) {
  logger.addLog("Loading settings...");

  if (!LittleFS.exists(CONFIG_FILE)) {
    logger.addLog("Config file not found, using defaults");
    return;
  }

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) {
    logger.addLog("ERROR: Failed to open config file");
    return;
  }

  size_t fileSize = f.size();
  logger.addLog("Config file size: " + String(fileSize) + " bytes");

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, f);
  f.close();

  if (error) {
    logger.addLog("ERROR: Failed to parse config: " + String(error.c_str()));
    return;
  }

  // Print loaded JSON for debugging
  String jsonStr;
  serializeJson(doc, jsonStr);
  logger.addLog("Loaded JSON: " + jsonStr);

  // Load relay modes
  if (doc.containsKey("modes")) {
    JsonArray modes = doc["modes"];
    int i = 0;
    for (int m : modes) {
      if (i < 4) {
        relayController.setRelayMode(i, (Mode)m);
        i++;
      }
    }
  }

  // Load temp ON thresholds
  if (doc.containsKey("tempOn")) {
    JsonArray a = doc["tempOn"];
    int i = 0;
    float tempOnValues[4];
    for (float v : a) {
      if (i < 4) tempOnValues[i++] = v;
    }
    // Update thresholds
    for (int j = 0; j < i; j++) {
      relayController.setTempThresholds(j, tempOnValues[j], relayController.getTempOff(j));
    }
  }

  // Load temp OFF thresholds
  if (doc.containsKey("tempOff")) {
    JsonArray a = doc["tempOff"];
    int i = 0;
    float tempOffValues[4];
    for (float v : a) {
      if (i < 4) tempOffValues[i++] = v;
    }
    // Update thresholds
    for (int j = 0; j < i; j++) {
      relayController.setTempThresholds(j, relayController.getTempOn(j), tempOffValues[j]);
    }
  }

  // Load update frequency
  if (doc.containsKey("updateFrequency")) {
    updateFrequency = doc["updateFrequency"];
  }

  // Load temperature unit preference
  if (doc.containsKey("useFahrenheit")) {
    useFahrenheit = doc["useFahrenheit"];
  }

  logger.addLog("Settings loaded successfully");
}
