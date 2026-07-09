#pragma once

#include "app/settings.h"
#include "game/match.h"
#include "net/session.h"

#include "sokol_app.h"

namespace toy {

// v0.6 app shell screens
enum class AppScreen : uint8_t {
	Menu,
	Settings,
	HowToPlay,
	Lobby,   // host/client lobby or offline prep
	Match,   // playing
	Results, // game over
};

struct UiState {
	AppScreen screen = AppScreen::Menu;

	int selectedHand = 0;
	int selectedTarget = 1;
	// Lobby / join form
	char hostName[kPlayerNameLen] = "Host";
	char playerName[kPlayerNameLen] = "Player";
	char joinHost[64] = "127.0.0.1";
	int joinPort = kDefaultPort;
	int hostPort = kDefaultPort;
	bool fillAI = true;
	int mapIndex = 0;
	bool eventsEnabled = true;
	int plasticIndex = 0;
	int towerSkinIndex = 0;
	int accessoryIndex = 0;

	bool showHowToPlay = false; // full-screen help (first run or Help)
	bool autoOrbit = true;
	float masterVolume = 1.0f;
	int uiScalePercent = 100;

	uint32_t lastToastSync = 0;
	char toast[160] = {};
	float toastTimer = 0.0f;

	// Turn change banner
	int lastTurnSeen = -1;
	int lastActiveSeen = -1;
	float turnBannerTimer = 0.0f;
	char turnBanner[96] = {};

	// Pause overlay in match
	bool pauseOpen = false;

	bool settingsDirty = false;
};

void uiInit();
void uiShutdown();
void uiBeginFrame(int width, int height, double dt);
// dt used for banners/toasts; may request screen transitions via ui.screen
void uiDraw(Match& match, NetSession& session, UiState& ui);
void uiEndFrame();
void uiEvent(const sapp_event* e);
bool uiWantsMouse();
bool uiWantsKeyboard();

// Settings bridge
void uiApplySettings(UiState& ui, const Settings& s);
void uiCaptureSettings(const UiState& ui, Settings& s);
void uiPushToast(UiState& ui, const char* text, float seconds = 2.5f);

// Build MatchConfig from UI fields
MatchConfig uiMakeConfig(const UiState& ui, uint32_t seed);

} // namespace toy
