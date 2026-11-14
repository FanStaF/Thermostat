#ifndef SYSTEM_LOGGER_H
#define SYSTEM_LOGGER_H

#include <Arduino.h>
#include <time.h>
#include "Config.h"

struct LogEntry {
  String message;
  unsigned long timestamp; // millis() — for ordering and uptime fallback
  time_t epoch;            // UTC unix time, or 0 if NTP not synced yet
};

// Fixed-capacity ring buffer. Storage is allocated once (as a static array),
// adding a log never shifts entries, and messages are length-capped so the
// worst-case heap use is bounded (~MAX_SYSTEM_LOGS * MAX_LOG_MESSAGE_LEN).
class SystemLogger {
public:
  SystemLogger();

  void addLog(const String& message);

  // Number of stored entries (<= MAX_SYSTEM_LOGS).
  size_t getLogCount() const { return count; }
  // Entry by age: 0 = oldest, getLogCount()-1 = newest.
  const LogEntry& getLog(size_t i) const;

  // Local-time offset from UTC in seconds, supplied by the server (device
  // timezone setting) in heartbeat replies, so DST changes are picked up.
  void setUtcOffset(int32_t seconds) { utcOffset = seconds; utcOffsetKnown = true; }
  int32_t getUtcOffset() const { return utcOffset; }
  bool hasUtcOffset() const { return utcOffsetKnown; }

private:
  LogEntry entries[MAX_SYSTEM_LOGS];
  size_t head;   // index where the next entry will be written
  size_t count;
  int32_t utcOffset = 0;
  bool utcOffsetKnown = false;
};

// Global instance declaration
extern SystemLogger logger;

#endif // SYSTEM_LOGGER_H
