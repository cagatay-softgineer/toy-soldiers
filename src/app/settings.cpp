#include "app/settings.h"

#define _CRT_SECURE_NO_WARNINGS
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace toy {

namespace {

std::string& pathBuf()
{
	static std::string p;
	return p;
}

void ensureDir(const std::string& dir)
{
#if defined(_WIN32)
	CreateDirectoryA(dir.c_str(), nullptr);
#else
	std::string cmd = "mkdir -p \"" + dir + "\"";
	(void)std::system(cmd.c_str());
#endif
}

bool parseBool(const char* v, bool def)
{
	if (!v || !v[0]) {
		return def;
	}
	if (v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' || v[0] == 'Y') {
		return true;
	}
	if (v[0] == '0' || v[0] == 'f' || v[0] == 'F' || v[0] == 'n' || v[0] == 'N') {
		return false;
	}
	return def;
}

} // namespace

const char* settingsPath()
{
	if (!pathBuf().empty()) {
		return pathBuf().c_str();
	}
#if defined(_WIN32)
	const char* appdata = std::getenv("APPDATA");
	if (appdata && appdata[0]) {
		std::string dir = std::string(appdata) + "\\toy-soldiers";
		ensureDir(dir);
		pathBuf() = dir + "\\settings.ini";
	} else {
		pathBuf() = "toy-soldiers-settings.ini";
	}
#else
	const char* home = std::getenv("HOME");
	if (home) {
		std::string dir = std::string(home) + "/.config/toy-soldiers";
		ensureDir(dir);
		pathBuf() = dir + "/settings.ini";
	} else {
		pathBuf() = "toy-soldiers-settings.ini";
	}
#endif
	return pathBuf().c_str();
}

bool settingsLoad(Settings& out)
{
	FILE* f = std::fopen(settingsPath(), "rb");
	if (!f) {
		return false;
	}
	char line[256];
	while (std::fgets(line, sizeof(line), f)) {
		size_t n = std::strlen(line);
		while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
			line[--n] = 0;
		}
		char* eq = std::strchr(line, '=');
		if (!eq) {
			continue;
		}
		*eq = 0;
		const char* key = line;
		const char* val = eq + 1;
		if (std::strcmp(key, "playerName") == 0) {
			std::snprintf(out.playerName, sizeof(out.playerName), "%s", val);
		} else if (std::strcmp(key, "joinHost") == 0) {
			std::snprintf(out.joinHost, sizeof(out.joinHost), "%s", val);
		} else if (std::strcmp(key, "joinPort") == 0) {
			out.joinPort = std::atoi(val);
		} else if (std::strcmp(key, "hostPort") == 0) {
			out.hostPort = std::atoi(val);
		} else if (std::strcmp(key, "mapIndex") == 0) {
			out.mapIndex = std::atoi(val);
		} else if (std::strcmp(key, "eventsEnabled") == 0) {
			out.eventsEnabled = parseBool(val, true);
		} else if (std::strcmp(key, "fillAI") == 0) {
			out.fillAI = parseBool(val, true);
		} else if (std::strcmp(key, "autoOrbit") == 0) {
			out.autoOrbit = parseBool(val, true);
		} else if (std::strcmp(key, "showHowToPlay") == 0) {
			out.showHowToPlay = parseBool(val, true);
		} else if (std::strcmp(key, "masterVolume") == 0) {
			out.masterVolume = static_cast<float>(std::atof(val));
		} else if (std::strcmp(key, "plasticIndex") == 0) {
			out.plasticIndex = std::atoi(val);
		} else if (std::strcmp(key, "towerSkinIndex") == 0) {
			out.towerSkinIndex = std::atoi(val);
		} else if (std::strcmp(key, "accessoryIndex") == 0) {
			out.accessoryIndex = std::atoi(val);
		} else if (std::strcmp(key, "uiScalePercent") == 0) {
			out.uiScalePercent = std::atoi(val);
		}
	}
	std::fclose(f);
	if (out.joinPort < 1024 || out.joinPort > 65535) {
		out.joinPort = kDefaultPort;
	}
	if (out.hostPort < 1024 || out.hostPort > 65535) {
		out.hostPort = kDefaultPort;
	}
	if (out.uiScalePercent != 100 && out.uiScalePercent != 125 && out.uiScalePercent != 150) {
		out.uiScalePercent = 100;
	}
	return true;
}

bool settingsSave(const Settings& s)
{
	FILE* f = std::fopen(settingsPath(), "wb");
	if (!f) {
		return false;
	}
	std::fprintf(f, "playerName=%s\n", s.playerName);
	std::fprintf(f, "joinHost=%s\n", s.joinHost);
	std::fprintf(f, "joinPort=%d\n", s.joinPort);
	std::fprintf(f, "hostPort=%d\n", s.hostPort);
	std::fprintf(f, "mapIndex=%d\n", s.mapIndex);
	std::fprintf(f, "eventsEnabled=%d\n", s.eventsEnabled ? 1 : 0);
	std::fprintf(f, "fillAI=%d\n", s.fillAI ? 1 : 0);
	std::fprintf(f, "autoOrbit=%d\n", s.autoOrbit ? 1 : 0);
	std::fprintf(f, "showHowToPlay=%d\n", s.showHowToPlay ? 1 : 0);
	std::fprintf(f, "masterVolume=%.3f\n", static_cast<double>(s.masterVolume));
	std::fprintf(f, "plasticIndex=%d\n", s.plasticIndex);
	std::fprintf(f, "towerSkinIndex=%d\n", s.towerSkinIndex);
	std::fprintf(f, "accessoryIndex=%d\n", s.accessoryIndex);
	std::fprintf(f, "uiScalePercent=%d\n", s.uiScalePercent);
	std::fclose(f);
	return true;
}

} // namespace toy
