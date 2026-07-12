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
	if (out.matchesCompleted < 0) {
		out.matchesCompleted = 0;
	}
	if (out.matchesCompleted > 99999) {
		out.matchesCompleted = 99999;
	}
	auto clamp01 = [](float& v) {
		if (v < 0.0f) {
			v = 0.0f;
		}
		if (v > 1.0f) {
			v = 1.0f;
		}
	};
	clamp01(out.masterVolume);
	clamp01(out.sfxVolume);
	clamp01(out.musicVolume);
	if (out.wins < 0) {
		out.wins = 0;
	}
	if (out.wins > 99999) {
		out.wins = 99999;
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
		} else if (std::strcmp(key, "reducedMotion") == 0) {
			out.reducedMotion = parseBool(val, false);
		} else if (std::strcmp(key, "coachTips") == 0) {
			out.coachTips = parseBool(val, true);
		} else if (std::strcmp(key, "matchesCompleted") == 0) {
			out.matchesCompleted = std::atoi(val);
		} else if (std::strncmp(key, "recentHost", 10) == 0) {
			const int idx = std::atoi(key + 10);
			if (idx >= 0 && idx < Settings::kRecentHostMax) {
				std::snprintf(out.recentHosts[idx], sizeof(out.recentHosts[idx]), "%s", val);
			}
		} else if (std::strcmp(key, "sfxVolume") == 0) {
			out.sfxVolume = static_cast<float>(std::atof(val));
		} else if (std::strcmp(key, "musicVolume") == 0) {
			out.musicVolume = static_cast<float>(std::atof(val));
		} else if (std::strcmp(key, "wins") == 0) {
			out.wins = std::atoi(val);
		} else if (std::strcmp(key, "tutorialDone") == 0) {
			out.tutorialDone = parseBool(val, false);
		} else if (std::strcmp(key, "hostedLobbies") == 0) {
			out.hostedLobbies = std::atoi(val);
		} else if (std::strcmp(key, "missionFlags") == 0) {
			out.missionFlags = std::atoi(val);
		} else if (std::strncmp(key, "mapPlays", 8) == 0) {
			const int idx = std::atoi(key + 8);
			if (idx >= 0 && idx < 7) {
				out.mapPlays[idx] = std::atoi(val);
			}
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
	std::fprintf(f, "reducedMotion=%d\n", tmp.reducedMotion ? 1 : 0);
	std::fprintf(f, "coachTips=%d\n", tmp.coachTips ? 1 : 0);
	std::fprintf(f, "matchesCompleted=%d\n", tmp.matchesCompleted);
	for (int i = 0; i < Settings::kRecentHostMax; ++i) {
		if (tmp.recentHosts[i][0]) {
			std::fprintf(f, "recentHost%d=%s\n", i, tmp.recentHosts[i]);
		}
	}
	std::fprintf(f, "sfxVolume=%.3f\n", static_cast<double>(tmp.sfxVolume));
	std::fprintf(f, "musicVolume=%.3f\n", static_cast<double>(tmp.musicVolume));
	std::fprintf(f, "wins=%d\n", tmp.wins);
	std::fprintf(f, "tutorialDone=%d\n", tmp.tutorialDone ? 1 : 0);
	std::fprintf(f, "hostedLobbies=%d\n", tmp.hostedLobbies);
	std::fprintf(f, "missionFlags=%d\n", tmp.missionFlags);
	for (int i = 0; i < 7; ++i) {
		if (tmp.mapPlays[i] > 0) {
			std::fprintf(f, "mapPlays%d=%d\n", i, tmp.mapPlays[i]);
		}
	}
	std::fclose(f);
	return true;
}

void settingsAddRecentHost(Settings& s, const char* name, const char* ip, int port)
{
	if (!ip || !ip[0]) {
		return;
	}
	char entry[96];
	std::snprintf(entry, sizeof(entry), "%s|%s|%d", (name && name[0]) ? name : "Host", ip, port);
	// Dedup by ip|port suffix.
	char suffix[80];
	std::snprintf(suffix, sizeof(suffix), "|%s|%d", ip, port);
	int writeAt = Settings::kRecentHostMax - 1;
	for (int i = 0; i < Settings::kRecentHostMax; ++i) {
		if (s.recentHosts[i][0] && std::strstr(s.recentHosts[i], suffix)) {
			writeAt = i;
			break;
		}
		if (!s.recentHosts[i][0]) {
			writeAt = i;
			break;
		}
	}
	for (int i = writeAt; i > 0; --i) {
		std::snprintf(s.recentHosts[i], sizeof(s.recentHosts[i]), "%s", s.recentHosts[i - 1]);
	}
	std::snprintf(s.recentHosts[0], sizeof(s.recentHosts[0]), "%s", entry);
}

// --- v1.0 #186: match history ---

const char* historyPath()
{
	static std::string p;
	if (p.empty()) {
		p = settingsPath(); // ensures the directory exists
		const size_t slash = p.find_last_of("/\\");
		p = (slash == std::string::npos) ? "toy-soldiers-history.txt" : p.substr(0, slash + 1) + "history.txt";
	}
	return p.c_str();
}

bool historyAppend(const char* line)
{
	if (!line || !line[0]) {
		return false;
	}
	FILE* f = std::fopen(historyPath(), "ab");
	if (!f) {
		return false;
	}
	std::fprintf(f, "%s\n", line);
	std::fclose(f);
	return true;
}

int historyReadLast(char out[][160], int maxLines)
{
	if (maxLines <= 0) {
		return 0;
	}
	FILE* f = std::fopen(historyPath(), "rb");
	if (!f) {
		return 0;
	}
	// Ring buffer over all lines; file stays small (one line per match).
	std::string all;
	char buf[512];
	while (std::fgets(buf, sizeof(buf), f)) {
		all += buf;
	}
	std::fclose(f);

	// Collect line start offsets.
	std::string cur;
	// newest first: walk lines, keep last maxLines in a rotating window
	char window[20][160] = {};
	int count = 0;
	size_t start = 0;
	for (size_t i = 0; i <= all.size(); ++i) {
		if (i == all.size() || all[i] == '\n') {
			if (i > start) {
				std::string line = all.substr(start, i - start);
				while (!line.empty() && (line.back() == '\r')) {
					line.pop_back();
				}
				if (!line.empty()) {
					std::snprintf(window[count % 20], sizeof(window[0]), "%s", line.c_str());
					++count;
				}
			}
			start = i + 1;
		}
	}
	const int have = count < 20 ? count : 20;
	const int n = have < maxLines ? have : maxLines;
	for (int k = 0; k < n; ++k) {
		// newest first
		const int idx = (count - 1 - k) % 20;
		std::snprintf(out[k], 160, "%s", window[(idx + 20) % 20]);
	}
	return n;
}

namespace {

void jsonEscape(FILE* f, const char* s)
{
	if (!s) {
		std::fputs("\"\"", f);
		return;
	}
	std::fputc('"', f);
	for (const char* p = s; *p; ++p) {
		if (*p == '"' || *p == '\\') {
			std::fputc('\\', f);
		}
		if (*p == '\n' || *p == '\r') {
			continue;
		}
		std::fputc(*p, f);
	}
	std::fputc('"', f);
}

// Very small JSON string value extractor: "key":"value" or "key":123 or "key":true
bool jsonFind(const char* json, const char* key, char* out, int outCap)
{
	char pattern[128];
	std::snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	const char* p = std::strstr(json, pattern);
	if (!p) {
		return false;
	}
	p = std::strchr(p + std::strlen(pattern), ':');
	if (!p) {
		return false;
	}
	++p;
	while (*p == ' ' || *p == '\t') {
		++p;
	}
	if (*p == '"') {
		++p;
		int i = 0;
		while (*p && *p != '"' && i + 1 < outCap) {
			if (*p == '\\' && p[1]) {
				++p;
			}
			out[i++] = *p++;
		}
		out[i] = 0;
		return true;
	}
	// number / bool
	int i = 0;
	while (*p && *p != ',' && *p != '}' && *p != ' ' && i + 1 < outCap) {
		out[i++] = *p++;
	}
	out[i] = 0;
	return i > 0;
}

} // namespace

bool settingsExportJson(const Settings& s, const char* path)
{
	if (!path) {
		return false;
	}
	FILE* f = std::fopen(path, "wb");
	if (!f) {
		return false;
	}
	std::fputs("{\n", f);
	std::fputs("  \"playerName\": ", f);
	jsonEscape(f, s.playerName);
	std::fputs(",\n", f);
	std::fputs("  \"joinHost\": ", f);
	jsonEscape(f, s.joinHost);
	std::fprintf(f,
				 ",\n  \"joinPort\": %d,\n  \"hostPort\": %d,\n  \"mapIndex\": %d,\n  \"eventsEnabled\": %s,\n  "
				 "\"fillAI\": %s,\n  \"autoOrbit\": %s,\n  \"showHowToPlay\": %s,\n  \"masterVolume\": %.3f,\n  "
				 "\"plasticIndex\": %d,\n  \"towerSkinIndex\": %d,\n  \"accessoryIndex\": %d,\n  \"uiScalePercent\": "
				 "%d,\n  \"fullscreen\": %s,\n  \"vsync\": %s,\n  \"windowWidth\": %d,\n  \"windowHeight\": %d,\n  "
				 "\"showFps\": %s,\n  \"showSyncGen\": %s,\n  \"highContrast\": %s,\n  \"language\": %d,\n  "
				 "\"lastMode\": %d,\n  \"reducedMotion\": %s,\n  \"coachTips\": %s,\n  \"matchesCompleted\": %d\n}\n",
				 s.joinPort, s.hostPort, s.mapIndex, s.eventsEnabled ? "true" : "false", s.fillAI ? "true" : "false",
				 s.autoOrbit ? "true" : "false", s.showHowToPlay ? "true" : "false", static_cast<double>(s.masterVolume),
				 s.plasticIndex, s.towerSkinIndex, s.accessoryIndex, s.uiScalePercent,
				 s.fullscreen ? "true" : "false", s.vsync ? "true" : "false", s.windowWidth, s.windowHeight,
				 s.showFps ? "true" : "false", s.showSyncGen ? "true" : "false", s.highContrast ? "true" : "false",
				 s.language, s.lastMode, s.reducedMotion ? "true" : "false", s.coachTips ? "true" : "false",
				 s.matchesCompleted);
	std::fclose(f);
	return true;
}

bool settingsImportJson(Settings& out, const char* path)
{
	if (!path) {
		return false;
	}
	FILE* f = std::fopen(path, "rb");
	if (!f) {
		return false;
	}
	std::fseek(f, 0, SEEK_END);
	long sz = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > 64 * 1024) {
		std::fclose(f);
		return false;
	}
	std::string buf(static_cast<size_t>(sz) + 1, '\0');
	const size_t rd = std::fread(buf.data(), 1, static_cast<size_t>(sz), f);
	std::fclose(f);
	buf.resize(rd);
	const char* j = buf.c_str();
	char v[256];
	if (jsonFind(j, "playerName", v, sizeof(v))) {
		std::snprintf(out.playerName, sizeof(out.playerName), "%s", v);
	}
	if (jsonFind(j, "joinHost", v, sizeof(v))) {
		std::snprintf(out.joinHost, sizeof(out.joinHost), "%s", v);
	}
	if (jsonFind(j, "joinPort", v, sizeof(v))) {
		out.joinPort = std::atoi(v);
	}
	if (jsonFind(j, "hostPort", v, sizeof(v))) {
		out.hostPort = std::atoi(v);
	}
	if (jsonFind(j, "mapIndex", v, sizeof(v))) {
		out.mapIndex = std::atoi(v);
	}
	if (jsonFind(j, "eventsEnabled", v, sizeof(v))) {
		out.eventsEnabled = parseBool(v, true);
	}
	if (jsonFind(j, "fillAI", v, sizeof(v))) {
		out.fillAI = parseBool(v, true);
	}
	if (jsonFind(j, "autoOrbit", v, sizeof(v))) {
		out.autoOrbit = parseBool(v, true);
	}
	if (jsonFind(j, "showHowToPlay", v, sizeof(v))) {
		out.showHowToPlay = parseBool(v, true);
	}
	if (jsonFind(j, "masterVolume", v, sizeof(v))) {
		out.masterVolume = static_cast<float>(std::atof(v));
	}
	if (jsonFind(j, "plasticIndex", v, sizeof(v))) {
		out.plasticIndex = std::atoi(v);
	}
	if (jsonFind(j, "towerSkinIndex", v, sizeof(v))) {
		out.towerSkinIndex = std::atoi(v);
	}
	if (jsonFind(j, "accessoryIndex", v, sizeof(v))) {
		out.accessoryIndex = std::atoi(v);
	}
	if (jsonFind(j, "uiScalePercent", v, sizeof(v))) {
		out.uiScalePercent = std::atoi(v);
	}
	if (jsonFind(j, "fullscreen", v, sizeof(v))) {
		out.fullscreen = parseBool(v, false);
	}
	if (jsonFind(j, "vsync", v, sizeof(v))) {
		out.vsync = parseBool(v, true);
	}
	if (jsonFind(j, "windowWidth", v, sizeof(v))) {
		out.windowWidth = std::atoi(v);
	}
	if (jsonFind(j, "windowHeight", v, sizeof(v))) {
		out.windowHeight = std::atoi(v);
	}
	if (jsonFind(j, "showFps", v, sizeof(v))) {
		out.showFps = parseBool(v, false);
	}
	if (jsonFind(j, "showSyncGen", v, sizeof(v))) {
		out.showSyncGen = parseBool(v, false);
	}
	if (jsonFind(j, "highContrast", v, sizeof(v))) {
		out.highContrast = parseBool(v, false);
	}
	if (jsonFind(j, "language", v, sizeof(v))) {
		out.language = std::atoi(v);
	}
	if (jsonFind(j, "lastMode", v, sizeof(v))) {
		out.lastMode = std::atoi(v);
	}
	if (jsonFind(j, "reducedMotion", v, sizeof(v))) {
		out.reducedMotion = parseBool(v, false);
	}
	if (jsonFind(j, "coachTips", v, sizeof(v))) {
		out.coachTips = parseBool(v, true);
	}
	if (jsonFind(j, "matchesCompleted", v, sizeof(v))) {
		out.matchesCompleted = std::atoi(v);
	}
	clampSettings(out);
	return true;
}

} // namespace toy

