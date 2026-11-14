#include "RelayController.h"

RelayController::RelayController() : sensorFault(false) {
  // Initialize default values
  for (int i = 0; i < 4; i++) {
    relayStates[i] = false;
    relayModes[i] = MANUAL_OFF;
    relayTypes[i] = HEATING;  // Default to heating for floor heat
    tempOn[i] = DEFAULT_TEMP_ON;
    tempOff[i] = DEFAULT_TEMP_OFF;
    lastChangeMs[i] = 0;
    everChanged[i] = false;
  }
}

void RelayController::begin() {
  // Initialize all relay pins to OFF
  for (int i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], HIGH);  // Active-LOW: HIGH = OFF
  }
  logger.addLog("All 4 relays initialized OFF");
}

void RelayController::setSensorFault(bool fault) {
  if (fault == sensorFault) return;
  sensorFault = fault;
  logger.addLog(fault ? "SENSOR FAULT: AUTO relays forced OFF until readings recover"
                      : "Sensor fault cleared, AUTO control resumed");
}

void RelayController::applyRelayLogic(float currentTemp) {
  for (int i = 0; i < 4; i++) {
    bool prevState = relayStates[i];

    switch (relayModes[i]) {
      case MANUAL_ON:
        relayStates[i] = true;
        break;
      case MANUAL_OFF:
        relayStates[i] = false;
        break;
      case AUTO:
      default: {
        // For MANUAL_ONLY type, AUTO behaves like MANUAL_OFF. Fail safe: with
        // no trustworthy temperature, AUTO relays are OFF — hardware override
        // switches are the force-on path, not the firmware.
        if (relayTypes[i] == MANUAL_ONLY || sensorFault || isnan(currentTemp)) {
          relayStates[i] = false;
          break;
        }

        // Apply temperature-based logic depending on relay type
        // Use small epsilon for floating point comparison to avoid edge flickering
        const float epsilon = 0.05;  // ~0.1°F tolerance
        bool want = relayStates[i];

        if (relayTypes[i] == HEATING) {
          // Heating: ON when cold, OFF when warm
          if (currentTemp < tempOn[i] - epsilon) want = true;
          else if (currentTemp > tempOff[i] + epsilon) want = false;
        } else {
          // Cooling/Generic: ON when hot, OFF when cold
          if (currentTemp > tempOn[i] + epsilon) want = true;
          else if (currentTemp < tempOff[i] - epsilon) want = false;
        }
        // Else maintain current state (hysteresis)

        // Minimum on/off time: hold the current state until it has lasted
        // long enough. Re-evaluated on every reading, so it switches as soon
        // as the hold expires if the condition still applies.
        if (want != relayStates[i] && everChanged[i]) {
          unsigned long held = millis() - lastChangeMs[i];
          unsigned long minHold = relayStates[i] ? RELAY_MIN_ON_MS : RELAY_MIN_OFF_MS;
          if (held < minHold) want = relayStates[i];
        }
        relayStates[i] = want;
        break;
      }
    }

    digitalWrite(RELAY_PINS[i], relayStates[i] ? LOW : HIGH); // Active-LOW

    if (prevState != relayStates[i]) {
      lastChangeMs[i] = millis();
      everChanged[i] = true;
      String msg = "Relay " + String(i + 1) + " -> " + (relayStates[i] ? "ON" : "OFF");
      if (isnan(currentTemp)) msg += " (no valid temp)";
      else msg += " @ " + String(currentTemp, 1) + "C";
      logger.addLog(msg);
    }
  }
}

// Getters
bool RelayController::getRelayState(int index) const {
  if (index >= 0 && index < 4) return relayStates[index];
  return false;
}

Mode RelayController::getRelayMode(int index) const {
  if (index >= 0 && index < 4) return relayModes[index];
  return MANUAL_OFF;
}

RelayType RelayController::getRelayType(int index) const {
  if (index >= 0 && index < 4) return relayTypes[index];
  return HEATING;
}

float RelayController::getTempOn(int index) const {
  if (index >= 0 && index < 4) return tempOn[index];
  return DEFAULT_TEMP_ON;
}

float RelayController::getTempOff(int index) const {
  if (index >= 0 && index < 4) return tempOff[index];
  return DEFAULT_TEMP_OFF;
}

// Setters
bool RelayController::setRelayMode(int index, Mode mode) {
  if (index < 0 || index >= 4) return false;
  if (mode != AUTO && mode != MANUAL_ON && mode != MANUAL_OFF) return false;
  if (relayModes[index] != mode) everChanged[index] = false;  // user action: no hold
  relayModes[index] = mode;
  return true;
}

bool RelayController::setRelayType(int index, RelayType type) {
  if (index < 0 || index >= 4) return false;
  if (type != HEATING && type != COOLING && type != GENERIC && type != MANUAL_ONLY) return false;
  relayTypes[index] = type;
  if (!thresholdsOrdered(type, tempOn[index], tempOff[index])) {
    float t = tempOn[index];
    tempOn[index] = tempOff[index];
    tempOff[index] = t;
    logger.addLog("Relay " + String(index + 1) + ": ON/OFF swapped to suit " + typeToString(type));
  }
  return true;
}

bool RelayController::isValidThreshold(float t) {
  return isfinite(t) && t >= THRESHOLD_MIN && t <= THRESHOLD_MAX;
}

bool RelayController::thresholdsOrdered(RelayType type, float tOn, float tOff) {
  switch (type) {
    case HEATING: return tOn < tOff;
    case COOLING:
    case GENERIC: return tOn > tOff;
    default:      return true;  // MANUAL_ONLY ignores thresholds
  }
}

const char* RelayController::thresholdRule(RelayType type) {
  switch (type) {
    case HEATING: return "for heating, ON must be below OFF";
    case COOLING:
    case GENERIC: return "for cooling, ON must be above OFF";
    default:      return "";
  }
}

bool RelayController::setTempThresholds(int index, float tOn, float tOff) {
  if (index < 0 || index >= 4) return false;
  if (!isValidThreshold(tOn) || !isValidThreshold(tOff)) return false;
  if (!thresholdsOrdered(relayTypes[index], tOn, tOff)) return false;
  everChanged[index] = false;  // user action: apply new thresholds immediately
  tempOn[index] = tOn;
  tempOff[index] = tOff;
  return true;
}

// Utility
String RelayController::modeToString(Mode m) {
  switch (m) {
    case AUTO: return "AUTO";
    case MANUAL_ON: return "MANUAL_ON";
    case MANUAL_OFF: return "MANUAL_OFF";
  }
  return "AUTO";
}

String RelayController::typeToString(RelayType t) {
  switch (t) {
    case HEATING: return "HEATING";
    case COOLING: return "COOLING";
    case GENERIC: return "GENERIC";
    case MANUAL_ONLY: return "MANUAL_ONLY";
  }
  return "HEATING";
}

bool RelayController::parseMode(const String& s, Mode& out) {
  if (s == "AUTO") { out = AUTO; return true; }
  if (s == "ON" || s == "MANUAL_ON") { out = MANUAL_ON; return true; }
  if (s == "OFF" || s == "MANUAL_OFF") { out = MANUAL_OFF; return true; }
  return false;
}

bool RelayController::parseType(const String& s, RelayType& out) {
  if (s == "HEATING") { out = HEATING; return true; }
  if (s == "COOLING") { out = COOLING; return true; }
  if (s == "GENERIC") { out = GENERIC; return true; }
  if (s == "MANUAL_ONLY") { out = MANUAL_ONLY; return true; }
  return false;
}
