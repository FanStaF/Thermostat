#ifndef SYSTEM_LOGGER_H
#define SYSTEM_LOGGER_H

#include <Arduino.h>
#include <vector>
#include "Config.h"

struct LogEntry {
  String message;
  unsigned long timestamp;
};

class SystemLogger {
public:
  SystemLogger();

  void addLog(String message);
  const std::vector<LogEntry>& getLogs() const;
  size_t getLogCount() const;

private:
  std::vector<LogEntry> logs;
};

// Global instance declaration
extern SystemLogger logger;

#endif // SYSTEM_LOGGER_H
