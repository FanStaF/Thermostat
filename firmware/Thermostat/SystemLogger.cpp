#include "SystemLogger.h"

SystemLogger::SystemLogger() : head(0), count(0) {
}

void SystemLogger::addLog(const String& message) {
  LogEntry& log = entries[head];

  if (message.length() > MAX_LOG_MESSAGE_LEN) {
    log.message = message.substring(0, MAX_LOG_MESSAGE_LEN);
    log.message += "...";
  } else {
    log.message = message;
  }
  log.timestamp = millis();

  // Capture wall-clock too. time(nullptr) returns 0 (or near-zero) until NTP
  // syncs; once synced the value jumps to a real UTC unix timestamp.
  time_t now = time(nullptr);
  log.epoch = (now >= 1000000000) ? now : 0;

  head = (head + 1) % MAX_SYSTEM_LOGS;
  if (count < MAX_SYSTEM_LOGS) count++;

  Serial.println(log.message);
}

const LogEntry& SystemLogger::getLog(size_t i) const {
  // Oldest entry sits at head when the buffer is full, at 0 otherwise.
  size_t oldest = (count < MAX_SYSTEM_LOGS) ? 0 : head;
  return entries[(oldest + i) % MAX_SYSTEM_LOGS];
}

// Global instance definition
SystemLogger logger;
