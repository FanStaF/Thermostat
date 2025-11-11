#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "SystemLogger.h"
#include "RelayController.h"

class ConfigManager {
public:
  ConfigManager(RelayController& relayCtrl);

  void begin();
  void saveSettings(int updateFrequency, bool useFahrenheit);
  void loadSettings(int& updateFrequency, bool& useFahrenheit);

private:
  RelayController& relayController;
};

#endif // CONFIG_MANAGER_H
