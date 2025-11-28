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
  void applyRelayLogic(float currentTemp);

  // Getters
  bool getRelayState(int index) const;
  Mode getRelayMode(int index) const;
  RelayType getRelayType(int index) const;
  float getTempOn(int index) const;
  float getTempOff(int index) const;

  // Setters
  void setRelayMode(int index, Mode mode);
  void setRelayType(int index, RelayType type);
  void setTempThresholds(int index, float tempOn, float tempOff);

  // Utility
  static String modeToString(Mode m);
  static String typeToString(RelayType t);

private:
  bool relayStates[4];
  Mode relayModes[4];
  RelayType relayTypes[4];
  float tempOn[4];
  float tempOff[4];
};

#endif // RELAY_CONTROLLER_H
