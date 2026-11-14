#ifndef NET_MANAGER_H
#define NET_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "SystemLogger.h"

// WiFi with layered fallbacks so the controller stays reachable without USB:
//  1. Up to WIFI_MAX_NETWORKS saved networks (/wifi.json), joined by best
//     signal, plus the compiled-in WIFI_SSID as a permanent last resort.
//  2. Settings are edited on the local web page; a save that leaves the
//     device unable to connect is rolled back after WIFI_CONFIRM_TIMEOUT.
//  3. After WIFI_AP_FALLBACK_AFTER without any connection, the device
//     starts its own access point (configurable name/password) at
//     192.168.4.1 while it keeps retrying the saved networks.
// None of this touches the control loop: relays keep running regardless.
class NetManager {
public:
  struct Network {
    String ssid;
    String password;
  };

  void begin();       // load settings, connect (blocks up to ~20s at boot)
  void loop();        // call every loop(): reconnects, AP fallback, rollback

  bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
  bool isApActive() const { return apActive; }

  // Settings page support.
  void toJson(JsonDocument& doc) const;  // no passwords
  // Validates and saves new settings. An empty password keeps the existing
  // one for that SSID. Returns an error message, or "" on success.
  String applySettings(JsonDocument& doc);
  void startTestAp();  // start the AP for WIFI_AP_TEST_DURATION

  // True if the current web request arrived through our own access point
  // (used to redirect phones' captive-portal checks to the settings page).
  static bool requestViaAp(const IPAddress& localIp) {
    return localIp == WiFi.softAPIP();
  }

private:
  Network networks[WIFI_MAX_NETWORKS];
  int networkCount = 0;
  String apSsid = WIFI_AP_DEFAULT_SSID;
  String apPassword = WIFI_AP_DEFAULT_PASSWORD;

  ESP8266WiFiMulti multi;
  // Captive portal: while the AP runs, every DNS name resolves to us, so
  // phones show the settings page on join and stay on the AP.
  DNSServer dns;
  bool apActive = false;
  unsigned long apStopAt = 0;           // 0 = no automatic stop (fallback AP)
  unsigned long disconnectedSince = 0;  // 0 = currently connected
  unsigned long lastAttempt = 0;
  unsigned long connectedSince = 0;
  bool pendingConfirm = false;          // saved settings not yet proven
  unsigned long pendingSince = 0;

  bool loadFrom(const char* path);
  bool saveTo(const char* path) const;
  void rebuildMulti();
  void startAp(unsigned long durationMs);
  void stopAp();
};

extern NetManager net;

#endif // NET_MANAGER_H
