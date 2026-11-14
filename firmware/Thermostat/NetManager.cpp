#include "NetManager.h"

NetManager net;

static const char* WIFI_FILE = "/wifi.json";
static const char* WIFI_PREV_FILE = "/wifi.prev.json";
static const char* WIFI_TMP_FILE = "/wifi.tmp";

void NetManager::begin() {
  // The SDK would otherwise write credentials to its own flash area on every
  // WiFi.begin() and auto-connect behind our back.
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(OTA_HOSTNAME);

  if (!loadFrom(WIFI_FILE)) {
    logger.addLog("WiFi: no saved networks, using built-in network only");
  }
  rebuildMulti();

  logger.addLog("Connecting to WiFi...");
  unsigned long start = millis();
  while (multi.run(5000) != WL_CONNECTED && millis() - start < 20000) {
    delay(100);
  }
  if (isConnected()) {
    logger.addLog("WiFi connected: " + WiFi.SSID() + " " + WiFi.localIP().toString());
    connectedSince = millis();
  } else {
    logger.addLog("ERROR: WiFi connection failed, will keep retrying");
    disconnectedSince = millis();
  }
}

bool NetManager::loadFrom(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logger.addLog(String("WiFi: ") + path + " unreadable, ignored");
    return false;
  }

  networkCount = 0;
  for (JsonObject n : doc["networks"].as<JsonArray>()) {
    if (networkCount >= WIFI_MAX_NETWORKS) break;
    String ssid = n["ssid"] | "";
    if (ssid.length() == 0) continue;
    networks[networkCount].ssid = ssid;
    networks[networkCount].password = n["password"] | "";
    networkCount++;
  }
  apSsid = doc["ap"]["ssid"] | WIFI_AP_DEFAULT_SSID;
  apPassword = doc["ap"]["password"] | WIFI_AP_DEFAULT_PASSWORD;
  return true;
}

bool NetManager::saveTo(const char* path) const {
  JsonDocument doc;
  JsonArray arr = doc["networks"].to<JsonArray>();
  for (int i = 0; i < networkCount; i++) {
    JsonObject n = arr.add<JsonObject>();
    n["ssid"] = networks[i].ssid;
    n["password"] = networks[i].password;
  }
  doc["ap"]["ssid"] = apSsid;
  doc["ap"]["password"] = apPassword;

  // Write-then-rename so a reset mid-save can't corrupt the settings.
  File f = LittleFS.open(WIFI_TMP_FILE, "w");
  if (!f) return false;
  size_t expected = measureJson(doc);
  bool ok = serializeJson(doc, f) == expected;
  f.close();
  if (ok) {
    LittleFS.remove(path);
    ok = LittleFS.rename(WIFI_TMP_FILE, path);
  }
  if (!ok) LittleFS.remove(WIFI_TMP_FILE);
  return ok;
}

void NetManager::rebuildMulti() {
  multi.cleanAPlist();
  for (int i = 0; i < networkCount; i++) {
    multi.addAP(networks[i].ssid.c_str(), networks[i].password.c_str());
  }
  // The compiled-in network is always a candidate, so editing the list can
  // never remove the network that is known to work.
  if (!multi.existsAP(WIFI_SSID, WIFI_PASSWORD)) {
    multi.addAP(WIFI_SSID, WIFI_PASSWORD);
  }
}

void NetManager::startAp(unsigned long durationMs) {
  if (!apActive) {
    WiFi.mode(WIFI_AP_STA);
    // WPA2 needs >= 8 characters; fall back to an open AP rather than none.
    bool ok = apPassword.length() >= 8
                ? WiFi.softAP(apSsid.c_str(), apPassword.c_str())
                : WiFi.softAP(apSsid.c_str());
    if (!ok) {
      logger.addLog("ERROR: Could not start access point");
      WiFi.mode(WIFI_STA);
      return;
    }
    apActive = true;
    logger.addLog("Access point '" + apSsid + "' started: http://" + WiFi.softAPIP().toString());
  }
  apStopAt = durationMs ? millis() + durationMs : 0;
}

void NetManager::stopAp() {
  if (!apActive) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive = false;
  apStopAt = 0;
  logger.addLog("Access point stopped");
}

void NetManager::startTestAp() {
  logger.addLog("Access point test requested");
  startAp(WIFI_AP_TEST_DURATION);
}

void NetManager::loop() {
  unsigned long now = millis();
  bool connected = isConnected();

  if (connected) {
    if (disconnectedSince) {
      logger.addLog("WiFi connected: " + WiFi.SSID() + " " + WiFi.localIP().toString());
      disconnectedSince = 0;
      connectedSince = now;
    }
    if (pendingConfirm) {
      // New settings proved themselves: drop the rollback copy.
      pendingConfirm = false;
      LittleFS.remove(WIFI_PREV_FILE);
      logger.addLog("WiFi settings confirmed");
    }
    // Fallback AP is no longer needed once we're back (give a client that is
    // using it a minute to finish). Test APs stop on their own timer.
    if (apActive && apStopAt == 0 && WiFi.softAPgetStationNum() == 0 &&
        now - connectedSince > 60000UL) {
      stopAp();
    }
  } else {
    if (!disconnectedSince) {
      disconnectedSince = now;
      logger.addLog("WiFi disconnected");
    }

    // Rollback: saved settings that never connected are reverted.
    if (pendingConfirm && now - pendingSince >= WIFI_CONFIRM_TIMEOUT) {
      pendingConfirm = false;
      if (loadFrom(WIFI_PREV_FILE)) {
        saveTo(WIFI_FILE);
        LittleFS.remove(WIFI_PREV_FILE);
        rebuildMulti();
        logger.addLog("WiFi: new settings did not connect, previous settings restored");
      }
    }

    // Last resort: our own access point, until a network comes back.
    if (!apActive && now - disconnectedSince >= WIFI_AP_FALLBACK_AFTER) {
      startAp(0);
    }

    // Retry known networks. A scan briefly disturbs the AP, so back off
    // while someone is connected to it (they're probably configuring us).
    unsigned long retryEvery = (apActive && WiFi.softAPgetStationNum() > 0)
                                 ? WIFI_RETRY_WITH_AP_CLIENT : WIFI_RECONNECT_INTERVAL;
    if (now - lastAttempt >= retryEvery) {
      lastAttempt = now;
      multi.run(5000);
    }
  }

  if (apActive && apStopAt && (long)(now - apStopAt) >= 0) {
    stopAp();
  }
}

void NetManager::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["networks"].to<JsonArray>();
  for (int i = 0; i < networkCount; i++) {
    arr.add<JsonObject>()["ssid"] = networks[i].ssid;
  }
  doc["maxNetworks"] = WIFI_MAX_NETWORKS;
  doc["builtinSsid"] = WIFI_SSID;
  doc["apSsid"] = apSsid;
  doc["connected"] = isConnected();
  doc["currentSsid"] = isConnected() ? WiFi.SSID() : "";
  doc["rssi"] = isConnected() ? WiFi.RSSI() : 0;
  doc["ip"] = isConnected() ? WiFi.localIP().toString() : "";
  doc["apActive"] = apActive;
  doc["apIp"] = apActive ? WiFi.softAPIP().toString() : "";
  doc["pendingConfirm"] = pendingConfirm;
}

String NetManager::applySettings(JsonDocument& doc) {
  JsonArray in = doc["networks"].as<JsonArray>();
  if (in.isNull()) return "networks missing";
  if ((int)in.size() > WIFI_MAX_NETWORKS) return "at most " + String(WIFI_MAX_NETWORKS) + " networks";

  Network next[WIFI_MAX_NETWORKS];
  int nextCount = 0;
  for (JsonObject n : in) {
    String ssid = n["ssid"] | "";
    ssid.trim();
    if (ssid.length() == 0) continue;
    if (ssid.length() > 32) return "network name too long: " + ssid;
    String pass = n["password"] | "";
    if (pass.length() == 0) {
      // Blank password = keep the stored one for this network (the page
      // never receives stored passwords).
      for (int i = 0; i < networkCount; i++) {
        if (networks[i].ssid == ssid) { pass = networks[i].password; break; }
      }
    }
    if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63)) {
      return "password for " + ssid + " must be 8-63 characters";
    }
    next[nextCount].ssid = ssid;
    next[nextCount].password = pass;
    nextCount++;
  }

  String nextApSsid = doc["apSsid"] | apSsid.c_str();
  nextApSsid.trim();
  if (nextApSsid.length() == 0 || nextApSsid.length() > 32) return "access point name must be 1-32 characters";
  String nextApPass = doc["apPassword"] | "";
  if (nextApPass.length() == 0) nextApPass = apPassword;  // blank = keep
  if (nextApPass.length() < 8 || nextApPass.length() > 63) return "access point password must be 8-63 characters";

  // Keep the current settings as the rollback copy, then save the new ones.
  bool hadSaved = LittleFS.exists(WIFI_FILE);
  if (hadSaved) {
    LittleFS.remove(WIFI_PREV_FILE);
    LittleFS.rename(WIFI_FILE, WIFI_PREV_FILE);
  } else {
    saveTo(WIFI_PREV_FILE);  // defaults, so rollback has something to restore
  }

  for (int i = 0; i < nextCount; i++) networks[i] = next[i];
  networkCount = nextCount;
  bool apChanged = nextApSsid != apSsid || nextApPass != apPassword;
  apSsid = nextApSsid;
  apPassword = nextApPass;

  if (!saveTo(WIFI_FILE)) return "could not save settings";
  rebuildMulti();
  logger.addLog("WiFi settings saved (" + String(networkCount) + " network(s))");

  // Rollback only matters if this save could cost us the connection: the
  // current network stays a candidate, so arm it only when we're offline.
  pendingConfirm = !isConnected();
  pendingSince = millis();
  if (!pendingConfirm) LittleFS.remove(WIFI_PREV_FILE);

  if (apActive && apChanged) {
    stopAp();          // restart with the new name/password
    startAp(WIFI_AP_TEST_DURATION);
  }
  return "";
}
