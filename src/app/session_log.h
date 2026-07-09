#pragma once

namespace toy {

// Structured session log (v0.6 P1) — no secrets, locations only.
// Writes to logs/session-YYYYMMDD-HHMMSS.txt under cwd or APPDATA/toy-soldiers/logs.

void sessionLogOpen();
void sessionLogClose();
void sessionLog(const char* level, const char* msg);
void sessionLogf(const char* level, const char* fmt, ...);

const char* sessionLogPath();

} // namespace toy
