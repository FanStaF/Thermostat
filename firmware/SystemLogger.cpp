#include "SystemLogger.h"

SystemLogger::SystemLogger() {
  logs.reserve(MAX_SYSTEM_LOGS);
}

void SystemLogger::addLog(String message) {
  LogEntry log;
  log.message = message;
  log.timestamp = millis();

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
