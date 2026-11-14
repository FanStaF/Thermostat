#ifndef TEMPERATURE_MANAGER_H
#define TEMPERATURE_MANAGER_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LittleFS.h>
#include <time.h>
#include "Config.h"
#include "SystemLogger.h"

struct TempReadResult {
  bool ok;                     // true on successful read (possibly after retry)
  float temp;                  // valid only when ok
  uint8_t attemptsTaken;       // 1 if first try succeeded, 2 if a retry happened
  const char* lastFailReason;  // nullptr if no attempt failed; otherwise
                               // "disconnect", "out-of-range", "power-on-reset"
                               // or "inconsistent"
};

// Reads are non-blocking: startReading() kicks off a conversion and poll()
// advances the state machine each loop() pass, returning true once a result
// is ready. One validated reading = TEMP_NUM_SAMPLES conversions (~750ms each
// at 12-bit), retried once on failure. The web server and OTA keep running.
class TemperatureManager {
public:
  TemperatureManager();

  void begin();
  void startReading();
  bool isReading() const { return state != IDLE; }
  bool poll(TempReadResult& out);
  // Blocking convenience wrapper (used once at boot).
  TempReadResult readBlocking();

  void logTemperature(float temp, int sensorID);
  // Call after the log file is deleted externally (e.g. "Clear Data").
  void onLogCleared() { logFileSize = 0; }

  // Bus diagnostics as JSON (blocks ~1s for one conversion). Aborts any
  // in-progress reading; the loop simply starts a new one.
  String diagnostics();

  // NAN until the first successful read.
  float getCurrentTemp() const { return currentTemp; }
  bool hasValidTemp() const { return !isnan(currentTemp); }
  void setCurrentTemp(float temp) { currentTemp = temp; }

  // Temperature conversion helpers
  static float celsiusToFahrenheit(float c);
  static float fahrenheitToCelsius(float f);
  static String formatTemp(float tempC, bool useFahrenheit, int decimals = 1);
  static String getTempUnit(bool useFahrenheit);

private:
  enum State { IDLE, CONVERTING, RETRY_WAIT };

  OneWire oneWire;
  DallasTemperature sensors;
  float currentTemp;

  State state;
  unsigned long stateStart;
  uint16_t conversionMs;
  uint8_t attempt;
  uint8_t sampleIdx;
  float samples[TEMP_NUM_SAMPLES];
  const char* lastReason;

  long logFileSize;        // bytes, -1 = not yet known
  int appendsSinceInfo;    // appends since the last free-space check
  bool fsLow;              // last free-space check said we're too full

  void rescanBus();
  void requestSample();
  void failAttempt(const char* reason, TempReadResult& out, bool& done);
  void trimLogIfNeeded();
};

#endif // TEMPERATURE_MANAGER_H
