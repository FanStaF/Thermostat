#include "ApiClient.h"
#include "SystemLogger.h"
#include "Credentials.h"
#include <LittleFS.h>

extern SystemLogger logger;

ApiClient::ApiClient(const String& apiUrl) : apiUrl(apiUrl), deviceId(-1), authToken(""), pendingCommandCount(0) {}

void ApiClient::begin() {
  loadToken();
  logger.addLog("API Client initialized");
  if (authToken.length() > 0) {
    logger.addLog("Auth token loaded");
  }
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
    logger.addLog("Failed to mount LittleFS for token save");
    return;
  }

  File file = LittleFS.open("/api_token.txt", "w");
  if (file) {
    file.print(token);
    file.close();
    authToken = token;
    logger.addLog("Auth token saved");
  } else {
    logger.addLog("Failed to save token");
  }
}

bool ApiClient::registerDevice(const String& hostname, const String& macAddress, const String& ipAddress, const String& firmwareVersion) {
  logger.addLog("Attempting registration to: " + String(apiUrl));

  JsonDocument doc;
  doc["hostname"] = hostname;
  doc["mac_address"] = macAddress;
  doc["ip_address"] = ipAddress;
  doc["firmware_version"] = firmwareVersion;

  String jsonPayload;
  serializeJson(doc, jsonPayload);
  logger.addLog("Payload: " + jsonPayload);

  // Special handling for registration - use API key instead of auth token
  if (WiFi.status() != WL_CONNECTED) {
    logger.addLog("WiFi not connected");
    return false;
  }

  HTTPClient http;
  String url = apiUrl + "/api/devices/register";
  logger.addLog("POST to: " + url);

  http.begin(wifiClient, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("X-API-Key", API_KEY);

  int httpCode = http.POST(jsonPayload);
  logger.addLog("HTTP Code: " + String(httpCode));

  if (httpCode > 0 && (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED)) {
    String response = http.getString();
    http.end();

    logger.addLog("Got response: " + response);
    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (!error && responseDoc.containsKey("device_id") && responseDoc.containsKey("token")) {
      deviceId = responseDoc["device_id"];
      String token = responseDoc["token"].as<String>();
      saveToken(token);
      logger.addLog("Device registered with ID: " + String(deviceId));
      return true;
    } else {
      logger.addLog("JSON parse error or missing device_id/token");
    }
  } else {
    logger.addLog("HTTP request error: " + String(httpCode));
    http.end();
  }

  logger.addLog("Device registration failed");
  return false;
}

bool ApiClient::sendHeartbeat() {
  if (deviceId <= 0) {
    logger.addLog("Cannot send heartbeat: device not registered");
    return false;
  }

  JsonDocument doc;
  doc["ip_address"] = WiFi.localIP().toString();

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  String response;
  String endpoint = "/api/devices/" + String(deviceId) + "/heartbeat";

  if (makePostRequest(endpoint, jsonPayload, response)) {
    logger.addLog("Heartbeat sent successfully");
    return true;
  }

  logger.addLog("Heartbeat failed");
  return false;
}

bool ApiClient::sendTemperatureReading(float temperature, int sensorId) {
  if (deviceId <= 0) {
    logger.addLog("Cannot send temperature: device not registered");
    return false;
  }

  JsonDocument doc;
  doc["temperature"] = temperature;
  doc["sensor_id"] = sensorId;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  String response;
  String endpoint = "/api/devices/" + String(deviceId) + "/temperature";

  if (makePostRequest(endpoint, jsonPayload, response)) {
    return true;
  }

  return false;
}

bool ApiClient::sendRelayState(int relayNumber, bool state, const String& mode, float tempOn, float tempOff, const String& name) {
  if (deviceId <= 0) {
    logger.addLog("Cannot send relay state: device not registered");
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

  HTTPClient http;
  String url = apiUrl + endpoint;
  logger.addLog("POST to: " + url);

  http.begin(wifiClient, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  // Add Bearer token for authenticated requests
  if (authToken.length() > 0) {
    http.addHeader("Authorization", "Bearer " + authToken);
  }

  int httpCode = http.POST(jsonPayload);
  logger.addLog("HTTP Code: " + String(httpCode));

  if (httpCode > 0) {
    response = http.getString();
    http.end();
    return (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);
  }

  logger.addLog("HTTP request error: " + String(httpCode));
  http.end();
  return false;
}

bool ApiClient::makeGetRequest(const String& endpoint, String& response) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  String url = apiUrl + endpoint;

  http.begin(wifiClient, url);
  http.addHeader("Accept", "application/json");

  // Add Bearer token for authenticated requests
  if (authToken.length() > 0) {
    http.addHeader("Authorization", "Bearer " + authToken);
  }

  int httpCode = http.GET();

  if (httpCode > 0) {
    response = http.getString();
    http.end();
    return (httpCode == HTTP_CODE_OK);
  }

  http.end();
  return false;
}

bool ApiClient::makePutRequest(const String& endpoint, const String& jsonPayload, String& response) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  String url = apiUrl + endpoint;

  http.begin(wifiClient, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  // Add Bearer token for authenticated requests
  if (authToken.length() > 0) {
    http.addHeader("Authorization", "Bearer " + authToken);
  }

  int httpCode = http.PUT(jsonPayload);

  if (httpCode > 0) {
    response = http.getString();
    http.end();
    return (httpCode == HTTP_CODE_OK);
  }

  http.end();
  return false;
}
