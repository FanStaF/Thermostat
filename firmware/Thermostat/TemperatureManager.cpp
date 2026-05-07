#include "TemperatureManager.h"

TemperatureManager::TemperatureManager()
  : oneWire(ONE_WIRE_BUS), sensors(&oneWire), currentTemp(0.0) {
}

void TemperatureManager::begin() {
  sensors.begin();
  logger.addLog("DS18B20 sensor initialized");
}

TempReadResult TemperatureManager::readTemperatureWithValidation() {
  TempReadResult result = {false, 0.0f, 0, nullptr};
  const char* lastReason = nullptr;

  for (int attempt = 0; attempt < 2; attempt++) {
    result.attemptsTaken = attempt + 1;

    float samples[TEMP_NUM_SAMPLES];
    bool sawDisconnect = false;
    bool sawOutOfRange = false;
    bool allValid = true;

    for (int i = 0; i < TEMP_NUM_SAMPLES; i++) {
      sensors.requestTemperatures();
      delay(TEMP_SAMPLE_DELAY);
      samples[i] = sensors.getTempCByIndex(0);

      if (samples[i] == -127.0f) {
        sawDisconnect = true;
        allValid = false;
        break;
      }
      if (samples[i] < TEMP_MIN_VALID || samples[i] > TEMP_MAX_VALID) {
        sawOutOfRange = true;
        allValid = false;
        break;
      }
      yield(); // Feed watchdog
    }

    if (!allValid) {
      lastReason = sawDisconnect ? "disconnect" : "out-of-range";
      delay(TEMP_RETRY_DELAY);
      continue;
    }

    float avg = (samples[0] + samples[1] + samples[2]) / 3.0f;
    float maxDiff = 0.0f;
    for (int i = 0; i < TEMP_NUM_SAMPLES; i++) {
      float d = fabs(samples[i] - avg);
      if (d > maxDiff) maxDiff = d;
    }

    if (maxDiff <= TEMP_MAX_DIFF) {
      result.ok = true;
      result.temp = avg;
      result.lastFailReason = lastReason; // null on first-try success, set on retry-success
      return result;
    }

    lastReason = "inconsistent";
    delay(TEMP_RETRY_DELAY);
  }

  // Both attempts failed
  result.lastFailReason = lastReason ? lastReason : "unknown";
  return result;
}

void TemperatureManager::logTemperature(float temp, int sensorID) {
  // Get real Unix timestamp
  time_t now = time(nullptr);

  // Log with timestamp (or use millis/1000 as fallback before NTP sync)
  unsigned long timestamp;
  if (now < 1000000000) {
    // NTP not synced yet, use millis as seconds since boot
    timestamp = millis() / 1000;
    static bool warned = false;
    if (!warned) {
      logger.addLog("Logging temp (NTP not synced yet)");
      warned = true;
    }
  } else {
    timestamp = (unsigned long)now;
    static bool ntpSynced = false;
    if (!ntpSynced) {
      logger.addLog("NTP synced! Using real timestamps");
      ntpSynced = true;
    }
  }

  File logFile = LittleFS.open(LOG_FILE, "a");
  if (logFile) {
    logFile.printf("%lu,%.2f,%d\n", timestamp, temp, sensorID);
    logFile.close();
  } else {
    static bool fileError = false;
    if (!fileError) {
      logger.addLog("ERROR: Cannot open log file");
      fileError = true;
    }
  }
}

// Temperature conversion helpers
float TemperatureManager::celsiusToFahrenheit(float c) {
  return (c * 9.0 / 5.0) + 32.0;
}

float TemperatureManager::fahrenheitToCelsius(float f) {
  return (f - 32.0) * 5.0 / 9.0;
}

String TemperatureManager::formatTemp(float tempC, bool useFahrenheit, int decimals) {
  if (useFahrenheit) {
    return String(celsiusToFahrenheit(tempC), decimals);
  }
  return String(tempC, decimals);
}

String TemperatureManager::getTempUnit(bool useFahrenheit) {
  return useFahrenheit ? "F" : "C";
}
