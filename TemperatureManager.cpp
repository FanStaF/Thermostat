#include "TemperatureManager.h"

TemperatureManager::TemperatureManager()
  : oneWire(ONE_WIRE_BUS), sensors(&oneWire), currentTemp(0.0) {
}

void TemperatureManager::begin() {
  sensors.begin();
  logger.addLog("DS18B20 sensor initialized");
}

bool TemperatureManager::readTemperatureWithValidation(float &outTemp) {
  for (int attempt = 0; attempt < 2; attempt++) { // Try twice
    float samples[TEMP_NUM_SAMPLES];
    bool allValid = true;

    // Take samples
    for (int i = 0; i < TEMP_NUM_SAMPLES; i++) {
      sensors.requestTemperatures();
      delay(TEMP_SAMPLE_DELAY);
      samples[i] = sensors.getTempCByIndex(0);

      // Check for obvious sensor errors
      if (samples[i] == -127.0 || samples[i] < TEMP_MIN_VALID || samples[i] > TEMP_MAX_VALID) {
        allValid = false;
        break;
      }
      yield(); // Feed watchdog
    }

    if (!allValid) {
      logger.addLog("Temp read failed (attempt " + String(attempt + 1) + "), retrying...");
      delay(TEMP_RETRY_DELAY);
      continue;
    }

    // Calculate average and check consistency
    float avg = (samples[0] + samples[1] + samples[2]) / 3.0;
    float maxDiff = 0;

    for (int i = 0; i < TEMP_NUM_SAMPLES; i++) {
      float diff = abs(samples[i] - avg);
      if (diff > maxDiff) maxDiff = diff;
    }

    if (maxDiff <= TEMP_MAX_DIFF) {
      // Samples are consistent
      outTemp = avg;
      return true;
    } else {
      logger.addLog("Temp samples inconsistent (diff=" + String(maxDiff, 2) + "C), retrying...");
      delay(TEMP_RETRY_DELAY);
    }
  }

  // Both attempts failed
  logger.addLog("ERROR: Temperature read failed after 2 attempts");
  return false;
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
