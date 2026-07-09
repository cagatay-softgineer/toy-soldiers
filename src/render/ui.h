#pragma once

#include "game/match.h"
#include "net/session.h"

#include "sokol_app.h"

namespace toy {

struct UiState {
	int selectedHand = 0;
	int selectedTarget = 1;
	// Lobby / join form
	char hostName[kPlayerNameLen] = "Host";
	char playerName[kPlayerNameLen] = "Player";
	char joinHost[64] = "127.0.0.1";
	int joinPort = kDefaultPort;
	int hostPort = kDefaultPort;
	bool fillAI = true;
	int mapIndex = 0; // MapId
	bool eventsEnabled = true;
	// M4 cosmetics (applied to local seat)
	int plasticIndex = 0;
	int towerSkinIndex = 0;
	int accessoryIndex = 0;
	// M5 polish
	bool showHowToPlay = true;
	bool autoOrbit = true;
	uint32_t lastToastSync = 0;
	char toast[160] = {};
	float toastTimer = 0.0f;
};

void uiInit();
void uiShutdown();
void uiBeginFrame(int width, int height, double dt);
void uiDraw(Match& match, NetSession& session, UiState& ui);
void uiEndFrame();
void uiEvent(const sapp_event* e);
bool uiWantsMouse();
bool uiWantsKeyboard();

} // namespace toy
