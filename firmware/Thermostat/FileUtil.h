#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include <Arduino.h>
#include <LittleFS.h>

namespace FileUtil {

// Replace `path` with its contents from byte `offset` onward (via `tmpPath`
// + rename, so a reset mid-way never leaves a half-written file). With
// alignToLine, the copy starts after the next '\n' at/after offset.
// If the copy fails, the file is deleted rather than left oversized.
// Returns false if the file had to be dropped.
bool keepTail(const char* path, const char* tmpPath, size_t offset, bool alignToLine);

}  // namespace FileUtil

#endif // FILE_UTIL_H
