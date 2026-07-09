#include "app/i18n.h"
#include "app/session_log.h"
#include "app/settings.h"
#include "game/cosmetics.h"
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
toy::TableScene g_scene;
toy::Draw3D g_draw;
toy::Camera g_camera;
toy::UiState g_ui;
toy::Settings g_settings;
bool g_dragging = false;
float g_lastMouseX = 0.0f;
float g_lastMouseY = 0.0f;
bool g_hasScene = false;
bool g_fullscreen = false;

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
	toy::sessionLog("INFO", "app init");

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

	g_session.update(g_match);

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
		toy::sessionLogf("INFO", "game over winner=%d turns=%d", g_match.winner, g_match.turnNumber);
	}

	const bool in3d = (g_ui.screen == toy::AppScreen::Lobby || g_ui.screen == toy::AppScreen::Match ||
					   g_ui.screen == toy::AppScreen::Results) &&
					  g_hasScene;

	if (in3d) {
		if (!g_dragging && g_ui.autoOrbit && !g_ui.showHowToPlay && !g_ui.pauseOpen) {
			g_camera.tickIdle(static_cast<float>(dt), true);
		}
		g_scene.consumeImpulse(g_match);
		g_scene.step(static_cast<float>(dt));
		g_scene.syncFromMatch(g_match);
	}

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
		g_draw.drawScene(g_scene);
	}
	sg_end_pass();

	{
		sg_pass pass = {};
		pass.action.colors[0].load_action = SG_LOADACTION_LOAD;
		pass.action.depth.load_action = SG_LOADACTION_LOAD;
		pass.swapchain = sglue_swapchain();
		sg_begin_pass(&pass);
		toy::uiBeginFrame(w, h, dt);
		toy::uiDraw(g_match, g_session, g_ui);
		toy::uiEndFrame();
		sg_end_pass();
	}
	sg_commit();

	if (g_ui.settingsDirty && (g_ui.screen == toy::AppScreen::Menu || g_ui.screen == toy::AppScreen::Settings)) {
		saveSettingsIfDirty();
	}
}

void event(const sapp_event* e)
{
	toy::uiEvent(e);
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
			} else if (g_ui.screen == toy::AppScreen::Settings || g_ui.showHowToPlay) {
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
					const char* err = g_session.lastError();
					if (err && err[0]) {
						toy::uiPushToast(g_ui, err, 2.8f);
						toy::sessionLogf("WARN", "play rejected: %s", err);
					}
				}
			}
		} else if (e->key_code >= SAPP_KEYCODE_1 && e->key_code <= SAPP_KEYCODE_5) {
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
	g_session.shutdown(&g_match);
	destroyScene();
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
	d.window_title = "Oyuncak Asker Masa Savasi — v0.6 Solid Core";
	d.logger.func = slog_func;
	d.high_dpi = true;
	return d;
}
