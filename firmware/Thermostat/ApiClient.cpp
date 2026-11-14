#include "ApiClient.h"
#include "Config.h"
#include "SystemLogger.h"
#include "Credentials.h"
#include <LittleFS.h>
#include <time.h>
#include "FileUtil.h"

extern SystemLogger logger;

ApiClient::ApiClient(const String& apiUrl)
  : apiUrl(apiUrl), deviceId(-1), authToken(""),
    pendingCommandCount(0), nextCommandIdx(0),
    outboxSize(-1), bootId(0), pendingFlag(false), serverReportsPending(false),
    lastHttpCode(0), batchUnsupported(false), batchUnsupportedSince(0) {
  useHttps = apiUrl.startsWith("https://");
  for (int i = 0; i < 4; i++) {
    relayDirty[i] = false;
    lastSent[i].valid = false;
  }
}

void ApiClient::begin() {
  bootId = ESP.random();
  if (useHttps) {
    wifiClientSecure.setInsecure();
    wifiClientSecure.setBufferSizes(TLS_RX_BUFFER, TLS_TX_BUFFER);
  }

  loadToken();
}

void ApiClient::loadToken() {
  if (!LittleFS.begin()) {
    logger.addLog("Failed to mount LittleFS for token");
    return;
  }

  File file = LittleFS.open("/api_token.txt", "r");
  if (file) {
    authToken = file.readString();
    authToken.trim();
    file.close();
  }
}

void ApiClient::saveToken(const String& token) {
  if (!LittleFS.begin()) {
    logger.addLog("ERROR: Failed to mount LittleFS");
    return;
  }

  File file = LittleFS.open("/api_token.txt", "w");
  if (file) {
    file.print(token);
    file.close();
    authToken = token;
  } else {
    logger.addLog("ERROR: Failed to save auth token");
  }
}

bool ApiClient::registerDevice(const String& hostname, const String& macAddress, const String& ipAddress, const String& firmwareVersion) {
  JsonDocument doc;
  doc["hostname"] = hostname;
  doc["mac_address"] = macAddress;
  doc["ip_address"] = ipAddress;
  doc["firmware_version"] = firmwareVersion;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  if (WiFi.status() != WL_CONNECTED) {
    logger.addLog("Registration failed: WiFi not connected");
    return false;
  }

  HTTPClient http;
  String url = apiUrl + "/api/devices/register";

  http.setTimeout(HTTP_REQUEST_TIMEOUT);

  bool beginSuccess = useHttps ? http.begin(wifiClientSecure, url) : http.begin(wifiClient, url);
  if (!beginSuccess) {
    logger.addLog("Registration failed: HTTP connection error");
    return false;
  }

  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("X-API-Key", API_KEY);
  // An already-registered device must prove it holds its current token to
  // re-register (the backend refuses re-registration by MAC alone).
  if (authToken.length() > 0) {
    http.addHeader("Authorization", "Bearer " + authToken);
  }

  int httpCode = http.POST(jsonPayload);

  String response = "";
  if (httpCode > 0) {
    response = http.getString();
  }

  if (httpCode > 0 && (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED)) {
    http.end();

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (!error && responseDoc.containsKey("device_id") && responseDoc.containsKey("token")) {
      deviceId = responseDoc["device_id"];
      String token = responseDoc["token"].as<String>();
      saveToken(token);
      logger.addLog("Registered as device ID: " + String(deviceId));
      return true;
    } else {
      logger.addLog("Registration failed: Invalid response");
    }
  } else {
    if (httpCode == 403 || httpCode == 409) {
      logger.addLog("Registration refused (HTTP " + String(httpCode) +
                    "): ask an admin to allow re-registration on the dashboard");
    } else {
      logger.addLog("Registration failed: HTTP " + String(httpCode));
    }
    http.end();
  }

  return false;
}

bool ApiClient::sendHeartbeat(bool sensorFault, long secondsSinceGoodReading) {
  if (deviceId <= 0) {
    return false;
  }

  JsonDocument doc;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["sensor_fault"] = sensorFault;
  if (secondsSinceGoodReading >= 0) doc["seconds_since_good_reading"] = secondsSinceGoodReading;
  else doc["seconds_since_good_reading"] = nullptr;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  String response;
  String endpoint = "/api/devices/" + String(deviceId) + "/heartbeat";

  bool ok = makePostRequest(endpoint, jsonPayload, response);
  if (ok) notePendingCommands(response);
  return ok;
}

bool ApiClient::sendTemperatureReading(float temperature, int sensorId) {
  if (deviceId <= 0) {
    return false;
  }

  JsonDocument doc;
  doc["temperature"] = temperature;
  doc["sensor_id"] = sensorId;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  String response;
  String endpoint = "/api/devices/" + String(deviceId) + "/temperature";

  return makePostRequest(endpoint, jsonPayload, response);
}

bool ApiClient::sendRelayState(int relayNumber, bool state, const String& mode, float tempOn, float tempOff, const String& name) {
  if (deviceId <= 0) {
    return false;
  }

  JsonDocument doc;
  doc["relay_number"] = relayNumber;
  doc["state"] = state;
  doc["mode"] = mode;
  doc["temp_on"] = tempOn;
  doc["temp_off"] = tempOff;
  if (name.length() > 0) {
    doc["name"] = name;
  }

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  String response;
  String endpoint = "/api/devices/" + String(deviceId) + "/relay-state";

  if (makePostRequest(endpoint, jsonPayload, response)) {
    return true;
  }

  return false;
}

bool ApiClient::pollCommands() {
  if (deviceId <= 0) {
    return false;
  }

  String response;
  String endpoint = "/api/devices/" + String(deviceId) + "/commands/pending";

  if (makeGetRequest(endpoint, response)) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (!error && doc.containsKey("commands")) {
      JsonArray commands = doc["commands"].as<JsonArray>();
      pendingCommandCount = 0;
      nextCommandIdx = 0;

      for (JsonObject cmd : commands) {
        if (pendingCommandCount >= MAX_PENDING_COMMANDS) break;

        pendingCommands[pendingCommandCount].id = cmd["id"];
        pendingCommands[pendingCommandCount].type = cmd["type"].as<String>();

        String paramsStr;
        serializeJson(cmd["params"], paramsStr);
        pendingCommands[pendingCommandCount].params = paramsStr;
        pendingCommands[pendingCommandCount].isValid = true;

        pendingCommandCount++;
      }

      pendingFlag = false;
      if (pendingCommandCount > 0) {
        logger.addLog("Received " + String(pendingCommandCount) + " pending commands");
      }

      return true;
    }
  }

  return false;
}

bool ApiClient::updateCommandStatus(int commandId, const String& status, const String& result) {
  if (deviceId <= 0) {
    return false;
  }

  JsonDocument doc;
  doc["status"] = status;
  if (result.length() > 0) {
    doc["result"]["message"] = result;
  }

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  String response;
  String endpoint = "/api/devices/" + String(deviceId) + "/commands/" + String(commandId);

  if (makePutRequest(endpoint, jsonPayload, response)) {
    logger.addLog("Command " + String(commandId) + " status updated to: " + status);
    return true;
  }

  return false;
}

bool ApiClient::makePostRequest(const String& endpoint, const String& jsonPayload, String& response) {
  if (WiFi.status() != WL_CONNECTED) {
    logger.addLog("WiFi not connected");
    return false;
  }

  if (authToken.length() == 0) {
    logger.addLog("ERROR: No auth token for request!");
    return false;
  }

  HTTPClient http;
  String url = apiUrl + endpoint;

  // Use appropriate client based on protocol
  if (useHttps) {
    http.begin(wifiClientSecure, url);
  } else {
    http.begin(wifiClient, url);
  }

  // Set timeout and follow redirects
  http.setTimeout(HTTP_REQUEST_TIMEOUT);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("Authorization", "Bearer " + authToken);

  int httpCode = http.POST(jsonPayload);
  lastHttpCode = httpCode;

  // Combined log: endpoint and response code
  logger.addLog("POST " + endpoint + " : " + String(httpCode));

  if (httpCode == -5) {
    logger.addLog("ERROR: Connection timeout");
  }

  if (httpCode > 0) {
    response = http.getString();
    http.end();
    return (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);
  }

  http.end();
  return false;
}

bool ApiClient::makeGetRequest(const String& endpoint, String& response) {
  if (WiFi.status() != WL_CONNECTED || authToken.length() == 0) {
    return false;
  }

  HTTPClient http;
  String url = apiUrl + endpoint;

  // Use appropriate client based on protocol
  if (useHttps) {
    http.begin(wifiClientSecure, url);
  } else {
    http.begin(wifiClient, url);
  }

  // Set timeout and follow redirects
  http.setTimeout(HTTP_REQUEST_TIMEOUT);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  http.addHeader("Accept", "application/json");
  http.addHeader("Authorization", "Bearer " + authToken);

  int httpCode = http.GET();

  // Only log errors, not successful polling
  if (httpCode <= 0 || httpCode >= 400) {
    logger.addLog("GET " + endpoint + " : " + String(httpCode));
  }

  if (httpCode > 0) {
    response = http.getString();
    http.end();
    return (httpCode == HTTP_CODE_OK);
  }

  http.end();
  return false;
}

bool ApiClient::makePutRequest(const String& endpoint, const String& jsonPayload, String& response) {
  if (WiFi.status() != WL_CONNECTED || authToken.length() == 0) {
    return false;
  }

  HTTPClient http;
  String url = apiUrl + endpoint;

  // Use appropriate client based on protocol
  if (useHttps) {
    http.begin(wifiClientSecure, url);
  } else {
    http.begin(wifiClient, url);
  }

  // Set timeout and follow redirects
  http.setTimeout(HTTP_REQUEST_TIMEOUT);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("Authorization", "Bearer " + authToken);

  int httpCode = http.PUT(jsonPayload);

  // Combined log: endpoint and response code
  logger.addLog("PUT " + endpoint + " : " + String(httpCode));

  if (httpCode > 0) {
    response = http.getString();
    http.end();
    return (httpCode == HTTP_CODE_OK);
  }

  http.end();
  return false;
}

// ---- Dirty-flag drain interface ----

void ApiClient::markRelayDirty(int relayIdx) {
  if (relayIdx >= 0 && relayIdx < 4) relayDirty[relayIdx] = true;
}

void ApiClient::clearRelayDirty(int relayIdx) {
  if (relayIdx >= 0 && relayIdx < 4) relayDirty[relayIdx] = false;
}

int ApiClient::nextDirtyRelay() const {
  for (int i = 0; i < 4; i++) {
    if (relayDirty[i]) return i;
  }
  return -1;
}

void ApiClient::notePendingCommands(const String& response) {
  JsonDocument doc;
  if (deserializeJson(doc, response)) return;
  if (!doc["pending_commands"].is<int>()) return;
  serverReportsPending = true;
  pendingFlag = doc["pending_commands"].as<int>() > 0;
}

// ---- Outbox ----
// Line formats:
//   e<epoch>,<temp>,<sensor>          clock was synced when measured
//   m<bootId>:<upsec>,<temp>,<sensor> measured before NTP sync this boot

void ApiClient::enqueueReading(float temperature, int sensorId) {
  if (outboxSize < 0) {
    File f = LittleFS.open(OUTBOX_FILE, "r");
    outboxSize = f ? (long)f.size() : 0;
    if (f) f.close();
  }
  if ((size_t)outboxSize > OUTBOX_MAX_BYTES) {
    // Long outage: keep the newest readings, drop the oldest.
    File f = LittleFS.open(OUTBOX_FILE, "r");
    size_t size = f ? f.size() : 0;
    if (f) f.close();
    if (size > OUTBOX_KEEP_BYTES) {
      FileUtil::keepTail(OUTBOX_FILE, OUTBOX_TMP_FILE, size - OUTBOX_KEEP_BYTES, true);
      logger.addLog("WARN: Outbox full, oldest unsent readings dropped");
    }
    outboxSize = -1;
  }

  char line[48];
  time_t now = time(nullptr);
  if (now >= 1000000000) {
    snprintf(line, sizeof(line), "e%lu,%.2f,%d\n", (unsigned long)now, temperature, sensorId);
  } else {
    snprintf(line, sizeof(line), "m%lu:%lu,%.2f,%d\n", (unsigned long)bootId,
             (unsigned long)(millis() / 1000), temperature, sensorId);
  }

  File f = LittleFS.open(OUTBOX_FILE, "a");
  if (!f) return;
  size_t n = f.print(line);
  f.close();
  if (outboxSize >= 0) outboxSize += n;
}

bool ApiClient::hasOutbox() {
  if (outboxSize < 0) {
    File f = LittleFS.open(OUTBOX_FILE, "r");
    outboxSize = f ? (long)f.size() : 0;
    if (f) f.close();
  }
  return outboxSize > 0;
}

bool ApiClient::resolveOutboxLine(const String& line, uint32_t nowEpoch, long& epochOut,
                                  float& tempOut, int& sensorOut, bool& waitForClock) {
  waitForClock = false;
  int c1 = line.indexOf(',');
  int c2 = line.indexOf(',', c1 + 1);
  if (line.length() < 4 || c1 < 0 || c2 < 0) return false;
  tempOut = line.substring(c1 + 1, c2).toFloat();
  sensorOut = line.substring(c2 + 1).toInt();

  if (line[0] == 'e') {
    epochOut = line.substring(1, c1).toInt();
    return true;
  }
  if (line[0] == 'm') {
    int colon = line.indexOf(':');
    if (colon < 0 || colon > c1) return false;
    uint32_t id = strtoul(line.substring(1, colon).c_str(), nullptr, 10);
    unsigned long upsec = strtoul(line.substring(colon + 1, c1).c_str(), nullptr, 10);
    if (id != bootId) {
      epochOut = 0;  // previous boot: real time unknown, server uses receipt time
      return true;
    }
    if (nowEpoch == 0) {
      waitForClock = true;  // this boot, clock not synced yet: wait for NTP
      return false;
    }
    epochOut = (long)nowEpoch - (long)(millis() / 1000 - upsec);
    return true;
  }
  return false;
}

// Older backend: upload only the newest queued reading (no timestamps
// supported), then clear the queue.
bool ApiClient::sendNewestQueuedReadingLegacy() {
  File f = LittleFS.open(OUTBOX_FILE, "r");
  if (!f) { outboxSize = 0; return false; }
  String last;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) last = line;
  }
  f.close();
  int c1 = last.indexOf(',');
  int c2 = last.indexOf(',', c1 + 1);
  bool ok = true;
  if (c1 > 0 && c2 > c1) {
    ok = sendTemperatureReading(last.substring(c1 + 1, c2).toFloat(), last.substring(c2 + 1).toInt());
  }
  if (ok) {
    LittleFS.remove(OUTBOX_FILE);
    outboxSize = 0;
  }
  return ok;
}

bool ApiClient::sendOutboxBatch() {
  if (deviceId <= 0 || !hasOutbox()) return false;
  if (batchUnsupported && millis() - batchUnsupportedSince >= 600000UL) {
    batchUnsupported = false;  // re-check every 10 min in case the backend was upgraded
  }
  if (batchUnsupported) return sendNewestQueuedReadingLegacy();

  File f = LittleFS.open(OUTBOX_FILE, "r");
  if (!f) { outboxSize = 0; return false; }

  time_t now = time(nullptr);
  uint32_t nowEpoch = now >= 1000000000 ? (uint32_t)now : 0;

  JsonDocument doc;
  JsonArray readings = doc["readings"].to<JsonArray>();
  size_t consumed = 0;
  int count = 0;

  while (f.available() && count < OUTBOX_BATCH) {
    String line = f.readStringUntil('\n');
    line.trim();
    long epoch; float temp; int sensor; bool wait;
    if (line.length() > 0 && !resolveOutboxLine(line, nowEpoch, epoch, temp, sensor, wait)) {
      if (wait) break;  // keep this and later lines until the clock syncs
      consumed = f.position();  // malformed line: drop it
      continue;
    }
    consumed = f.position();
    if (line.length() == 0) continue;
    JsonObject r = readings.add<JsonObject>();
    r["temperature"] = temp;
    r["sensor_id"] = sensor;
    if (epoch > 0) r["recorded_at"] = epoch;
    count++;
  }
  f.close();

  if (count == 0) {
    if (consumed > 0) FileUtil::keepTail(OUTBOX_FILE, OUTBOX_TMP_FILE, consumed, false);
    outboxSize = -1;
    return consumed > 0;
  }

  String payload;
  serializeJson(doc, payload);
  String response;
  String endpoint = "/api/devices/" + String(deviceId) + "/temperature/batch";
  bool ok = makePostRequest(endpoint, payload, response);

  if (!ok && lastHttpCode == 404) {
    // Older backend without the batch endpoint: fall back to single readings.
    logger.addLog("Server has no batch endpoint, using single-reading uploads");
    batchUnsupported = true;
    batchUnsupportedSince = millis();
    return sendNewestQueuedReadingLegacy();
  } else if (!ok && lastHttpCode == 422) {
    logger.addLog("WARN: Server rejected reading batch, dropping it");
    ok = true;  // don't retry bad data forever
  }

  if (!ok) return false;  // keep everything; retried next loop

  notePendingCommands(response);
  // Server confirmed: remove the sent lines (purge-after-upload).
  FileUtil::keepTail(OUTBOX_FILE, OUTBOX_TMP_FILE, consumed, false);
  outboxSize = -1;
  return true;
}

ApiClient::Command* ApiClient::peekNextCommand() {
  if (nextCommandIdx < pendingCommandCount) {
    return &pendingCommands[nextCommandIdx];
  }
  return nullptr;
}

void ApiClient::popNextCommand() {
  if (nextCommandIdx < pendingCommandCount) {
    nextCommandIdx++;
  }
  // Reset to clean state when fully drained so the next pollCommands
  // starts from a known-empty queue.
  if (nextCommandIdx >= pendingCommandCount) {
    nextCommandIdx = 0;
    pendingCommandCount = 0;
  }
}

bool ApiClient::relayStateMatchesLastSent(int relayIdx, bool state, const String& mode, float tempOn, float tempOff) const {
  if (relayIdx < 0 || relayIdx >= 4) return false;
  const RelaySnapshot& s = lastSent[relayIdx];
  if (!s.valid) return false;
  return s.state == state
      && s.mode == mode
      && fabs(s.tempOn  - tempOn)  < 0.01f
      && fabs(s.tempOff - tempOff) < 0.01f;
}

void ApiClient::recordRelaySent(int relayIdx, bool state, const String& mode, float tempOn, float tempOff) {
  if (relayIdx < 0 || relayIdx >= 4) return;
  lastSent[relayIdx].valid   = true;
  lastSent[relayIdx].state   = state;
  lastSent[relayIdx].mode    = mode;
  lastSent[relayIdx].tempOn  = tempOn;
  lastSent[relayIdx].tempOff = tempOff;
}
