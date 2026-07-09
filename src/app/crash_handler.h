#pragma once

namespace toy {

// Windows: install unhandled exception filter that writes a minidump under
// %APPDATA%/toy-soldiers/crashes/ and logs the path. No-op on other platforms.
void crashHandlerInstall();
const char* crashHandlerLastDumpPath();

} // namespace toy
