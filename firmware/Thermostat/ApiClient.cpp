#include "ApiClient.h"
#include "SystemLogger.h"
#include "Credentials.h"
#include <LittleFS.h>

extern SystemLogger logger;

ApiClient::ApiClient(const String& apiUrl) : apiUrl(apiUrl), deviceId(-1), authToken(""), pendingCommandCount(0) {
  // Detect if using HTTPS
  useHttps = apiUrl.startsWith("https://");
}

void ApiClient::begin() {
  // Configure HTTPS client if needed
  if (useHttps) {
    // Configure WiFiClientSecure to not verify SSL certificates
    wifiClientSecure.setInsecure();
    // Reduce buffer size to save memory (ESP8266 has limited RAM)
    wifiClientSecure.setBufferSizes(512, 512);
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

  http.setTimeout(15000);

  bool beginSuccess = useHttps ? http.begin(wifiClientSecure, url) : http.begin(wifiClient, url);
  if (!beginSuccess) {
    logger.addLog("Registration failed: HTTP connection error");
    return false;
  }

  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("X-API-Key", API_KEY);

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
    logger.addLog("Registration failed: HTTP " + String(httpCode));
    http.end();
  }

  return false;
}

bool ApiClient::sendHeartbeat() {
  if (deviceId <= 0) {
    return false;
  }

  JsonDocument doc;
  doc["ip_address"] = WiFi.localIP().toString();

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  String response;
  String endpoint = "/api/devices/" + String(deviceId) + "/heartbeat";

  return makePostRequest(endpoint, jsonPayload, response);
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
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("Authorization", "Bearer " + authToken);

  int httpCode = http.POST(jsonPayload);

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
  http.setTimeout(15000);
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
  http.setTimeout(15000);
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
