#include "TemperatureManager.h"
#include "FileUtil.h"

TemperatureManager::TemperatureManager()
  : oneWire(ONE_WIRE_BUS), sensors(&oneWire), currentTemp(NAN),
    state(IDLE), stateStart(0), conversionMs(750), attempt(0), sampleIdx(0),
    lastReason(nullptr), logFileSize(-1), appendsSinceInfo(0), fsLow(false) {
}

void TemperatureManager::begin() {
  rescanBus();
  logger.addLog("DS18B20 sensor initialized (" + String(sensors.getDeviceCount()) +
                " found, " + String(conversionMs) + "ms/conversion)");
}

// DallasTemperature only enumerates the bus in begin(); getTempCByIndex()
// can't see a sensor that was absent then. Re-run it after a disconnect so a
// sensor that comes back (loose connector at power-up) is picked up again.
void TemperatureManager::rescanBus() {
  sensors.begin();
  sensors.setWaitForConversion(false);  // we poll instead of blocking ~750ms
  uint8_t res = sensors.getResolution();
  conversionMs = sensors.millisToWaitForConversion(res ? res : 12);
}

void TemperatureManager::requestSample() {
  sensors.requestTemperatures();
  stateStart = millis();
  state = CONVERTING;
}

void TemperatureManager::startReading() {
  if (sensors.getDeviceCount() == 0) {
    rescanBus();
    if (sensors.getDeviceCount() > 0) logger.addLog("DS18B20 sensor found on rescan");
  }
  attempt = 0;
  sampleIdx = 0;
  lastReason = nullptr;
  requestSample();
}

void TemperatureManager::failAttempt(const char* reason, TempReadResult& out, bool& done) {
  lastReason = reason;
  if (strcmp(reason, "disconnect") == 0) rescanBus();
  if (attempt == 0) {
    // One retry after a short pause.
    attempt = 1;
    sampleIdx = 0;
    stateStart = millis();
    state = RETRY_WAIT;
    return;
  }
  state = IDLE;
  out = {false, 0.0f, 2, lastReason};
  done = true;
}

bool TemperatureManager::poll(TempReadResult& out) {
  bool done = false;
  unsigned long now = millis();

  switch (state) {
    case IDLE:
      return false;

    case RETRY_WAIT:
      if (now - stateStart >= (unsigned long)TEMP_RETRY_DELAY) requestSample();
      return false;

    case CONVERTING: {
      // Small margin over the datasheet conversion time.
      if (now - stateStart < (unsigned long)conversionMs + 10) return false;

      float s = sensors.getTempCByIndex(0);
      if (s == DEVICE_DISCONNECTED_C) {
        failAttempt("disconnect", out, done);
      } else if (s == TEMP_POWER_ON_VALUE) {
        failAttempt("power-on-reset", out, done);
      } else if (s < TEMP_MIN_VALID || s >= TEMP_MAX_VALID) {
        failAttempt("out-of-range", out, done);
      } else {
        samples[sampleIdx++] = s;
        if (sampleIdx < TEMP_NUM_SAMPLES) {
          requestSample();
          return false;
        }

        float sum = 0.0f;
        for (int i = 0; i < TEMP_NUM_SAMPLES; i++) sum += samples[i];
        float avg = sum / TEMP_NUM_SAMPLES;
        float maxDiff = 0.0f;
        for (int i = 0; i < TEMP_NUM_SAMPLES; i++) {
          float d = fabs(samples[i] - avg);
          if (d > maxDiff) maxDiff = d;
        }

        if (maxDiff <= TEMP_MAX_DIFF) {
          state = IDLE;
          // lastReason is null on first-try success, set on retry-success
          out = {true, avg, (uint8_t)(attempt + 1), lastReason};
          return true;
        }
        failAttempt("inconsistent", out, done);
      }
      return done;
    }
  }
  return false;
}

TempReadResult TemperatureManager::readBlocking() {
  TempReadResult r = {false, 0.0f, 0, "unknown"};
  startReading();
  while (!poll(r)) {
    delay(10);  // yields to the WiFi stack / watchdog
  }
  return r;
}

void TemperatureManager::trimLogIfNeeded() {
  // Size is tracked in RAM; the file is only stat'ed once after boot.
  if (logFileSize < 0) {
    File f = LittleFS.open(LOG_FILE, "r");
    logFileSize = f ? (long)f.size() : 0;
    if (f) f.close();
  }
  if ((size_t)logFileSize <= LOG_FILE_MAX_BYTES) return;

  File in = LittleFS.open(LOG_FILE, "r");
  if (!in) { logFileSize = 0; return; }
  size_t size = in.size();
  if (size <= LOG_FILE_MAX_BYTES) {  // RAM estimate was off; resync
    in.close();
    logFileSize = size;
    return;
  }

  in.close();

  // Keep only the newest ~LOG_FILE_KEEP_BYTES, starting at a line boundary.
  if (!FileUtil::keepTail(LOG_FILE, LOG_TMP_FILE, size - LOG_FILE_KEEP_BYTES, true)) {
    logger.addLog("WARN: Temp log trim failed, history cleared");
  }
  logFileSize = -1;  // re-stat on next append
  appendsSinceInfo = FS_INFO_CHECK_EVERY;  // and re-check free space
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

  trimLogIfNeeded();

  // Never let the history log eat the space config.json needs. info() walks
  // the allocation table, so only check every FS_INFO_CHECK_EVERY appends.
  if (++appendsSinceInfo >= FS_INFO_CHECK_EVERY) {
    appendsSinceInfo = 0;
    FSInfo info;
    bool low = LittleFS.info(info) && info.totalBytes - info.usedBytes < FS_MIN_FREE_BYTES;
    if (low && !fsLow) logger.addLog("WARN: Filesystem nearly full, temp logging paused");
    if (!low && fsLow) logger.addLog("Filesystem space OK, temp logging resumed");
    fsLow = low;
  }
  if (fsLow) return;

  File logFile = LittleFS.open(LOG_FILE, "a");
  if (logFile) {
    size_t n = logFile.printf("%lu,%.2f,%d\n", timestamp, temp, sensorID);
    logFile.close();
    if (logFileSize >= 0) logFileSize += n;
    if (n == 0) appendsSinceInfo = FS_INFO_CHECK_EVERY;  // write failed: re-check space next time
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
