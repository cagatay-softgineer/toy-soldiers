#include "app/crash_handler.h"

#include "app/session_log.h"

#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace toy {

namespace {

char g_lastDump[512] = {};

#if defined(_WIN32)
std::string crashesDir()
{
	const char* appdata = std::getenv("APPDATA");
	std::string dir = (appdata && appdata[0]) ? (std::string(appdata) + "\\toy-soldiers\\crashes") : "crashes";
	CreateDirectoryA((appdata && appdata[0]) ? (std::string(appdata) + "\\toy-soldiers").c_str() : ".", nullptr);
	CreateDirectoryA(dir.c_str(), nullptr);
	return dir;
}

LONG WINAPI toyUnhandledFilter(EXCEPTION_POINTERS* ep)
{
	const std::string dir = crashesDir();
	SYSTEMTIME st{};
	GetLocalTime(&st);
	char path[MAX_PATH];
	std::snprintf(path, sizeof(path), "%s\\crash-%04d%02d%02d-%02d%02d%02d.dmp", dir.c_str(), st.wYear, st.wMonth,
				  st.wDay, st.wHour, st.wMinute, st.wSecond);
	HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file != INVALID_HANDLE_VALUE) {
		MINIDUMP_EXCEPTION_INFORMATION mei{};
		mei.ThreadId = GetCurrentThreadId();
		mei.ExceptionPointers = ep;
		mei.ClientPointers = FALSE;
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpWithDataSegs, &mei, nullptr,
						  nullptr);
		CloseHandle(file);
		std::snprintf(g_lastDump, sizeof(g_lastDump), "%s", path);
		sessionLogf("ERROR", "crash minidump written: %s", path);
	}
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace

void crashHandlerInstall()
{
#if defined(_WIN32)
	SetUnhandledExceptionFilter(toyUnhandledFilter);
	sessionLog("INFO", "Windows crash handler installed (minidumps enabled)");
#else
	sessionLog("INFO", "crash handler: no-op on this platform");
#endif
}

const char* crashHandlerLastDumpPath()
{
	return g_lastDump;
}

} // namespace toy
