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
};

bool settingsLoad(Settings& out);
bool settingsSave(const Settings& s);
void settingsReset(Settings& out);
const char* settingsPath();

// JSON export/import (v0.6 P2 #19) — simple hand-written object, no deps.
bool settingsExportJson(const Settings& s, const char* path);
bool settingsImportJson(Settings& out, const char* path);

} // namespace toy
