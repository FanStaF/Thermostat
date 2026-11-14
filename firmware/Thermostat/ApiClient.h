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
  // secondsSinceGoodReading < 0 means "never had a good reading".
  bool sendHeartbeat(bool sensorFault, long secondsSinceGoodReading);
  bool sendTemperatureReading(float temperature, int sensorId = 0);

  // ---- Reading outbox (flash-backed) ----
  // Queue a reading with its measurement time; uploaded in batches and
  // deleted once the server confirms. Survives outages and reboots.
  void enqueueReading(float temperature, int sensorId = 0);
  bool hasOutbox();
  // Upload up to OUTBOX_BATCH queued readings. Returns true on progress.
  bool sendOutboxBatch();

  // True when a server reply said commands are waiting (fetch them now).
  bool commandsWaiting() const { return pendingFlag; }

  // Reported in heartbeats (device health panel on the dashboard).
  void setBootCount(uint32_t n) { bootCount = n; }
  // True once the server has ever reported pending_commands (newer backend);
  // until then the firmware keeps polling for commands every 30s.
  bool serverSignalsCommands() const { return serverReportsPending; }
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
  long outboxSize;           // bytes, -1 = not yet known
  uint32_t bootId;           // tags clock-less readings from this boot
  bool pendingFlag;
  bool serverReportsPending;
  int lastHttpCode;
  uint32_t bootCount = 0;
  bool batchUnsupported;     // old backend: no /temperature/batch endpoint
  unsigned long batchUnsupportedSince;  // retry batch after a while (backend may be upgraded)
  bool sendNewestQueuedReadingLegacy();

  void notePendingCommands(const String& response);
  bool resolveOutboxLine(const String& line, uint32_t nowEpoch, long& epochOut,
                         float& tempOut, int& sensorOut, bool& waitForClock);

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
