#include "ConfigManager.h"

ConfigManager::ConfigManager(RelayController& relayCtrl)
  : relayController(relayCtrl) {
}

void ConfigManager::begin() {
  if (!LittleFS.begin()) {
    logger.addLog("ERROR: LittleFS mount failed!");
  }
}

void ConfigManager::saveSettings(int updateFrequency, bool useFahrenheit) {
  StaticJsonDocument<1024> doc;

  JsonArray modes = doc.createNestedArray("modes");
  JsonArray types = doc.createNestedArray("types");
  JsonArray onArr = doc.createNestedArray("tempOn");
  JsonArray offArr = doc.createNestedArray("tempOff");
  for (int i = 0; i < 4; i++) {
    modes.add((int)relayController.getRelayMode(i));
    types.add((int)relayController.getRelayType(i));
    onArr.add(relayController.getTempOn(i));
    offArr.add(relayController.getTempOff(i));
  }

  doc["updateFrequency"] = updateFrequency;
  doc["useFahrenheit"] = useFahrenheit;

  // Write to a temp file first and rename over the real one, so a full
  // filesystem or a reset mid-write can never leave config.json truncated.
  File f = LittleFS.open(CONFIG_TMP_FILE, "w");
  if (!f) {
    logger.addLog("ERROR: Failed to save config (open)");
    return;
  }
  size_t expected = measureJson(doc);
  size_t written = serializeJson(doc, f);
  f.close();

  if (written != expected) {
    LittleFS.remove(CONFIG_TMP_FILE);
    logger.addLog("ERROR: Failed to save config (disk full?) - previous settings kept");
    return;
  }

  if (!LittleFS.rename(CONFIG_TMP_FILE, CONFIG_FILE)) {
    // Some FS versions refuse to rename over an existing file.
    LittleFS.remove(CONFIG_FILE);
    if (!LittleFS.rename(CONFIG_TMP_FILE, CONFIG_FILE)) {
      logger.addLog("ERROR: Failed to save config (rename)");
    }
  }
}

void ConfigManager::loadSettings(int& updateFrequency, bool& useFahrenheit) {
  // Recover from a save that was interrupted between remove and rename.
  if (!LittleFS.exists(CONFIG_FILE) && LittleFS.exists(CONFIG_TMP_FILE)) {
    LittleFS.rename(CONFIG_TMP_FILE, CONFIG_FILE);
    logger.addLog("Recovered config from temp file");
  }

  if (!LittleFS.exists(CONFIG_FILE)) {
    logger.addLog("No config file, using defaults");
    return;
  }

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) {
    logger.addLog("ERROR: Failed to open config file");
    return;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, f);
  f.close();

  if (error) {
    logger.addLog("ERROR: Failed to parse config: " + String(error.c_str()));
    return;
  }

  // Every value is validated; anything invalid keeps its default and is logged.
  int rejected = 0;

  JsonArray modes = doc["modes"];
  JsonArray types = doc["types"];
  JsonArray onArr = doc["tempOn"];
  JsonArray offArr = doc["tempOff"];

  for (int i = 0; i < 4; i++) {
    if (!modes.isNull() && i < (int)modes.size()) {
      if (!modes[i].is<int>() || !relayController.setRelayMode(i, (Mode)modes[i].as<int>())) rejected++;
    }
    if (!types.isNull() && i < (int)types.size()) {
      if (!types[i].is<int>() || !relayController.setRelayType(i, (RelayType)types[i].as<int>())) rejected++;
    }
    float on = relayController.getTempOn(i);
    float off = relayController.getTempOff(i);
    if (!onArr.isNull() && i < (int)onArr.size()) {
      if (onArr[i].is<float>()) on = onArr[i].as<float>(); else rejected++;
    }
    if (!offArr.isNull() && i < (int)offArr.size()) {
      if (offArr[i].is<float>()) off = offArr[i].as<float>(); else rejected++;
    }
    if (!relayController.setTempThresholds(i, on, off) &&
        !relayController.setTempThresholds(i, off, on)) rejected++;
  }

  if (doc["updateFrequency"].is<int>()) {
    int freq = doc["updateFrequency"];
    if (freq >= MIN_UPDATE_FREQUENCY && freq <= MAX_UPDATE_FREQUENCY) updateFrequency = freq;
    else rejected++;
  }

  if (doc["useFahrenheit"].is<bool>()) {
    useFahrenheit = doc["useFahrenheit"];
  }

  if (rejected > 0) {
    logger.addLog("WARN: " + String(rejected) + " invalid config value(s) ignored, defaults used");
  }
}
