#ifndef CONFIG_H
#define CONFIG_H

#include "Credentials.h"

// ---- Pin Definitions ----
#define ONE_WIRE_BUS D2
static const int RELAY_PINS[4] = { D1, D5, D6, D7 };

// ---- System Configuration ----
constexpr int DEFAULT_UPDATE_FREQUENCY = 5; // seconds
constexpr int MIN_UPDATE_FREQUENCY = 5;     // seconds — one validated read takes ~2.3s
constexpr int MAX_UPDATE_FREQUENCY = 300;   // seconds
constexpr size_t MAX_SYSTEM_LOGS = 120;     // fixed ring buffer; keep small, heap is ~45KB
constexpr size_t MAX_LOG_MESSAGE_LEN = 160; // longer messages are truncated

// ---- API Configuration ----
// API_URL is defined in Credentials.h
#define FIRMWARE_VERSION "2.6.1"
constexpr int API_HEARTBEAT_INTERVAL = 30000;  // ms — replies say whether commands are waiting
constexpr int API_COMMAND_POLL_INTERVAL = 30000;          // ms, used until the server reports pending_commands
constexpr int API_COMMAND_POLL_FALLBACK = 300000;         // ms, safety poll once it does (5 min)
constexpr int HTTP_REQUEST_TIMEOUT = 5000;     // ms — kept short so blocking calls don't starve the web server
constexpr int TLS_RX_BUFFER = 1024;            // bytes — TLS fragment input buffer
constexpr int TLS_TX_BUFFER = 4096;            // bytes — TLS output buffer (handshake needs >2K)
constexpr unsigned long WIFI_RECONNECT_INTERVAL = 30000; // ms between reconnect attempts
constexpr unsigned long HEAP_LOG_INTERVAL = 300000;      // ms (5 min) periodic heap snapshot
constexpr unsigned long REGISTER_RETRY_INTERVAL = 60000; // ms between backend registration retries

// ---- File Paths ----
#define CONFIG_FILE "/config.json"
#define CONFIG_TMP_FILE "/config.tmp"
#define LOG_FILE "/temp_log.csv"
#define LOG_TMP_FILE "/temp_log.tmp"
// Outbox: readings not yet confirmed by the server. Lines are deleted once
// the server accepts them; capped so a long outage can't fill the flash.
#define OUTBOX_FILE "/outbox.csv"
#define OUTBOX_TMP_FILE "/outbox.tmp"
constexpr size_t OUTBOX_MAX_BYTES = 32 * 1024;   // ~1500 readings (~25h at 60s)
constexpr size_t OUTBOX_KEEP_BYTES = 24 * 1024;  // oldest dropped beyond the cap
constexpr int OUTBOX_BATCH = 30;                  // readings per upload request
constexpr size_t LOG_FILE_MAX_BYTES = 64 * 1024;  // trim temp log when it grows past this
constexpr size_t LOG_FILE_KEEP_BYTES = 16 * 1024; // ...keeping roughly this much of the newest data
constexpr size_t FS_MIN_FREE_BYTES = 32 * 1024;   // stop appending to the log below this much free space
constexpr int FS_INFO_CHECK_EVERY = 50;           // re-check free space every N log appends

// ---- OTA Configuration ----
#define OTA_HOSTNAME "thermostat"

// ---- Temperature Sensor Configuration ----
constexpr int TEMP_NUM_SAMPLES = 3;
constexpr float TEMP_MAX_DIFF = 2.0;        // Max difference between samples in °C
constexpr float TEMP_MIN_VALID = -50.0;     // DS18B20 can read -55°C but let's be safe
constexpr float TEMP_MAX_VALID = 85.0;      // exclusive: 85.0 is the DS18B20 power-on-reset value
constexpr float TEMP_POWER_ON_VALUE = 85.0; // a reading of exactly this means a reset/failed conversion
constexpr int TEMP_RETRY_DELAY = 200;       // ms before retry
// Fail-safe: AUTO relays go OFF once the sensor has failed at least
// SENSOR_FAILSAFE_MIN_FAILS times in a row AND no good reading has arrived for
// SENSOR_FAILSAFE_MS. While failing, reads are retried every
// MIN_UPDATE_FREQUENCY seconds regardless of the configured interval.
constexpr unsigned long SENSOR_FAILSAFE_MS = 60000;
constexpr int SENSOR_FAILSAFE_MIN_FAILS = 3;

// ---- Relay protection ----
// AUTO switching never flips a relay sooner than this after its last change,
// so a setpoint sitting right at room temperature can't make it chatter.
// Manual modes and the sensor fail-safe (forcing OFF) act immediately.
constexpr unsigned long RELAY_MIN_ON_MS = 120000;   // 2 min
constexpr unsigned long RELAY_MIN_OFF_MS = 120000;  // 2 min

// ---- Threshold limits (°C) ----
constexpr float THRESHOLD_MIN = -40.0;
constexpr float THRESHOLD_MAX = 80.0;

// ---- Web Server Configuration ----
constexpr int WEB_SERVER_PORT = 80;
constexpr int MAX_DATA_LINES = 500;         // Maximum lines to send in data endpoint
constexpr int MIN_DATA_LINES = 100;         // Minimum lines to send
constexpr int BYTES_PER_LINE_ESTIMATE = 20; // For file seeking calculation

// ---- WiFi Connection (see NetManager) ----
constexpr int WIFI_MAX_NETWORKS = 5;                        // saved networks (+ built-in WIFI_SSID)
#define WIFI_AP_DEFAULT_SSID "thermostat-setup"
#define WIFI_AP_DEFAULT_PASSWORD "thermostat"               // WPA2 minimum is 8 characters
constexpr unsigned long WIFI_AP_FALLBACK_AFTER = 300000;    // ms offline before own AP starts (5 min)
constexpr unsigned long WIFI_AP_TEST_DURATION = 180000;     // ms the "test AP" button keeps it up
constexpr unsigned long WIFI_CONFIRM_TIMEOUT = 180000;      // ms before unproven settings roll back
constexpr unsigned long WIFI_RETRY_WITH_AP_CLIENT = 120000; // ms between scans while someone uses the AP

// ---- Default Temperature Settings ----
// For HEATING: ON when temp drops to tempOn, OFF when temp rises to tempOff
// For COOLING: ON when temp rises to tempOn, OFF when temp drops to tempOff
constexpr float DEFAULT_TEMP_ON = 18.0;     // °C (~65°F) - turn on heating when cold
constexpr float DEFAULT_TEMP_OFF = 21.0;    // °C (~70°F) - turn off heating when warm

#endif // CONFIG_H
