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
#include "ApiClient.h"

class WebInterface {
public:
  WebInterface(TemperatureManager& tempMgr, RelayController& relayCtrl,
               ConfigManager& cfgMgr, ApiClient& apiCli, int& updateFreq, bool& useFahr);

  void begin();
  void handleClient();

private:
  ESP8266WebServer server;
  TemperatureManager& tempManager;
  RelayController& relayController;
  ConfigManager& configManager;
  ApiClient& apiClient;
  int& updateFrequency;
  bool& useFahrenheit;

  // Handler functions
  void handleRoot();
  void handleStatus();
  void handleSetMode();
  void handleSetType();
  void handleSetThresholds();
  void handleSetFrequency();
  void handleClearData();
  void handleSetUnit();
  void handleLogs();
  void handleLogsJson();
  void handleData();

};

#endif // WEB_INTERFACE_H
