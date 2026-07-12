#pragma once

#include "game/types.h"

namespace toy {

// Last launched mode for "Continue" (v0.6 P1).
enum class LastMode : int {
	None = 0,
	Offline = 1,
	Host = 2,
	Join = 3,
};

// v0.6: persistent player preferences (simple key=value file).
struct Settings {
	char playerName[kPlayerNameLen] = "Player";
	char joinHost[64] = "127.0.0.1";
	int joinPort = kDefaultPort;
	int hostPort = kDefaultPort;
	int mapIndex = 0;
	bool eventsEnabled = true;
	bool fillAI = true;
	bool autoOrbit = true;
	bool showHowToPlay = true;
	float masterVolume = 1.0f;
	int plasticIndex = 0;
	int towerSkinIndex = 0;
	int accessoryIndex = 0;
	int uiScalePercent = 100; // 100 / 125 / 150

	// P1 display
	bool fullscreen = false;
	bool vsync = true;
	int windowWidth = 1440;
	int windowHeight = 900;
	bool showFps = false;
	bool showSyncGen = false;
	bool highContrast = false;
	int language = 0; // 0=EN 1=TR
	int lastMode = 0; // LastMode

	// P2
	bool reducedMotion = false;   // shorter toasts, no auto-orbit
	bool coachTips = true;        // tips for first N completed matches
	int matchesCompleted = 0;     // local counter for coach tips

	// v0.8 #86/#110: recent hosts ("name|ip|port"), most recent first.
	static constexpr int kRecentHostMax = 5;
	char recentHosts[kRecentHostMax][96] = {};

	// v0.9 #134: per-category volumes (masterVolume above is the master).
	float sfxVolume = 0.8f;
	float musicVolume = 0.5f;
	// v0.9 #146: local unlock track.
	int wins = 0;

	// --- v1.0 meta ---
	bool tutorialDone = false; // #168/#169
	int hostedLobbies = 0;     // #171 badge counter
	int missionFlags = 0;      // #170 bitmask of completed challenge missions
	int mapPlays[7] = {};      // #185 favorite-map tracking (indexed by MapId)
};

// v0.8: push a host to the top of the recent list (dedup by ip+port).
void settingsAddRecentHost(Settings& s, const char* name, const char* ip, int port);

// v1.0 #186: plain-text match history next to settings.ini (append-only, read last N).
const char* historyPath();
bool historyAppend(const char* line);
// Fills out[0..returned-1] with the most recent lines, newest first (max 20).
int historyReadLast(char out[][160], int maxLines);

bool settingsLoad(Settings& out);
bool settingsSave(const Settings& s);
void settingsReset(Settings& out);
const char* settingsPath();

// JSON export/import (v0.6 P2 #19) — simple hand-written object, no deps.
bool settingsExportJson(const Settings& s, const char* path);
bool settingsImportJson(Settings& out, const char* path);

} // namespace toy
