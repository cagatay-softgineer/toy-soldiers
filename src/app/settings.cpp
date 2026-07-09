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

void clampSettings(Settings& out)
{
	if (out.joinPort < 1024 || out.joinPort > 65535) {
		out.joinPort = kDefaultPort;
	}
	if (out.hostPort < 1024 || out.hostPort > 65535) {
		out.hostPort = kDefaultPort;
	}
	if (out.uiScalePercent != 100 && out.uiScalePercent != 125 && out.uiScalePercent != 150) {
		out.uiScalePercent = 100;
	}
	if (out.windowWidth < 800) {
		out.windowWidth = 800;
	}
	if (out.windowHeight < 600) {
		out.windowHeight = 600;
	}
	if (out.language != 0 && out.language != 1) {
		out.language = 0;
	}
	if (out.lastMode < 0 || out.lastMode > 3) {
		out.lastMode = 0;
	}
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

void settingsReset(Settings& out)
{
	out = Settings{};
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
		} else if (std::strcmp(key, "fullscreen") == 0) {
			out.fullscreen = parseBool(val, false);
		} else if (std::strcmp(key, "vsync") == 0) {
			out.vsync = parseBool(val, true);
		} else if (std::strcmp(key, "windowWidth") == 0) {
			out.windowWidth = std::atoi(val);
		} else if (std::strcmp(key, "windowHeight") == 0) {
			out.windowHeight = std::atoi(val);
		} else if (std::strcmp(key, "showFps") == 0) {
			out.showFps = parseBool(val, false);
		} else if (std::strcmp(key, "showSyncGen") == 0) {
			out.showSyncGen = parseBool(val, false);
		} else if (std::strcmp(key, "highContrast") == 0) {
			out.highContrast = parseBool(val, false);
		} else if (std::strcmp(key, "language") == 0) {
			out.language = std::atoi(val);
		} else if (std::strcmp(key, "lastMode") == 0) {
			out.lastMode = std::atoi(val);
		}
	}
	std::fclose(f);
	clampSettings(out);
	return true;
}

bool settingsSave(const Settings& s)
{
	Settings tmp = s;
	clampSettings(tmp);
	FILE* f = std::fopen(settingsPath(), "wb");
	if (!f) {
		return false;
	}
	std::fprintf(f, "playerName=%s\n", tmp.playerName);
	std::fprintf(f, "joinHost=%s\n", tmp.joinHost);
	std::fprintf(f, "joinPort=%d\n", tmp.joinPort);
	std::fprintf(f, "hostPort=%d\n", tmp.hostPort);
	std::fprintf(f, "mapIndex=%d\n", tmp.mapIndex);
	std::fprintf(f, "eventsEnabled=%d\n", tmp.eventsEnabled ? 1 : 0);
	std::fprintf(f, "fillAI=%d\n", tmp.fillAI ? 1 : 0);
	std::fprintf(f, "autoOrbit=%d\n", tmp.autoOrbit ? 1 : 0);
	std::fprintf(f, "showHowToPlay=%d\n", tmp.showHowToPlay ? 1 : 0);
	std::fprintf(f, "masterVolume=%.3f\n", static_cast<double>(tmp.masterVolume));
	std::fprintf(f, "plasticIndex=%d\n", tmp.plasticIndex);
	std::fprintf(f, "towerSkinIndex=%d\n", tmp.towerSkinIndex);
	std::fprintf(f, "accessoryIndex=%d\n", tmp.accessoryIndex);
	std::fprintf(f, "uiScalePercent=%d\n", tmp.uiScalePercent);
	std::fprintf(f, "fullscreen=%d\n", tmp.fullscreen ? 1 : 0);
	std::fprintf(f, "vsync=%d\n", tmp.vsync ? 1 : 0);
	std::fprintf(f, "windowWidth=%d\n", tmp.windowWidth);
	std::fprintf(f, "windowHeight=%d\n", tmp.windowHeight);
	std::fprintf(f, "showFps=%d\n", tmp.showFps ? 1 : 0);
	std::fprintf(f, "showSyncGen=%d\n", tmp.showSyncGen ? 1 : 0);
	std::fprintf(f, "highContrast=%d\n", tmp.highContrast ? 1 : 0);
	std::fprintf(f, "language=%d\n", tmp.language);
	std::fprintf(f, "lastMode=%d\n", tmp.lastMode);
	std::fclose(f);
	return true;
}

} // namespace toy
