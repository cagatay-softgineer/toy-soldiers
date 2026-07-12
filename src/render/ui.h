#pragma once

#include "app/settings.h"
#include "game/match.h"
#include "net/session.h"

#include "sokol_app.h"

namespace toy {

// v0.6 app shell screens (+Codex in v0.7 #51)
enum class AppScreen : uint8_t {
	Menu,
	Settings,
	HowToPlay,
	Credits,
	Codex,   // offline card browser
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

	// --- v0.7 match setup ---
	int modeIndex = 0;       // GameMode
	int aiLevelIndex = 1;    // AiLevel (default Normal)
	int turnTimerIndex = 0;  // 0 off, 1=30s, 2=45s, 3=60s (#56)
	bool freeTargeting = true; // #70 lobby toggle
	bool paradeRest = false;   // #76
	// Deck builder lite (#47): defIds, 0 = unused.
	int banDef[2] = { 0, 0 };
	int extraDef[2] = { 0, 0 };

	// v0.7 HUD state
	char lastCardBanner[120] = {}; // #49 public last-played card
	float lastCardTimer = 0.0f;
	uint32_t lastCardSync = 0;
	bool showEventHistory = false; // #68
	float turnTimeLeft = -1.0f;    // #56 display (set by main; <0 = off)

	// --- v0.8 Reliable Party ---
	char roomCodeInput[8] = {};    // #86 join-by-code field
	char chatInput[200] = {};      // #81 lobby chat
	bool rematchVoteSent = false;  // #109 client-side
	char recentHosts[5][96] = {};  // #86/#110 mirrored from Settings ("name|ip|port")
	bool recentHostsDirty = false; // main persists when set

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
	// v0.9
	int wins = 0;            // #146 unlock track
	float sfxVolume = 0.8f;  // #134
	float musicVolume = 0.5f;
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
// dt used for banners/toasts; may request screen transitions via ui.screen.
// discovery: LAN lobby browser results for the menu (#85/#86), owned by main.
void uiDraw(Match& match, NetSession& session, LanDiscovery& discovery, UiState& ui);
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
