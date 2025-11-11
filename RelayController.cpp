#include "RelayController.h"

RelayController::RelayController() {
  // Initialize default values
  for (int i = 0; i < 4; i++) {
    relayStates[i] = false;
    relayModes[i] = MANUAL_OFF;
    tempOn[i] = DEFAULT_TEMP_ON;
    tempOff[i] = DEFAULT_TEMP_OFF;
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
        if (currentTemp >= tempOn[i]) relayStates[i] = true;
        else if (currentTemp <= tempOff[i]) relayStates[i] = false;
        // Else maintain current state (hysteresis)
        break;
    }

    digitalWrite(RELAY_PINS[i], relayStates[i] ? LOW : HIGH); // Active-LOW

    if (prevState != relayStates[i]) {
      logger.addLog("Relay " + String(i+1) + " -> " + String(relayStates[i] ? "ON" : "OFF") +
                    " @ " + String(currentTemp, 1) + "C");
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

float RelayController::getTempOn(int index) const {
  if (index >= 0 && index < 4) return tempOn[index];
  return DEFAULT_TEMP_ON;
}

float RelayController::getTempOff(int index) const {
  if (index >= 0 && index < 4) return tempOff[index];
  return DEFAULT_TEMP_OFF;
}

// Setters
void RelayController::setRelayMode(int index, Mode mode) {
  if (index >= 0 && index < 4) {
    relayModes[index] = mode;
  }
}

void RelayController::setTempThresholds(int index, float tOn, float tOff) {
  if (index >= 0 && index < 4) {
    tempOn[index] = tOn;
    tempOff[index] = tOff;
  }
}

// Utility
String RelayController::modeToString(Mode m) {
  switch (m) {
    case AUTO: return "AUTO";
    case MANUAL_ON: return "ON";
    case MANUAL_OFF: return "OFF";
  }
  return "?";
}
