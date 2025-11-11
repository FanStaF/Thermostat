#ifndef CONFIG_H
#define CONFIG_H

#include "Credentials.h"

// ---- Pin Definitions ----
#define ONE_WIRE_BUS D2
static const int RELAY_PINS[4] = { D1, D5, D6, D7 };

// ---- System Configuration ----
constexpr int DEFAULT_UPDATE_FREQUENCY = 5; // seconds
constexpr size_t MAX_SYSTEM_LOGS = 100;

// ---- API Configuration ----
// API_URL is defined in Credentials.h
#define FIRMWARE_VERSION "2.0.0"
constexpr int API_HEARTBEAT_INTERVAL = 60000;  // ms (1 minute)
constexpr int API_COMMAND_POLL_INTERVAL = 30000;  // ms (30 seconds)

// ---- File Paths ----
#define CONFIG_FILE "/config.json"
#define LOG_FILE "/temp_log.csv"

// ---- OTA Configuration ----
#define OTA_HOSTNAME "thermostat"

// ---- Temperature Sensor Configuration ----
constexpr int TEMP_NUM_SAMPLES = 3;
constexpr float TEMP_MAX_DIFF = 2.0;        // Max difference between samples in °C
constexpr float TEMP_MIN_VALID = -50.0;     // DS18B20 can read -55°C but let's be safe
constexpr float TEMP_MAX_VALID = 85.0;      // DS18B20 max is 125°C but 85°C is realistic
constexpr int TEMP_SAMPLE_DELAY = 100;      // ms between samples
constexpr int TEMP_RETRY_DELAY = 200;       // ms before retry

// ---- Web Server Configuration ----
constexpr int WEB_SERVER_PORT = 80;
constexpr int MAX_DATA_LINES = 500;         // Maximum lines to send in data endpoint
constexpr int MIN_DATA_LINES = 100;         // Minimum lines to send
constexpr int BYTES_PER_LINE_ESTIMATE = 20; // For file seeking calculation

// ---- WiFi Connection ----
constexpr int WIFI_MAX_ATTEMPTS = 40;
constexpr int WIFI_RETRY_DELAY = 500;       // ms

// ---- Default Temperature Settings ----
constexpr float DEFAULT_TEMP_ON = 25.0;     // °C
constexpr float DEFAULT_TEMP_OFF = 23.0;    // °C

#endif // CONFIG_H
