#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
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

  // Drain-one-per-loop interface: web handlers and the loop tick mark state
  // dirty; the main loop drains a single sync per iteration so the web server
  // is never blocked behind a stack of HTTP calls.
  void markRelayDirty(int relayIdx);
  void clearRelayDirty(int relayIdx);
  int  nextDirtyRelay() const;             // 0..3 of next dirty relay, or -1
  void markTempDirty();
  void clearTempDirty();
  bool isTempDirty() const { return tempDirty; }

  bool hasPendingCommands() const { return nextCommandIdx < pendingCommandCount; }
  Command* peekNextCommand();
  void popNextCommand();

  // Last-sent dedupe: avoids re-POSTing identical relay state. The drain
  // logic asks whether the candidate matches what was last successfully sent
  // and skips the network call if so. recordRelaySent() is called only after
  // a 2xx response from the backend.
  bool relayStateMatchesLastSent(int relayIdx, bool state, const String& mode, float tempOn, float tempOff) const;
  void recordRelaySent(int relayIdx, bool state, const String& mode, float tempOn, float tempOff);

private:
  String apiUrl;
  int deviceId;
  String authToken;
  WiFiClient wifiClient;
  WiFiClientSecure wifiClientSecure;
  bool useHttps;

  static const int MAX_PENDING_COMMANDS = 10;
  Command pendingCommands[MAX_PENDING_COMMANDS];
  int pendingCommandCount;
  int nextCommandIdx;

  bool relayDirty[4];
  bool tempDirty;

  struct RelaySnapshot {
    bool valid;
    bool state;
    String mode;
    float tempOn;
    float tempOff;
  };
  RelaySnapshot lastSent[4];

  bool makePostRequest(const String& endpoint, const String& jsonPayload, String& response);
  bool makeGetRequest(const String& endpoint, String& response);
  bool makePutRequest(const String& endpoint, const String& jsonPayload, String& response);

  void loadToken();
  void saveToken(const String& token);
};

#endif
