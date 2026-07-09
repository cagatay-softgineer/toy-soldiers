#include "app/session_log.h"

#define _CRT_SECURE_NO_WARNINGS
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace toy {

namespace {

FILE* g_file = nullptr;
std::string g_path;

void ensureLogsDir(std::string& dir)
{
#if defined(_WIN32)
	CreateDirectoryA(dir.c_str(), nullptr);
#else
	std::string cmd = "mkdir -p \"" + dir + "\"";
	(void)std::system(cmd.c_str());
#endif
}

} // namespace

const char* sessionLogPath()
{
	return g_path.c_str();
}

void sessionLogOpen()
{
	if (g_file) {
		return;
	}
	std::string dir;
#if defined(_WIN32)
	const char* appdata = std::getenv("APPDATA");
	if (appdata && appdata[0]) {
		dir = std::string(appdata) + "\\toy-soldiers\\logs";
	} else {
		dir = "logs";
	}
#else
	const char* home = std::getenv("HOME");
	dir = home ? (std::string(home) + "/.config/toy-soldiers/logs") : "logs";
#endif
	ensureLogsDir(dir);

	std::time_t t = std::time(nullptr);
	std::tm tm{};
#if defined(_WIN32)
	localtime_s(&tm, &t);
#else
	localtime_r(&t, &tm);
#endif
	char name[64];
	std::snprintf(name, sizeof(name), "session-%04d%02d%02d-%02d%02d%02d.txt", tm.tm_year + 1900, tm.tm_mon + 1,
				  tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	g_path = dir +
#if defined(_WIN32)
			 "\\"
#else
			 "/"
#endif
			 + name;
	g_file = std::fopen(g_path.c_str(), "wb");
	if (g_file) {
		sessionLog("INFO", "session log opened");
	}
}

void sessionLogClose()
{
	if (!g_file) {
		return;
	}
	sessionLog("INFO", "session log closed");
	std::fclose(g_file);
	g_file = nullptr;
}

void sessionLog(const char* level, const char* msg)
{
	if (!g_file) {
		return;
	}
	std::time_t t = std::time(nullptr);
	std::tm tm{};
#if defined(_WIN32)
	localtime_s(&tm, &t);
#else
	localtime_r(&t, &tm);
#endif
	std::fprintf(g_file, "%02d:%02d:%02d [%s] %s\n", tm.tm_hour, tm.tm_min, tm.tm_sec, level ? level : "INFO",
				 msg ? msg : "");
	std::fflush(g_file);
}

void sessionLogf(const char* level, const char* fmt, ...)
{
	if (!g_file || !fmt) {
		return;
	}
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	std::vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	sessionLog(level, buf);
}

} // namespace toy
