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
bool g_dragging = false;
float g_lastMouseX = 0.0f;
float g_lastMouseY = 0.0f;

void rebuildScene()
{
	g_scene.create(g_match);
	g_session.clearSceneFlag();
}

void init()
{
	sg_desc desc = {};
	desc.environment = sglue_environment();
	desc.logger.func = slog_func;
	sg_setup(&desc);

	toy::uiInit();
	g_draw.init();

	toy::MatchConfig cfg;
	cfg.playerCount = 4;
	cfg.freeTargeting = true;
	cfg.seed = 42;
	cfg.fillEmptyWithAI = true;
	g_session.startOffline(g_match, cfg);
	rebuildScene();
	g_ui.selectedTarget = 1;
}

void frame()
{
	const int w = sapp_width();
	const int h = sapp_height();
	const double dt = sapp_frame_duration();

	g_session.update(g_match);

	if (g_session.sceneNeedsRebuild()) {
		rebuildScene();
	}

	// M5: gentle auto-orbit when not dragging the camera.
	if (!g_dragging && g_ui.autoOrbit && !g_ui.showHowToPlay) {
		g_camera.tickIdle(static_cast<float>(dt), true);
	}

	g_scene.consumeImpulse(g_match);
	g_scene.step(static_cast<float>(dt));
	g_scene.syncFromMatch(g_match);

	// 3D pass (map-tinted clear color for room atmosphere)
	float cr = 0.07f, cg = 0.09f, cb = 0.14f;
	toy::mapClearColor(g_match.config.mapId, cr, cg, cb);
	g_draw.begin(w, h, g_camera, cr, cg, cb);
	g_draw.drawScene(g_scene);
	sg_end_pass();

	// ImGui overlay
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
		if (e->key_code == SAPP_KEYCODE_R) {
			if (g_session.mode() == toy::AppMode::Offline) {
				toy::MatchConfig cfg = g_match.config;
				cfg.seed = g_match.rng + 3u;
				g_session.startOffline(g_match, cfg);
				rebuildScene();
				g_ui.selectedHand = 0;
			} else if (g_session.mode() == toy::AppMode::Host) {
				g_session.hostRematch(g_match);
				if (g_session.sceneNeedsRebuild()) {
					rebuildScene();
				}
			}
		} else if (e->key_code == SAPP_KEYCODE_SPACE) {
			if (g_session.mode() == toy::AppMode::Offline && g_match.phase == toy::Phase::Playing) {
				toy::autoPlayBest(g_match);
				toy::bumpSync(g_match);
			}
		} else if (e->key_code == SAPP_KEYCODE_N && g_session.mode() == toy::AppMode::Offline) {
			toy::MatchConfig cfg = g_match.config;
			cfg.seed = g_match.rng + 7u;
			g_session.startOffline(g_match, cfg);
			rebuildScene();
		} else if (e->key_code == SAPP_KEYCODE_H) {
			g_ui.showHowToPlay = !g_ui.showHowToPlay;
		} else if (e->key_code == SAPP_KEYCODE_ENTER || e->key_code == SAPP_KEYCODE_KP_ENTER) {
			if (g_match.phase == toy::Phase::Playing) {
				g_session.requestPlayCard(g_match, g_ui.selectedHand, g_ui.selectedTarget);
			}
		} else if (e->key_code >= SAPP_KEYCODE_1 && e->key_code <= SAPP_KEYCODE_5) {
			const int idx = static_cast<int>(e->key_code - SAPP_KEYCODE_1);
			const int handSeat =
				(g_session.mode() == toy::AppMode::Offline) ? g_match.activePlayer : g_session.localSeat();
			if (handSeat >= 0 && handSeat < g_match.config.playerCount) {
				const int handN =
					static_cast<int>(g_match.players[static_cast<size_t>(handSeat)].hand.size());
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
	g_session.shutdown(&g_match);
	g_draw.shutdown();
	g_scene.destroy();
	toy::uiShutdown();
	sg_shutdown();
}

} // namespace

sapp_desc sokol_main(int /*argc*/, char* /*argv*/[])
{
	sapp_desc d = {};
	d.init_cb = init;
	d.frame_cb = frame;
	d.event_cb = event;
	d.cleanup_cb = cleanup;
	d.width = 1440;
	d.height = 900;
	d.sample_count = 4;
	d.window_title = "Oyuncak Asker Masa Savasi — M5 Polish (Box3D)";
	d.logger.func = slog_func;
	d.high_dpi = true;
	return d;
}
