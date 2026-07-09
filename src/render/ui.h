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
	Credits,
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

	bool showHowToPlay = false;
	bool autoOrbit = true;
	float masterVolume = 1.0f;
	int uiScalePercent = 100;

	// P1 display / debug
	bool fullscreen = false;
	bool vsync = true;
	int windowWidth = 1440;
	int windowHeight = 900;
	bool showFps = false;
	bool showSyncGen = false;
	bool highContrast = false;
	int language = 0; // 0 EN 1 TR
	int lastMode = 0; // LastMode

	// P2
	bool reducedMotion = false;
	bool coachTips = true;
	int matchesCompleted = 0;
	int timelineIndex = -1; // scrubber into match.log (-1 = live/end)
	bool showTimeline = true;
	bool coachTipDismissed = false; // per-session; re-offer next match until 3 done

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
	bool leaveConfirmOpen = false;

	bool settingsDirty = false;
	bool wantFullscreenToggle = false; // consumed by main
	bool wantApplyDisplay = false;
	bool matchCounted = false; // results: count matchesCompleted once per game over
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
