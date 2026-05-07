#include "SystemLogger.h"

SystemLogger::SystemLogger() {
  logs.reserve(MAX_SYSTEM_LOGS);
}

void SystemLogger::addLog(String message) {
  LogEntry log;
  log.message = message;
  log.timestamp = millis();

  // Capture wall-clock too. time(nullptr) returns 0 (or near-zero) until NTP
  // syncs; once synced the value jumps to a real UTC unix timestamp.
  time_t now = time(nullptr);
  log.epoch = (now >= 1000000000) ? now : 0;

  logs.push_back(log);

  if (logs.size() > MAX_SYSTEM_LOGS) {
    logs.erase(logs.begin());
  }

  Serial.println(message);
}

const std::vector<LogEntry>& SystemLogger::getLogs() const {
  return logs;
}

size_t SystemLogger::getLogCount() const {
  return logs.size();
}

// Global instance definition
SystemLogger logger;
