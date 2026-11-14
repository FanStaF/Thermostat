#include "FileUtil.h"

namespace FileUtil {

bool keepTail(const char* path, const char* tmpPath, size_t offset, bool alignToLine) {
  File in = LittleFS.open(path, "r");
  if (!in) return true;  // nothing to do
  if (offset >= in.size()) {
    in.close();
    LittleFS.remove(path);
    return true;
  }

  in.seek(offset);
  if (alignToLine && offset > 0) in.readStringUntil('\n');

  File out = LittleFS.open(tmpPath, "w");
  bool ok = (bool)out;
  if (ok) {
    uint8_t buf[256];
    while (in.available()) {
      size_t n = in.read(buf, sizeof(buf));
      if (n == 0 || out.write(buf, n) != n) { ok = false; break; }
      yield();
    }
    out.close();
  }
  in.close();

  if (ok) {
    LittleFS.remove(path);
    ok = LittleFS.rename(tmpPath, path);
  }
  if (!ok) {
    LittleFS.remove(tmpPath);
    LittleFS.remove(path);
  }
  return ok;
}

}  // namespace FileUtil
