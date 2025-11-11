#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include "Config.h"
#include "SystemLogger.h"
#include "TemperatureManager.h"
#include "RelayController.h"
#include "ConfigManager.h"

class WebInterface {
public:
  WebInterface(TemperatureManager& tempMgr, RelayController& relayCtrl,
               ConfigManager& cfgMgr, int& updateFreq, bool& useFahr);

  void begin();
  void handleClient();

private:
  ESP8266WebServer server;
  TemperatureManager& tempManager;
  RelayController& relayController;
  ConfigManager& configManager;
  int& updateFrequency;
  bool& useFahrenheit;

  // Handler functions
  void handleRoot();
  void handleStatus();
  void handleSetMode();
  void handleSetThresholds();
  void handleSetFrequency();
  void handleClearData();
  void handleSetUnit();
  void handleLogs();
  void handleData();

  // HTML generation
  String generateHTML();
};

#endif // WEB_INTERFACE_H
