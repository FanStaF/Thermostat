#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include "Config.h"
#include "SystemLogger.h"

enum Mode { AUTO = 0, MANUAL_ON = 1, MANUAL_OFF = 2 };
enum RelayType { HEATING = 0, COOLING = 1, GENERIC = 2, MANUAL_ONLY = 3 };

class RelayController {
public:
  RelayController();

  void begin();

  // Re-evaluate every relay. While a sensor fault is active, AUTO relays are
  // forced OFF regardless of currentTemp (manual modes are unaffected).
  void applyRelayLogic(float currentTemp);

  // Sensor fail-safe. Set when the temperature is unknown/untrustworthy.
  void setSensorFault(bool fault);
  bool hasSensorFault() const { return sensorFault; }

  // Getters
  bool getRelayState(int index) const;
  Mode getRelayMode(int index) const;
  RelayType getRelayType(int index) const;
  float getTempOn(int index) const;
  float getTempOff(int index) const;

  // Setters — return false (and change nothing) on invalid input.
  bool setRelayMode(int index, Mode mode);
  // If the relay's thresholds are in the wrong order for the new type they
  // are swapped, so a type change never leaves a relay without hysteresis.
  bool setRelayType(int index, RelayType type);
  // Both values in range AND correctly ordered for the relay's current type:
  // HEATING needs ON < OFF, COOLING/GENERIC need ON > OFF.
  bool setTempThresholds(int index, float tempOn, float tempOff);

  // Utility
  static String modeToString(Mode m);
  static String typeToString(RelayType t);
  // Accepts "AUTO", "ON"/"MANUAL_ON", "OFF"/"MANUAL_OFF".
  static bool parseMode(const String& s, Mode& out);
  // Accepts "HEATING", "COOLING", "GENERIC", "MANUAL_ONLY".
  static bool parseType(const String& s, RelayType& out);
  static bool isValidThreshold(float t);
  static bool thresholdsOrdered(RelayType type, float tempOn, float tempOff);
  // Human-readable ordering rule for error messages.
  static const char* thresholdRule(RelayType type);

private:
  bool relayStates[4];
  Mode relayModes[4];
  RelayType relayTypes[4];
  float tempOn[4];
  float tempOff[4];
  bool sensorFault;
  unsigned long lastChangeMs[4];  // millis() of the last state change
  bool everChanged[4];            // first AUTO decision is never delayed
};

#endif // RELAY_CONTROLLER_H
