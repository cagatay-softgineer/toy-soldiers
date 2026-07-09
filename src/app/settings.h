#pragma once

#include "game/types.h"

namespace toy {

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
	float masterVolume = 1.0f; // placeholder for v0.9 audio
	int plasticIndex = 0;
	int towerSkinIndex = 0;
	int accessoryIndex = 0;
	int uiScalePercent = 100; // 100 / 125 / 150
};

// Path: %APPDATA%/toy-soldiers/settings.ini (Windows) or ~/.config/toy-soldiers/settings.ini
bool settingsLoad(Settings& out);
bool settingsSave(const Settings& s);
const char* settingsPath();

} // namespace toy
