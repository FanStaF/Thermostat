#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

class ApiClient {
public:
  ApiClient(const String& apiUrl);

  void begin();
  bool registerDevice(const String& hostname, const String& macAddress, const String& ipAddress, const String& firmwareVersion);
  bool sendHeartbeat();
  bool sendTemperatureReading(float temperature, int sensorId = 0);
  bool sendRelayState(int relayNumber, bool state, const String& mode, float tempOn, float tempOff, const String& name = "");
  bool pollCommands();
  bool updateCommandStatus(int commandId, const String& status, const String& result = "");

  int getDeviceId() const { return deviceId; }
  bool isRegistered() const { return deviceId > 0; }

  struct Command {
    int id;
    String type;
    String params;
    bool isValid;
  };

  Command* getPendingCommands() { return pendingCommands; }
  int getPendingCommandCount() const { return pendingCommandCount; }

private:
  String apiUrl;
  int deviceId;
  WiFiClient wifiClient;

  static const int MAX_PENDING_COMMANDS = 10;
  Command pendingCommands[MAX_PENDING_COMMANDS];
  int pendingCommandCount;

  bool makePostRequest(const String& endpoint, const String& jsonPayload, String& response);
  bool makeGetRequest(const String& endpoint, String& response);
  bool makePutRequest(const String& endpoint, const String& jsonPayload, String& response);
};

#endif
