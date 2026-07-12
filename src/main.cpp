#include "app/audio.h"
#include "app/crash_handler.h"
#include "app/i18n.h"
#include "app/session_log.h"
#include "app/settings.h"
#include "game/cards.h"
#include "game/cosmetics.h"
#include "game/events.h"
#include "game/match.h"
#include "net/session.h"
#include "physics/table_scene.h"
#include "render/camera.h"
#include "render/draw3d.h"
#include "render/ui.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"

#include <cstdio>

namespace {

toy::Match g_match;
toy::NetSession g_session;
toy::LanDiscovery g_discovery; // v0.8 #85/#86
toy::TableScene g_scene;
toy::Draw3D g_draw;
toy::Camera g_camera;
toy::UiState g_ui;
toy::Settings g_settings;
bool g_recordedRecentHost = false; // v0.8 #110
bool g_dragging = false;
float g_lastMouseX = 0.0f;
float g_lastMouseY = 0.0f;
bool g_hasScene = false;
bool g_fullscreen = false;
// v0.7 juice + timer state
float g_turnTimerAccum = 0.0f;
int g_lastTurnForTimer = -1;
float g_slowMoTimer = 0.0f;  // #74
float g_punchTimer = 0.0f;   // #75
int g_lastParadeTurn = -1;   // #76
// v0.9 delta trackers (idempotent across sync rebroadcasts) + juice
float g_appTime = 0.0f;
int g_fxLastHp[toy::kMaxPlayers] = { -1, -1, -1, -1 };
int g_fxLastShield[toy::kMaxPlayers] = { 0, 0, 0, 0 };
bool g_fxLastElim[toy::kMaxPlayers] = {};
size_t g_fxLastDiscard = 0;
size_t g_fxLastHand = 0;
int g_fxLastActive = -1;
toy::EventKind g_fxLastWorld = toy::EventKind::None;
uint32_t g_fxWinMatchId = 0;
uint32_t g_fxTrackedMatchId = 0;
float g_flyTimer = 0.0f; // #129 intro fly-over
float g_cardsReloadPoll = 0.0f;

void rebuildScene()
{
	g_scene.create(g_match);
	g_session.clearSceneFlag();
	g_hasScene = true;
}

void destroyScene()
{
	g_scene.destroy();
	g_hasScene = false;
}

void saveSettingsIfDirty()
{
	if (!g_ui.settingsDirty) {
		return;
	}
	toy::uiCaptureSettings(g_ui, g_settings);
	toy::settingsSave(g_settings);
	g_ui.settingsDirty = false;
}

void applyCosmeticsToSeat0()
{
	toy::Cosmetics cos;
	cos.plastic = static_cast<toy::PlasticColor>(g_ui.plasticIndex);
	cos.towerSkin = static_cast<toy::TowerSkin>(g_ui.towerSkinIndex);
	cos.accessory = static_cast<toy::Accessory>(g_ui.accessoryIndex);
	g_session.requestSetCosmetics(g_match, cos);
}

void goMenu()
{
	toy::sessionLog("INFO", "leave to menu");
	g_session.shutdown(&g_match);
	destroyScene();
	g_match = toy::Match{};
	g_ui.screen = toy::AppScreen::Menu;
	g_ui.selectedHand = 0;
	g_ui.pauseOpen = false;
	g_ui.leaveConfirmOpen = false;
	g_ui.rematchVoteSent = false;
	saveSettingsIfDirty();
}

void startOfflineMatch()
{
	toy::sessionLog("INFO", "start offline match");
	toy::MatchConfig cfg = toy::uiMakeConfig(g_ui, 100u + g_match.syncGeneration);
	g_session.startOffline(g_match, cfg);
	for (int i = 1; i < g_match.config.playerCount; ++i) {
		g_match.players[static_cast<size_t>(i)].cosmetics = toy::defaultCosmeticsForSeat(i);
	}
	applyCosmeticsToSeat0();
	rebuildScene();
	g_ui.screen = toy::AppScreen::Match;
	g_ui.selectedHand = 0;
	g_ui.selectedTarget = 1;
	g_ui.matchCounted = false;
	g_ui.coachTipDismissed = false;
	g_ui.timelineIndex = -1;
	g_ui.lastMode = static_cast<int>(toy::LastMode::Offline);
	g_ui.settingsDirty = true;
	saveSettingsIfDirty();
}

void startHostLobby()
{
	toy::sessionLog("INFO", "start host lobby");
	toy::MatchConfig cfg = toy::uiMakeConfig(g_ui, 42u + g_match.syncGeneration);
	std::snprintf(g_ui.hostName, sizeof(g_ui.hostName), "%s", g_ui.playerName);
	if (!g_session.hostLobby(g_match, cfg, g_ui.hostName, static_cast<uint16_t>(g_ui.hostPort))) {
		toy::uiPushToast(g_ui, g_session.status(), 3.0f);
		toy::sessionLogf("ERROR", "host failed: %s", g_session.status());
		g_ui.screen = toy::AppScreen::Menu;
		return;
	}
	applyCosmeticsToSeat0();
	rebuildScene();
	g_ui.screen = toy::AppScreen::Lobby;
	g_ui.selectedHand = 0;
	g_ui.lastMode = static_cast<int>(toy::LastMode::Host);
	g_ui.settingsDirty = true;
	saveSettingsIfDirty();
}

void startJoin()
{
	toy::sessionLogf("INFO", "join %s:%d", g_ui.joinHost, g_ui.joinPort);
	g_recordedRecentHost = false;
	if (!g_session.joinLobby(g_match, g_ui.joinHost, static_cast<uint16_t>(g_ui.joinPort), g_ui.playerName)) {
		toy::uiPushToast(g_ui, g_session.status(), 3.0f);
		toy::sessionLogf("ERROR", "join failed: %s", g_session.status());
		g_ui.screen = toy::AppScreen::Menu;
		return;
	}
	g_ui.screen = toy::AppScreen::Lobby;
	g_ui.selectedHand = 0;
	g_ui.lastMode = static_cast<int>(toy::LastMode::Join);
	g_ui.settingsDirty = true;
	saveSettingsIfDirty();
}

// v0.8 #86: resolve a typed room code against live LAN beacons, then join.
void startJoinByCode()
{
	g_discovery.update();
	const toy::net::BeaconInfo* b = g_discovery.findByCode(g_ui.roomCodeInput);
	if (!b) {
		toy::uiPushToast(g_ui, "Room code not found on LAN — is the host on the same network?", 3.2f);
		toy::sessionLogf("WARN", "room code %s not found", g_ui.roomCodeInput);
		return;
	}
	if (b->version != toy::kProtocolVersion) {
		toy::uiPushToast(g_ui, "That host runs a different game version — update both sides.", 3.2f);
		return;
	}
	std::snprintf(g_ui.joinHost, sizeof(g_ui.joinHost), "%s", b->fromIp);
	g_ui.joinPort = b->tcpPort;
	startJoin();
}

void processUiIntents()
{
	const int intent = g_ui.selectedHand;
	if (intent >= 0) {
		return;
	}
	g_ui.selectedHand = 0;
	switch (intent) {
	case -2:
		startOfflineMatch();
		break;
	case -3:
		startHostLobby();
		break;
	case -4:
		startJoin();
		break;
	case -5:
		startOfflineMatch();
		break;
	case -6:
		goMenu();
		break;
	case -7:
		startJoinByCode();
		break;
	default:
		break;
	}
}

void applyFullscreenPref()
{
	const bool isFs = sapp_is_fullscreen();
	if (g_ui.fullscreen != isFs) {
		sapp_toggle_fullscreen();
	}
	g_fullscreen = g_ui.fullscreen;
}

void init()
{
	sg_desc desc = {};
	desc.environment = sglue_environment();
	desc.logger.func = slog_func;
	sg_setup(&desc);

	toy::sessionLogOpen();
	toy::crashHandlerInstall();
	toy::sessionLog("INFO", "app init");

	// v0.9 #130: procedural audio (fail-open — game runs silent if no device).
	if (!toy::audioInit()) {
		toy::sessionLog("WARN", "audio init failed — running silent");
	}
	// v0.9 #148: optional JSON card catalog next to the exe/working dir.
	if (toy::cardsLoadJsonFile("data/cards.json")) {
		toy::sessionLog("INFO", "card catalog loaded from data/cards.json");
	}

	toy::uiInit();
	g_draw.init();

	if (toy::settingsLoad(g_settings)) {
		toy::uiApplySettings(g_ui, g_settings);
		toy::sessionLogf("INFO", "settings loaded from %s", toy::settingsPath());
	} else {
		toy::sessionLog("INFO", "using default settings");
	}
	g_ui.screen = toy::AppScreen::Menu;
	g_fullscreen = g_ui.fullscreen;
	if (g_ui.fullscreen) {
		// toggle after first frame is safer; queue for now
		g_ui.wantApplyDisplay = true;
	}
}

void frame()
{
	const int w = sapp_width();
	const int h = sapp_height();
	const double dt = sapp_frame_duration();

	processUiIntents();

	if (g_ui.wantApplyDisplay) {
		g_ui.wantApplyDisplay = false;
		applyFullscreenPref();
		// Note: window size change requires restart on sokol; toast that.
		if (g_ui.windowWidth != w || g_ui.windowHeight != h) {
			toy::uiPushToast(g_ui, "Window size saved — restart app to apply resolution.", 3.0f);
		}
		g_ui.settingsDirty = true;
		saveSettingsIfDirty();
	}

	// v0.8 #85: LAN browser runs only while on the menu.
	if (g_ui.screen == toy::AppScreen::Menu) {
		if (!g_discovery.active()) {
			g_discovery.start();
		}
		g_discovery.update();
	} else if (g_discovery.active()) {
		g_discovery.stop();
	}

	g_session.update(g_match);

	// v0.8 #98: connection died (timeout / kick / version mismatch) → back to menu with reason.
	if (g_session.connectionLost()) {
		toy::uiPushToast(g_ui, g_session.connectionLostReason(), 3.5f);
		toy::sessionLogf("WARN", "connection lost: %s", g_session.connectionLostReason());
		g_session.clearConnectionLost();
		goMenu();
	}

	// v0.8 #110: remember a successfully joined host once per join.
	if (g_session.mode() == toy::AppMode::Client && g_session.isConnected() && !g_recordedRecentHost) {
		g_recordedRecentHost = true;
		const char* hostName = g_match.players[0].name[0] ? g_match.players[0].name : g_ui.joinHost;
		toy::settingsAddRecentHost(g_settings, hostName, g_ui.joinHost, g_ui.joinPort);
		for (int i = 0; i < toy::Settings::kRecentHostMax; ++i) {
			std::snprintf(g_ui.recentHosts[i], sizeof(g_ui.recentHosts[i]), "%s", g_settings.recentHosts[i]);
		}
		toy::settingsSave(g_settings);
	}

	// Client soft-fail toast from rejects (also handled in UI)
	if (g_session.lastError()[0] && g_ui.screen != toy::AppScreen::Match) {
		toy::uiPushToast(g_ui, g_session.lastError(), 2.8f);
		toy::sessionLogf("WARN", "%s", g_session.lastError());
		g_session.clearError();
	}

	if (g_session.sceneNeedsRebuild()) {
		rebuildScene();
	}

	if (g_ui.screen == toy::AppScreen::Lobby && g_match.phase == toy::Phase::Playing) {
		g_ui.screen = toy::AppScreen::Match;
		if (!g_hasScene) {
			rebuildScene();
		}
	}
	if (g_ui.screen == toy::AppScreen::Match && g_match.phase == toy::Phase::GameOver) {
		g_ui.screen = toy::AppScreen::Results;
		g_ui.timelineIndex = g_match.log.empty() ? -1 : static_cast<int>(g_match.log.size()) - 1;
		toy::sessionLogf("INFO", "game over winner=%d turns=%d", g_match.winner, g_match.turnNumber);
		// v0.9 #146: local unlock track — a win is "your seat won" (seat 0 in hotseat).
		const int mySeat = (g_session.mode() == toy::AppMode::Offline) ? 0 : g_session.localSeat();
		if (g_match.winner >= 0 && g_match.winner == mySeat) {
			++g_ui.wins;
			g_ui.settingsDirty = true;
		}
	}

	// v0.7 #56: turn timer (authority enforces; client display approximates).
	if (g_match.phase == toy::Phase::Playing && g_match.config.turnTimerSeconds > 0) {
		if (g_lastTurnForTimer != g_match.turnNumber) {
			g_lastTurnForTimer = g_match.turnNumber;
			g_turnTimerAccum = 0.0f;
		}
		g_turnTimerAccum += static_cast<float>(dt);
		g_ui.turnTimeLeft = static_cast<float>(g_match.config.turnTimerSeconds) - g_turnTimerAccum;
		if (g_ui.turnTimeLeft <= 0.0f && g_session.mode() != toy::AppMode::Client) {
			toy::sessionLogf("INFO", "turn timer expired — auto end turn %d", g_match.turnNumber);
			toy::uiPushToast(g_ui, "Turn timer expired — turn skipped.", 2.2f);
			g_session.hostForceEndTurn(g_match);
			g_turnTimerAccum = 0.0f;
		}
	} else {
		g_ui.turnTimeLeft = -1.0f;
		g_lastTurnForTimer = g_match.turnNumber;
		g_turnTimerAccum = 0.0f;
	}

	// v0.7 #74/#75 + v0.9 audio: state-delta FX (idempotent across snapshot rebroadcasts).
	g_appTime += static_cast<float>(dt);
	{
		// New match: rebase trackers silently (no sounds for the reset).
		if (g_match.matchId != g_fxTrackedMatchId) {
			g_fxTrackedMatchId = g_match.matchId;
			size_t discards = 0, hands = 0;
			for (int i = 0; i < g_match.config.playerCount; ++i) {
				const toy::Player& p = g_match.players[static_cast<size_t>(i)];
				g_fxLastHp[i] = p.towerHp;
				g_fxLastShield[i] = p.shieldTurns;
				g_fxLastElim[i] = p.eliminated;
				discards += p.discard.size();
				hands += p.hand.size();
			}
			g_fxLastDiscard = discards;
			g_fxLastHand = hands;
			g_fxLastActive = g_match.activePlayer;
			g_fxLastWorld = g_match.world.kind;
			// #129: intro fly-over on match start.
			if (g_match.phase == toy::Phase::Playing && !g_ui.reducedMotion) {
				g_flyTimer = 1.2f;
			}
		} else if (g_match.phase == toy::Phase::Playing || g_match.phase == toy::Phase::GameOver) {
			size_t discards = 0, hands = 0;
			for (int i = 0; i < g_match.config.playerCount; ++i) {
				const toy::Player& p = g_match.players[static_cast<size_t>(i)];
				if (g_fxLastHp[i] >= 0 && p.towerHp < g_fxLastHp[i]) {
					const int dmg = g_fxLastHp[i] - p.towerHp;
					toy::sfxPlay(dmg >= 5 ? toy::Sfx::BigDamage : toy::Sfx::Damage);
					if (dmg >= 5 && !g_ui.reducedMotion) {
						g_punchTimer = 0.18f; // #75
					}
				}
				if (p.shieldTurns > g_fxLastShield[i]) {
					toy::sfxPlay(toy::Sfx::Shield);
				}
				if (p.eliminated && !g_fxLastElim[i]) {
					toy::sfxPlay(toy::Sfx::TowerDestroy);
					if (!g_ui.reducedMotion) {
						g_slowMoTimer = 0.35f; // #74
					}
				}
				g_fxLastHp[i] = p.towerHp;
				g_fxLastShield[i] = p.shieldTurns;
				g_fxLastElim[i] = p.eliminated;
				discards += p.discard.size();
				hands += p.hand.size();
			}
			if (discards > g_fxLastDiscard) {
				toy::sfxPlay(toy::Sfx::CardPlay);
			}
			if (hands > g_fxLastHand) {
				toy::sfxPlay(toy::Sfx::Draw);
			}
			g_fxLastDiscard = discards;
			g_fxLastHand = hands;
			if (g_match.activePlayer != g_fxLastActive) {
				g_fxLastActive = g_match.activePlayer;
				if (g_match.phase == toy::Phase::Playing && g_session.canLocalAct(g_match)) {
					toy::sfxPlay(toy::Sfx::YourTurn); // #131 chime
				}
			}
			if (g_match.world.kind != g_fxLastWorld) {
				g_fxLastWorld = g_match.world.kind;
				toy::sfxEventStinger(g_match.world.kind); // #132
			}
			if (g_match.phase == toy::Phase::GameOver && g_fxWinMatchId != g_match.matchId) {
				g_fxWinMatchId = g_match.matchId;
				toy::sfxPlay(toy::Sfx::Win);
			}
		}
	}

	// v0.9 #134: live volumes; #133 music routing per screen.
	toy::audioSetVolumes(g_ui.masterVolume, g_ui.sfxVolume, g_ui.musicVolume);
	if (g_ui.screen == toy::AppScreen::Match || g_ui.screen == toy::AppScreen::Lobby) {
		toy::musicPlay(g_match.phase == toy::Phase::Playing ? toy::MusicTrack::Match : toy::MusicTrack::Menu);
	} else if (g_ui.screen == toy::AppScreen::Results) {
		toy::musicPlay(toy::MusicTrack::ResultsSting);
	} else {
		toy::musicPlay(toy::MusicTrack::Menu);
	}

#ifndef NDEBUG
	// v0.9 #149: Debug hot-reload of data/cards.json (poll every 2s, lobby/menu only).
	g_cardsReloadPoll -= static_cast<float>(dt);
	if (g_cardsReloadPoll <= 0.0f) {
		g_cardsReloadPoll = 2.0f;
		if (g_match.phase != toy::Phase::Playing && toy::cardsReloadIfChanged("data/cards.json")) {
			toy::uiPushToast(g_ui, "cards.json reloaded", 2.0f);
			toy::sessionLog("INFO", "cards.json hot-reloaded");
		}
	}
#endif

	const bool in3d = (g_ui.screen == toy::AppScreen::Lobby || g_ui.screen == toy::AppScreen::Match ||
					   g_ui.screen == toy::AppScreen::Results) &&
					  g_hasScene;

	if (in3d) {
		// P2 #36: reduced motion disables auto-orbit even if checkbox was on
		const bool orbit = g_ui.autoOrbit && !g_ui.reducedMotion;
		if (!g_dragging && orbit && !g_ui.showHowToPlay && !g_ui.pauseOpen) {
			g_camera.tickIdle(static_cast<float>(dt), true);
		}
		// #76: optional parade rest at each round start.
		if (g_match.config.paradeRest && g_match.phase == toy::Phase::Playing &&
			g_match.turnNumber != g_lastParadeTurn &&
			(g_match.turnNumber - 1) % g_match.config.playerCount == 0) {
			g_lastParadeTurn = g_match.turnNumber;
			g_scene.paradeRest(g_match);
		}
		g_scene.consumeImpulse(g_match);
		float stepDt = static_cast<float>(dt);
		if (g_slowMoTimer > 0.0f) {
			g_slowMoTimer -= static_cast<float>(dt);
			stepDt *= 0.3f; // #74 slow-mo
		}
		g_scene.step(stepDt);
		g_scene.syncFromMatch(g_match);
		g_scene.updateFx(g_match, static_cast<float>(dt)); // v0.9 particles/flashes (#125/#127/#128)
	}

	// #75 camera punch + #129 intro fly-over — temporary offsets restored after drawing.
	float punchOffset = 0.0f;
	if (g_punchTimer > 0.0f) {
		g_punchTimer -= static_cast<float>(dt);
		punchOffset = -0.22f * (g_punchTimer / 0.18f);
	}
	float flyOffset = 0.0f;
	float flyYaw = 0.0f;
	if (g_flyTimer > 0.0f) {
		g_flyTimer -= static_cast<float>(dt);
		const float t = g_flyTimer / 1.2f; // 1 → 0
		flyOffset = 3.6f * t * t;          // start high & far, ease in
		flyYaw = 1.6f * t;
	}
	g_camera.distance += punchOffset + flyOffset;
	g_camera.yaw += flyYaw;

	float cr = 0.07f, cg = 0.09f, cb = 0.14f;
	if (in3d) {
		toy::mapClearColor(g_match.config.mapId, cr, cg, cb);
	} else {
		cr = 0.06f;
		cg = 0.07f;
		cb = 0.12f;
	}
	g_draw.begin(w, h, g_camera, cr, cg, cb);
	if (in3d) {
		// v0.9 render juice (#121/#124/#128).
		toy::RenderFx fx;
		fx.time = g_appTime;
		fx.reducedMotion = g_ui.reducedMotion;
		fx.activeSeat = (g_match.phase == toy::Phase::Playing) ? g_match.activePlayer : -1;
		fx.winnerSeat = (g_match.phase == toy::Phase::GameOver) ? g_match.winner : -1;
		fx.outlineSeat = (g_ui.screen == toy::AppScreen::Match && g_match.phase == toy::Phase::Playing &&
						  g_ui.selectedTarget >= 0 && g_ui.selectedTarget < g_match.config.playerCount)
							 ? g_ui.selectedTarget
							 : -1;
		float sr, sg2, sb;
		toy::mapTableColor(g_match.config.mapId, sr, sg2, sb);
		fx.shadowR = sr * 0.45f;
		fx.shadowG = sg2 * 0.45f;
		fx.shadowB = sb * 0.45f;
		g_draw.drawScene(g_scene, fx);
	}
	sg_end_pass();

	{
		sg_pass pass = {};
		pass.action.colors[0].load_action = SG_LOADACTION_LOAD;
		pass.action.depth.load_action = SG_LOADACTION_LOAD;
		pass.swapchain = sglue_swapchain();
		sg_begin_pass(&pass);
		toy::uiBeginFrame(w, h, dt);
		toy::uiDraw(g_match, g_session, g_discovery, g_ui);
		toy::uiEndFrame();
		sg_end_pass();
	}
	sg_commit();
	g_camera.distance -= punchOffset + flyOffset; // restore after #75 punch / #129 fly-over
	g_camera.yaw -= flyYaw;

	if (g_ui.settingsDirty && (g_ui.screen == toy::AppScreen::Menu || g_ui.screen == toy::AppScreen::Settings)) {
		saveSettingsIfDirty();
	}
}

void event(const sapp_event* e)
{
	toy::uiEvent(e);
	// v0.9 #131: soft UI click whenever the cursor presses inside ImGui.
	if (e->type == SAPP_EVENTTYPE_MOUSE_DOWN && toy::uiWantsMouse()) {
		toy::sfxPlay(toy::Sfx::UiClick);
	}
	if (toy::uiWantsMouse() &&
		(e->type == SAPP_EVENTTYPE_MOUSE_DOWN || e->type == SAPP_EVENTTYPE_MOUSE_UP ||
		 e->type == SAPP_EVENTTYPE_MOUSE_MOVE || e->type == SAPP_EVENTTYPE_MOUSE_SCROLL)) {
		return;
	}

	switch (e->type) {
	case SAPP_EVENTTYPE_MOUSE_DOWN:
		if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
			g_dragging = true;
			g_lastMouseX = e->mouse_x;
			g_lastMouseY = e->mouse_y;
		}
		break;
	case SAPP_EVENTTYPE_MOUSE_UP:
		if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
			g_dragging = false;
		}
		break;
	case SAPP_EVENTTYPE_MOUSE_MOVE:
		if (g_dragging) {
			const float dx = e->mouse_x - g_lastMouseX;
			const float dy = e->mouse_y - g_lastMouseY;
			g_camera.orbit(dx * 0.005f, dy * 0.005f);
			g_lastMouseX = e->mouse_x;
			g_lastMouseY = e->mouse_y;
		}
		break;
	case SAPP_EVENTTYPE_MOUSE_SCROLL:
		g_camera.zoom(e->scroll_y * 0.15f);
		break;
	case SAPP_EVENTTYPE_KEY_DOWN:
		if (e->key_code == SAPP_KEYCODE_ESCAPE) {
			if (g_ui.screen == toy::AppScreen::Match) {
				if (g_ui.leaveConfirmOpen) {
					g_ui.leaveConfirmOpen = false;
				} else {
					g_ui.pauseOpen = !g_ui.pauseOpen;
				}
			} else if (g_ui.screen == toy::AppScreen::Settings || g_ui.screen == toy::AppScreen::Credits ||
					   g_ui.screen == toy::AppScreen::Codex || g_ui.showHowToPlay) {
				g_ui.showHowToPlay = false;
				g_ui.screen = toy::AppScreen::Menu;
			}
		} else if (e->key_code == SAPP_KEYCODE_F11) {
			g_ui.fullscreen = !g_ui.fullscreen;
			applyFullscreenPref();
			g_ui.settingsDirty = true;
		} else if (e->key_code == SAPP_KEYCODE_R) {
			if (g_ui.screen == toy::AppScreen::Match && g_session.mode() == toy::AppMode::Offline) {
				startOfflineMatch();
			} else if (g_ui.screen == toy::AppScreen::Match && g_session.mode() == toy::AppMode::Host) {
				g_session.hostRematch(g_match);
			}
		} else if (e->key_code == SAPP_KEYCODE_SPACE) {
			if (g_ui.screen == toy::AppScreen::Match && g_session.mode() == toy::AppMode::Offline &&
				g_match.phase == toy::Phase::Playing) {
				toy::autoPlayBest(g_match);
				toy::bumpSync(g_match);
			}
		} else if (e->key_code == SAPP_KEYCODE_H) {
			g_ui.showHowToPlay = !g_ui.showHowToPlay;
		} else if (e->key_code == SAPP_KEYCODE_ENTER || e->key_code == SAPP_KEYCODE_KP_ENTER) {
			if (g_ui.screen == toy::AppScreen::Match && g_match.phase == toy::Phase::Playing) {
				if (!g_session.requestPlayCard(g_match, g_ui.selectedHand, g_ui.selectedTarget)) {
					toy::sfxPlay(toy::Sfx::IllegalBuzz); // #131
					const char* err = g_session.lastError();
					if (err && err[0]) {
						toy::uiPushToast(g_ui, err, 2.8f);
						toy::sessionLogf("WARN", "play rejected: %s", err);
					}
				}
			}
		} else if (e->key_code >= SAPP_KEYCODE_1 && e->key_code <= SAPP_KEYCODE_8) {
			if (g_ui.screen != toy::AppScreen::Match) {
				break;
			}
			const int idx = static_cast<int>(e->key_code - SAPP_KEYCODE_1);
			const int handSeat =
				(g_session.mode() == toy::AppMode::Offline) ? g_match.activePlayer : g_session.localSeat();
			if (handSeat >= 0 && handSeat < g_match.config.playerCount) {
				const int handN = static_cast<int>(g_match.players[static_cast<size_t>(handSeat)].hand.size());
				if (idx < handN) {
					g_ui.selectedHand = idx;
				}
			}
		}
		break;
	default:
		break;
	}
}

void cleanup()
{
	saveSettingsIfDirty();
	toy::sessionLog("INFO", "app cleanup");
	toy::sessionLogClose();
	g_discovery.stop();
	g_session.shutdown(&g_match);
	destroyScene();
	toy::audioShutdown();
	toy::uiShutdown();
	sg_shutdown();
}

} // namespace

sapp_desc sokol_main(int /*argc*/, char* /*argv*/[])
{
	// Load settings early for window size / fullscreen / vsync
	toy::Settings early{};
	const bool hasSettings = toy::settingsLoad(early);

	sapp_desc d = {};
	d.init_cb = init;
	d.frame_cb = frame;
	d.event_cb = event;
	d.cleanup_cb = cleanup;
	d.width = hasSettings ? early.windowWidth : 1440;
	d.height = hasSettings ? early.windowHeight : 900;
	d.sample_count = 4;
	d.fullscreen = hasSettings ? early.fullscreen : false;
	d.swap_interval = (hasSettings && !early.vsync) ? 0 : 1;
	d.window_title = "Oyuncak Asker Masa Savasi — v0.9 Identity & Content";
	d.logger.func = slog_func;
	d.high_dpi = true;
	return d;
}
