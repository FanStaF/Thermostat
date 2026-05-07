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
                               // "disconnect", "out-of-range", or "inconsistent"
};

class TemperatureManager {
public:
  TemperatureManager();

  void begin();
  TempReadResult readTemperatureWithValidation();
  void logTemperature(float temp, int sensorID);

  float getCurrentTemp() const { return currentTemp; }
  void setCurrentTemp(float temp) { currentTemp = temp; }

  // Temperature conversion helpers
  static float celsiusToFahrenheit(float c);
  static float fahrenheitToCelsius(float f);
  static String formatTemp(float tempC, bool useFahrenheit, int decimals = 1);
  static String getTempUnit(bool useFahrenheit);

private:
  OneWire oneWire;
  DallasTemperature sensors;
  float currentTemp;
};

#endif // TEMPERATURE_MANAGER_H
