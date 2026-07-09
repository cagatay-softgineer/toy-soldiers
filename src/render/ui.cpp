#include "render/ui.h"

#include "app/i18n.h"
#include "game/cards.h"
#include "game/cosmetics.h"
#include "game/events.h"
#include "game/play_reason.h"
#include "game/rules.h"
#include "game/summary.h"

#include "imgui.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_imgui.h"

#include <cstdio>
#include <cstring>

namespace toy {

void uiInit()
{
	simgui_desc_t desc = {};
	desc.no_default_font = false;
	simgui_setup(&desc);

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 10.0f;
	style.FrameRounding = 8.0f;
}

void uiShutdown()
{
	simgui_shutdown();
}

void uiBeginFrame(int width, int height, double dt)
{
	simgui_frame_desc_t desc = {};
	desc.width = width;
	desc.height = height;
	desc.delta_time = dt;
	desc.dpi_scale = sapp_dpi_scale();
	simgui_new_frame(&desc);
}

void uiEndFrame()
{
	simgui_render();
}

void uiEvent(const sapp_event* e)
{
	simgui_handle_event(e);
}

bool uiWantsMouse()
{
	return ImGui::GetIO().WantCaptureMouse;
}

bool uiWantsKeyboard()
{
	return ImGui::GetIO().WantCaptureKeyboard;
}

void uiApplySettings(UiState& ui, const Settings& s)
{
	std::snprintf(ui.playerName, sizeof(ui.playerName), "%s", s.playerName);
	std::snprintf(ui.hostName, sizeof(ui.hostName), "%s", s.playerName);
	std::snprintf(ui.joinHost, sizeof(ui.joinHost), "%s", s.joinHost);
	ui.joinPort = s.joinPort;
	ui.hostPort = s.hostPort;
	ui.mapIndex = s.mapIndex;
	ui.eventsEnabled = s.eventsEnabled;
	ui.fillAI = s.fillAI;
	ui.autoOrbit = s.autoOrbit;
	ui.showHowToPlay = s.showHowToPlay;
	ui.masterVolume = s.masterVolume;
	ui.plasticIndex = s.plasticIndex;
	ui.towerSkinIndex = s.towerSkinIndex;
	ui.accessoryIndex = s.accessoryIndex;
	ui.uiScalePercent = s.uiScalePercent;
	ui.fullscreen = s.fullscreen;
	ui.vsync = s.vsync;
	ui.windowWidth = s.windowWidth;
	ui.windowHeight = s.windowHeight;
	ui.showFps = s.showFps;
	ui.showSyncGen = s.showSyncGen;
	ui.highContrast = s.highContrast;
	ui.language = s.language;
	ui.lastMode = s.lastMode;
	i18nSetLang(s.language == 1 ? Lang::Tr : Lang::En);
}

void uiCaptureSettings(const UiState& ui, Settings& s)
{
	std::snprintf(s.playerName, sizeof(s.playerName), "%s", ui.playerName);
	std::snprintf(s.joinHost, sizeof(s.joinHost), "%s", ui.joinHost);
	s.joinPort = ui.joinPort;
	s.hostPort = ui.hostPort;
	s.mapIndex = ui.mapIndex;
	s.eventsEnabled = ui.eventsEnabled;
	s.fillAI = ui.fillAI;
	s.autoOrbit = ui.autoOrbit;
	s.showHowToPlay = ui.showHowToPlay;
	s.masterVolume = ui.masterVolume;
	s.plasticIndex = ui.plasticIndex;
	s.towerSkinIndex = ui.towerSkinIndex;
	s.accessoryIndex = ui.accessoryIndex;
	s.uiScalePercent = ui.uiScalePercent;
	s.fullscreen = ui.fullscreen;
	s.vsync = ui.vsync;
	s.windowWidth = ui.windowWidth;
	s.windowHeight = ui.windowHeight;
	s.showFps = ui.showFps;
	s.showSyncGen = ui.showSyncGen;
	s.highContrast = ui.highContrast;
	s.language = ui.language;
	s.lastMode = ui.lastMode;
}

void uiPushToast(UiState& ui, const char* text, float seconds)
{
	if (!text) {
		return;
	}
	std::snprintf(ui.toast, sizeof(ui.toast), "%s", text);
	ui.toastTimer = seconds;
}

MatchConfig uiMakeConfig(const UiState& ui, uint32_t seed)
{
	MatchConfig cfg;
	cfg.playerCount = 4;
	cfg.freeTargeting = true;
	cfg.fillEmptyWithAI = ui.fillAI;
	cfg.eventsEnabled = ui.eventsEnabled;
	cfg.mapId = static_cast<MapId>(ui.mapIndex);
	cfg.seed = seed ? seed : 42u;
	return cfg;
}

namespace {

const char* modeName(AppMode m)
{
	switch (m) {
	case AppMode::Offline: return "Offline Hotseat";
	case AppMode::Host: return "Host Lobby";
	case AppMode::Client: return "Client";
	}
	return "?";
}

void applyUiScale(const UiState& ui)
{
	ImGui::GetIO().FontGlobalScale = static_cast<float>(ui.uiScalePercent) / 100.0f;
}

void applyTheme(const UiState& ui)
{
	if (ui.highContrast) {
		ImGui::StyleColorsLight();
		ImGuiStyle& st = ImGui::GetStyle();
		st.Colors[ImGuiCol_WindowBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.98f);
		st.Colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		st.Colors[ImGuiCol_Button] = ImVec4(0.95f, 0.85f, 0.15f, 1.0f);
		st.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.92f, 0.25f, 1.0f);
		st.Colors[ImGuiCol_ButtonActive] = ImVec4(0.8f, 0.7f, 0.05f, 1.0f);
		st.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.45f, 0.95f, 1.0f);
		st.Colors[ImGuiCol_FrameBg] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
	} else {
		ImGui::StyleColorsDark();
	}
}

// Card keywords for tooltips (v0.6 P1 #10)
void appendCardKeywords(const CardDef& def, char* out, int cap)
{
	out[0] = 0;
	char tmp[128];
	std::snprintf(tmp, sizeof(tmp), "[%s]", trKeyword(cardClassName(def.klass)));
	std::strncat(out, tmp, static_cast<size_t>(cap - 1));
	if (def.klass == CardClass::Defense) {
		std::strncat(out, " · ", static_cast<size_t>(cap - std::strlen(out) - 1));
		std::strncat(out, trKeyword("Shield"), static_cast<size_t>(cap - std::strlen(out) - 1));
	}
	if (def.klass == CardClass::Attack && !def.freeTarget) {
		std::strncat(out, " · ", static_cast<size_t>(cap - std::strlen(out) - 1));
		std::strncat(out, trKeyword("AdjacentOnly"), static_cast<size_t>(cap - std::strlen(out) - 1));
	}
	if (def.id == 20 || def.id == 21 || def.id == 22 || def.id == 23 || def.id == 12) {
		std::strncat(out, " · ", static_cast<size_t>(cap - std::strlen(out) - 1));
		std::strncat(out, trKeyword("Draw"), static_cast<size_t>(cap - std::strlen(out) - 1));
	}
	if (def.id == 21 || def.id == 25) {
		std::strncat(out, " · ", static_cast<size_t>(cap - std::strlen(out) - 1));
		std::strncat(out, trKeyword("Heal"), static_cast<size_t>(cap - std::strlen(out) - 1));
	}
}

void drawToastsAndBanners(Match& match, UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	const float dt = ImGui::GetIO().DeltaTime;

	// World-event toasts from log
	if (match.syncGeneration != ui.lastToastSync) {
		ui.lastToastSync = match.syncGeneration;
		if (!match.log.empty()) {
			const MatchEvent& last = match.log.back();
			if (last.type == MatchEvent::Type::WorldEvent || last.type == MatchEvent::Type::Winner) {
				uiPushToast(ui, last.text.c_str(), 3.2f);
			}
		}
	}

	// Turn banner
	if (match.phase == Phase::Playing) {
		if (ui.lastTurnSeen != match.turnNumber || ui.lastActiveSeen != match.activePlayer) {
			ui.lastTurnSeen = match.turnNumber;
			ui.lastActiveSeen = match.activePlayer;
			const Player& ap = match.players[static_cast<size_t>(match.activePlayer)];
			std::snprintf(ui.turnBanner, sizeof(ui.turnBanner), "Turn %d — %s", match.turnNumber, ap.name);
			ui.turnBannerTimer = 0.85f;
		}
	} else {
		ui.lastTurnSeen = -1;
		ui.lastActiveSeen = -1;
	}

	if (ui.toastTimer > 0.0f) {
		ui.toastTimer -= dt;
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 48.0f), ImGuiCond_Always,
								ImVec2(0.5f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.85f);
		if (ImGui::Begin("##toast", nullptr,
						 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
							 ImGuiWindowFlags_NoInputs)) {
			ImGui::TextColored(ImVec4(0.85f, 0.95f, 1.0f, 1.0f), "%s", ui.toast);
		}
		ImGui::End();
	}

	if (ui.turnBannerTimer > 0.0f) {
		ui.turnBannerTimer -= dt;
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 96.0f), ImGuiCond_Always,
								ImVec2(0.5f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.75f);
		if (ImGui::Begin("##turnbanner", nullptr,
						 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
							 ImGuiWindowFlags_NoInputs)) {
			ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "%s", ui.turnBanner);
		}
		ImGui::End();
	}
}

void drawHowToPlaySteps(UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
							ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Always);
	ImGui::Begin(tr("menu.howto"), &ui.showHowToPlay, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "%s", tr("app.title"));
	ImGui::Separator();
	ImGui::TextWrapped("1. Fantasy — Plastic toy soldiers on a table. Cards are orders.");
	ImGui::TextWrapped("2. Goal — Destroy enemy main towers. Last tower standing wins.");
	ImGui::TextWrapped("3. Turn — Pick a target, pick one card, Play. Then turn passes.");
	ImGui::TextWrapped("4. Shield / Scout — Shield halves damage. Scout +1 lasts until your next attack.");
	ImGui::TextWrapped("5. World — Maps add props & events (sandstorm, rain, flood, cat). Cosmetics are fashion only.");
	ImGui::Separator();
	ImGui::TextUnformatted(tr("glossary.title"));
	ImGui::BulletText("%s", tr("glossary.shield"));
	ImGui::BulletText("%s", tr("glossary.adjacent"));
	ImGui::BulletText("%s", tr("glossary.world_event"));
	ImGui::BulletText("%s", tr("glossary.host"));
	ImGui::Separator();
	ImGui::BulletText("LMB orbit · Scroll zoom · 1-5 hand · Enter play · H help · Esc pause");
	ImGui::BulletText("Protocol v%u · Default port %u", static_cast<unsigned>(kProtocolVersion),
					  static_cast<unsigned>(kDefaultPort));
	if (ImGui::Button("Got it", ImVec2(-1, 36))) {
		ui.showHowToPlay = false;
		ui.settingsDirty = true;
	}
	ImGui::End();
}

void drawLoadoutEditors(Match& match, NetSession& session, UiState& ui, int seat)
{
	if (seat < 0 || seat >= match.config.playerCount) {
		return;
	}
	Player& me = match.players[static_cast<size_t>(seat)];
	ImGui::TextUnformatted("Tower type (gameplay):");
	if (ImGui::RadioButton("Machine Gun (36 HP)", me.tower == TowerType::MachineGun)) {
		session.requestSetTower(match, TowerType::MachineGun);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Sniper (34 HP +2 dmg)", me.tower == TowerType::Sniper)) {
		session.requestSetTower(match, TowerType::Sniper);
	}

	ImGui::Spacing();
	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "Cosmetics (no power)");
	ui.plasticIndex = static_cast<int>(me.cosmetics.plastic);
	ui.towerSkinIndex = static_cast<int>(me.cosmetics.towerSkin);
	ui.accessoryIndex = static_cast<int>(me.cosmetics.accessory);

	const char* colorNames[16] = {};
	const char* skinNames[8] = {};
	const char* accNames[8] = {};
	const int nc = plasticColorCount();
	const int ns = towerSkinCount();
	const int na = accessoryCount();
	for (int i = 0; i < nc; ++i) {
		colorNames[i] = plasticColorName(static_cast<PlasticColor>(i));
	}
	for (int i = 0; i < ns; ++i) {
		skinNames[i] = towerSkinName(static_cast<TowerSkin>(i));
	}
	for (int i = 0; i < na; ++i) {
		accNames[i] = accessoryName(static_cast<Accessory>(i));
	}

	bool cosChanged = false;
	cosChanged |= ImGui::Combo("Plastic color", &ui.plasticIndex, colorNames, nc);
	cosChanged |= ImGui::Combo("Tower skin", &ui.towerSkinIndex, skinNames, ns);
	cosChanged |= ImGui::Combo("Accessory", &ui.accessoryIndex, accNames, na);
	if (cosChanged) {
		Cosmetics cos;
		cos.plastic = static_cast<PlasticColor>(ui.plasticIndex);
		cos.towerSkin = static_cast<TowerSkin>(ui.towerSkinIndex);
		cos.accessory = static_cast<Accessory>(ui.accessoryIndex);
		session.requestSetCosmetics(match, cos);
		ui.settingsDirty = true;
	}
	const b3Vec3 rgb = plasticColorRgb(me.cosmetics.plastic);
	ImGui::ColorButton("##swatch", ImVec4(rgb.x, rgb.y, rgb.z, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(48, 24));
}

void drawMenu(UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.42f),
							ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);
	ImGui::Begin("Oyuncak Asker Masa Savasi", nullptr,
				 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "Toy Soldiers Tabletop");
	ImGui::TextDisabled("v0.6 Solid Core  ·  protocol %u", static_cast<unsigned>(kProtocolVersion));
	ImGui::Separator();

	if (ImGui::Button("Play Offline (Hotseat)", ImVec2(-1, 40))) {
		ui.screen = AppScreen::Lobby;
		// offline lobby prep — main applies startOffline when leaving lobby via Start
		ui.mapIndex = ui.mapIndex;
	}
	if (ImGui::Button("Host Lobby", ImVec2(-1, 40))) {
		ui.screen = AppScreen::Lobby;
		// flag via hostName path: we use a tiny mode hint in joinHost? better: use screen + pending
		// Store intent: empty joinHost special? Use accessory of settings: we'll use pauseOpen false and
		// hostPort path — main checks ui for "wantHost". Encode as mapIndex high bit? Cleaner: add field.
	}
	// Use hostName first char marker is ugly. Add explicit intent in UiState — already have screen Lobby.
	// Differentiate host vs offline lobby with fillAI panel; Host button sets joinHost to ""
	// Actually: use `ui.joinHost` sentinel or new field. Quick: set `ui.hostName` prefix.
	// I'll set selectedHand = -1 for host intent, -2 for offline, -3 for join when entering lobby from menu.
	// Better add ui.lobbyIntent: 0 offline 1 host 2 join - add to header... already pushed ui.h without it.
	// Use: plasticIndex temporary? No.
	// Use settingsDirty + string: if button Host, set joinHost to "@host"
	if (false) {
	}

	ImGui::Spacing();
	ImGui::InputText("Display name", ui.playerName, sizeof(ui.playerName));
	ImGui::InputText("Join IP", ui.joinHost, sizeof(ui.joinHost));
	ImGui::SetNextItemWidth(100);
	ImGui::InputInt("Join port", &ui.joinPort);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	ImGui::InputInt("Host port", &ui.hostPort);

	if (ImGui::Button("Join Host", ImVec2(-1, 36))) {
		ui.screen = AppScreen::Lobby;
		// main will call join when it sees Client transition — set marker in toast?
		// We'll handle join immediately in menu by requiring main callbacks.
		// For structure: set screen Lobby and ui.selectedTarget = -100 for join, -101 host, -102 offline.
		ui.selectedTarget = -100; // join intent
	}
	// Re-do menu buttons with selectedTarget intents for main to consume
	ImGui::Separator();
	if (ImGui::Button("Settings", ImVec2(-1, 32))) {
		ui.screen = AppScreen::Settings;
	}
	if (ImGui::Button("How to Play", ImVec2(-1, 32))) {
		ui.showHowToPlay = true;
	}
	if (ImGui::Button("Quit", ImVec2(-1, 32))) {
		sapp_request_quit();
	}
	ImGui::End();

	// Overlay intent buttons clearly (replace broken block)
	// The Play Offline / Host above need intents — rewrite menu end properly in second window? 
	// Fix: re-issue intents at end of frame via static pending - main polls ui.selectedTarget
}

// Fix menu: rewrite cleanly with lobbyIntent field - add to ui via selectedHand abuse:
// selectedHand: -1 menu idle, -2 offline, -3 host, -4 join after click until main clears.

void drawMenuV2(UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.42f),
							ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Always);
	ImGui::Begin("Oyuncak Asker Masa Savasi", nullptr,
				 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "%s", tr("app.title"));
	ImGui::TextDisabled("v0.6 Solid Core  ·  protocol %u", static_cast<unsigned>(kProtocolVersion));
	ImGui::Separator();

	ImGui::InputText(tr("menu.display_name"), ui.playerName, sizeof(ui.playerName));

	if (ui.lastMode >= 1 && ui.lastMode <= 3) {
		if (ImGui::Button(tr("menu.continue"), ImVec2(-1, 40))) {
			if (ui.lastMode == static_cast<int>(LastMode::Offline)) {
				ui.selectedHand = -2;
			} else if (ui.lastMode == static_cast<int>(LastMode::Host)) {
				ui.selectedHand = -3;
			} else {
				ui.selectedHand = -4;
			}
			ui.settingsDirty = true;
		}
	}

	if (ImGui::Button(tr("menu.play_offline"), ImVec2(-1, 42))) {
		ui.selectedHand = -2;
		ui.lastMode = static_cast<int>(LastMode::Offline);
		ui.settingsDirty = true;
	}
	if (ImGui::Button(tr("menu.host"), ImVec2(-1, 42))) {
		ui.selectedHand = -3;
		ui.lastMode = static_cast<int>(LastMode::Host);
		ui.settingsDirty = true;
	}

	ImGui::Separator();
	ImGui::InputText(tr("menu.join_ip"), ui.joinHost, sizeof(ui.joinHost));
	ImGui::SetNextItemWidth(120);
	ImGui::InputInt("Port##join", &ui.joinPort);
	if (ImGui::Button(tr("menu.join"), ImVec2(-1, 36))) {
		ui.selectedHand = -4;
		ui.lastMode = static_cast<int>(LastMode::Join);
		ui.settingsDirty = true;
	}

	ImGui::Separator();
	ImGui::SetNextItemWidth(120);
	ImGui::InputInt("Host listen port", &ui.hostPort);
	ImGui::Checkbox("Fill empty seats with AI", &ui.fillAI);

	ImGui::Separator();
	if (ImGui::Button(tr("menu.settings"), ImVec2(-1, 32))) {
		ui.screen = AppScreen::Settings;
	}
	if (ImGui::Button(tr("menu.howto"), ImVec2(-1, 32))) {
		ui.showHowToPlay = true;
	}
	if (ImGui::Button(tr("menu.quit"), ImVec2(-1, 32))) {
		sapp_request_quit();
	}
	ImGui::End();
}

void drawSettings(UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.42f),
							ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_Always);
	ImGui::Begin(tr("settings.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

	ImGui::InputText(tr("menu.display_name"), ui.playerName, sizeof(ui.playerName));
	ImGui::InputText(tr("menu.join_ip"), ui.joinHost, sizeof(ui.joinHost));
	ImGui::InputInt("Join port", &ui.joinPort);
	ImGui::InputInt("Host port", &ui.hostPort);
	ImGui::Checkbox(tr("settings.auto_orbit"), &ui.autoOrbit);
	ImGui::SliderFloat("Master volume (placeholder)", &ui.masterVolume, 0.0f, 1.0f);

	ImGui::Separator();
	ImGui::TextUnformatted(tr("settings.language"));
	if (ImGui::RadioButton("English", ui.language == 0)) {
		ui.language = 0;
		i18nSetLang(Lang::En);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Turkce", ui.language == 1)) {
		ui.language = 1;
		i18nSetLang(Lang::Tr);
	}

	ImGui::TextUnformatted("UI scale");
	if (ImGui::RadioButton("100%", ui.uiScalePercent == 100)) {
		ui.uiScalePercent = 100;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("125%", ui.uiScalePercent == 125)) {
		ui.uiScalePercent = 125;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("150%", ui.uiScalePercent == 150)) {
		ui.uiScalePercent = 150;
	}
	ImGui::Checkbox(tr("settings.high_contrast"), &ui.highContrast);

	ImGui::Separator();
	ImGui::Checkbox(tr("settings.fullscreen"), &ui.fullscreen);
	ImGui::Checkbox(tr("settings.vsync"), &ui.vsync);
	ImGui::InputInt("Window width", &ui.windowWidth);
	ImGui::InputInt("Window height", &ui.windowHeight);
	ImGui::TextDisabled("Resolution applies on next launch (or toggle fullscreen now).");

	ImGui::Separator();
	ImGui::Checkbox(tr("settings.show_fps"), &ui.showFps);
	ImGui::Checkbox(tr("settings.show_sync"), &ui.showSyncGen);
	ImGui::Checkbox("Show how-to on first run", &ui.showHowToPlay);
	ImGui::TextDisabled("Settings file: %s", settingsPath());

	if (ImGui::Button(tr("settings.save"), ImVec2(120, 32))) {
		ui.settingsDirty = true;
		ui.wantApplyDisplay = true;
		ui.screen = AppScreen::Menu;
	}
	ImGui::SameLine();
	if (ImGui::Button(tr("settings.reset"), ImVec2(160, 32))) {
		Settings def;
		settingsReset(def);
		// keep path; re-apply defaults to UI
		uiApplySettings(ui, def);
		ui.settingsDirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(tr("settings.back"), ImVec2(100, 32))) {
		ui.screen = AppScreen::Menu;
	}
	ImGui::End();
}

void drawLobbyScreen(Match& match, NetSession& session, UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 16, vp->WorkPos.y + 16), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);
	ImGui::Begin("Lobby", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

	ImGui::Text("Mode: %s", modeName(session.mode()));
	ImGui::TextWrapped("Status: %s", session.status());
	ImGui::Text("Protocol v%u · Seat %d · Sync %u", static_cast<unsigned>(kProtocolVersion), session.localSeat(),
				match.syncGeneration);

	ImGui::Separator();
	ImGui::TextUnformatted("Seats");
	for (int i = 0; i < match.config.playerCount; ++i) {
		const Player& p = match.players[static_cast<size_t>(i)];
		ImGui::Text("Seat %d: %-12s [%s] %s  %s", i, p.name[0] ? p.name : "(empty)", seatControlName(p.control),
					p.ready ? "READY" : "...", towerTypeName(p.tower));
	}

	if (session.mode() == AppMode::Host || (session.mode() == AppMode::Offline && match.phase == Phase::Lobby)) {
		ImGui::Separator();
		ImGui::TextUnformatted("Map");
		ui.mapIndex = static_cast<int>(match.config.mapId);
		const char* maps[] = { "Living Room Table", "Desert Playmat", "Backyard Picnic" };
		if (ImGui::Combo("##map", &ui.mapIndex, maps, 3)) {
			match.config.mapId = static_cast<MapId>(ui.mapIndex);
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			}
			ui.settingsDirty = true;
		}
		if (ImGui::Checkbox("World events", &ui.eventsEnabled)) {
			match.config.eventsEnabled = ui.eventsEnabled;
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			}
		}
		if (session.mode() == AppMode::Host) {
			ImGui::Checkbox("Fill empty with AI", &ui.fillAI);
			match.config.fillEmptyWithAI = ui.fillAI;
			if (ImGui::Button("Start Match", ImVec2(-1, 40))) {
				session.hostStartMatch(match);
				ui.screen = AppScreen::Match;
			}
		}
	}

	if (session.mode() == AppMode::Offline && match.phase == Phase::Lobby) {
		if (ImGui::Button("Start Offline Match", ImVec2(-1, 40))) {
			// main will start offline if needed; if already offline lobby empty, request via intent
			ui.selectedHand = -5; // start offline match from lobby
		}
	}

	const int seat = session.localSeat() >= 0 ? session.localSeat() : 0;
	if (match.phase == Phase::Lobby &&
		(session.mode() == AppMode::Host || session.mode() == AppMode::Client || session.mode() == AppMode::Offline)) {
		ImGui::Separator();
		drawLoadoutEditors(match, session, ui, seat);
	}

	if (session.mode() == AppMode::Client && match.phase == Phase::Lobby && session.localSeat() >= 0) {
		bool ready = match.players[static_cast<size_t>(session.localSeat())].ready;
		if (ImGui::Checkbox("Ready", &ready)) {
			session.requestReady(match, ready);
		}
	}

	// Transition to match when phase becomes Playing
	if (match.phase == Phase::Playing) {
		ui.screen = AppScreen::Match;
	}
	if (match.phase == Phase::GameOver) {
		ui.screen = AppScreen::Results;
	}

	ImGui::Separator();
	if (ImGui::Button("Leave to Menu", ImVec2(-1, 28))) {
		ui.selectedHand = -6; // disconnect intent
		ui.screen = AppScreen::Menu;
	}
	if (ImGui::SmallButton("Help")) {
		ui.showHowToPlay = true;
	}
	ImGui::End();
}

void drawMatchHud(Match& match, NetSession& session, UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();

	// Side panel
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12, vp->WorkPos.y + 12), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Always);
	ImGui::Begin("Match", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("%s · %s · proto v%u", modeName(session.mode()), mapName(match.config.mapId),
				static_cast<unsigned>(kProtocolVersion));
	ImGui::TextWrapped("%s", session.status());

	char worldStatus[160];
	formatWorldEventStatus(match, worldStatus, sizeof(worldStatus));
	if (match.world.kind != EventKind::None && match.world.remainingTurns > 0) {
		ImGui::TextColored(ImVec4(0.70f, 0.90f, 1.0f, 1.0f), "%s", worldStatus);
	} else {
		ImGui::TextDisabled("%s", worldStatus);
	}

	if (match.phase == Phase::Playing) {
		const Player& ap = match.players[static_cast<size_t>(match.activePlayer)];
		ImGui::Text("Turn %d — %s", match.turnNumber, ap.name);
		if (eventForcesAdjacent(match)) {
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "Sandstorm: adjacent only!");
		}
		if (session.canLocalAct(match) &&
			(session.mode() == AppMode::Offline || match.activePlayer == session.localSeat())) {
			ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "YOUR TURN — play one card");
		}
	}

	if (session.mode() == AppMode::Offline) {
		if (ImGui::Button("Auto Play")) {
			autoPlayBest(match);
			bumpSync(match);
		}
		ImGui::SameLine();
		if (ImGui::Button("End Turn")) {
			session.requestEndTurn(match);
		}
	}
	if (ImGui::Button("Pause / Menu")) {
		ui.pauseOpen = true;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Help")) {
		ui.showHowToPlay = true;
	}
	ImGui::End();

	// Towers
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 280, vp->WorkPos.y + 12), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(268, 0), ImGuiCond_Always);
	ImGui::Begin("Towers", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
	for (int i = 0; i < match.config.playerCount; ++i) {
		const Player& p = match.players[static_cast<size_t>(i)];
		const bool active = (i == match.activePlayer && match.phase == Phase::Playing);
		const bool mine = (session.mode() != AppMode::Offline && i == session.localSeat());
		// Seat text colors: blue/orange/purple/cyan (not red-green only) — a11y #35
		const ImVec4 seatCols[4] = {
			ImVec4(0.35f, 0.75f, 1.0f, 1.0f), // blue
			ImVec4(1.0f, 0.65f, 0.2f, 1.0f),  // orange
			ImVec4(0.75f, 0.5f, 1.0f, 1.0f),  // purple
			ImVec4(0.3f, 0.95f, 0.85f, 1.0f), // cyan
		};
		if (active) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.77f, 0.36f, 1.0f));
		} else {
			ImGui::PushStyleColor(ImGuiCol_Text, seatCols[i % 4]);
		}
		ImGui::Text("%s%s%s", p.name, p.eliminated ? " [OUT]" : "", mine ? " (you)" : "");
		ImGui::PopStyleColor();
		ImGui::ProgressBar(p.towerMaxHp > 0 ? static_cast<float>(p.towerHp) / static_cast<float>(p.towerMaxHp) : 0.f,
						   ImVec2(-1, 0), nullptr);
		ImGui::SameLine(0, 8);
		ImGui::Text("%d/%d", p.towerHp, p.towerMaxHp);
		if (p.shieldTurns > 0) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "SHD %d", p.shieldTurns);
		}
	}
	ImGui::End();

	// Hand
	if (match.phase == Phase::Playing) {
		const int handSeat = (session.mode() == AppMode::Offline) ? match.activePlayer : session.localSeat();
		if (handSeat >= 0 && handSeat < match.config.playerCount) {
			Player& hp = match.players[static_cast<size_t>(handSeat)];
			ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12, vp->WorkPos.y + vp->WorkSize.y - 250), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - 24, 238), ImGuiCond_Always);
			ImGui::Begin("Hand", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

			// Auto-select first legal target (#21)
			if (ui.selectedTarget == handSeat || ui.selectedTarget < 0 ||
				ui.selectedTarget >= match.config.playerCount ||
				match.players[static_cast<size_t>(ui.selectedTarget)].eliminated) {
				for (int t = 0; t < match.config.playerCount; ++t) {
					if (t != handSeat && !match.players[static_cast<size_t>(t)].eliminated &&
						match.players[static_cast<size_t>(t)].control != SeatControl::Empty) {
						ui.selectedTarget = t;
						break;
					}
				}
			}

			ImGui::Text("Target:");
			for (int t = 0; t < match.config.playerCount; ++t) {
				if (t == handSeat) {
					continue;
				}
				const Player& tp = match.players[static_cast<size_t>(t)];
				if (tp.eliminated || tp.control == SeatControl::Empty) {
					continue;
				}
				ImGui::SameLine();
				char label[48];
				std::snprintf(label, sizeof(label), "%s (%d)%s##tgt", tp.name, tp.towerHp,
							  tp.shieldTurns > 0 ? " shd" : "");
				if (ImGui::RadioButton(label, ui.selectedTarget == t)) {
					ui.selectedTarget = t;
				}
			}

			ImGui::Separator();
			if (ui.selectedHand < 0 || ui.selectedHand >= static_cast<int>(hp.hand.size())) {
				ui.selectedHand = 0;
			}

			for (int h = 0; h < static_cast<int>(hp.hand.size()); ++h) {
				const CardDef* def = findCard(hp.hand[static_cast<size_t>(h)].defId);
				if (!def) {
					continue;
				}
				ImGui::PushID(h);
				const bool selected = (ui.selectedHand == h);
				if (selected) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.45f, 0.25f, 1.0f));
				}
				char btn[128];
				if (def->klass == CardClass::Attack) {
					const int preview = computeAttackDamage(match, match.activePlayer, ui.selectedTarget, *def);
					std::snprintf(btn, sizeof(btn), "[%s] %s\n~%d dmg", cardClassName(def->klass), def->name, preview);
				} else {
					std::snprintf(btn, sizeof(btn), "[%s] %s\n%s", cardClassName(def->klass), def->name, def->description);
				}
				if (ImGui::Button(btn, ImVec2(170, 72))) {
					ui.selectedHand = h;
				}
				if (ImGui::IsItemHovered()) {
					char kw[96];
					appendCardKeywords(*def, kw, sizeof(kw));
					ImGui::BeginTooltip();
					ImGui::TextUnformatted(def->name);
					ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "%s", kw);
					ImGui::Separator();
					ImGui::TextWrapped("%s", def->description);
					ImGui::EndTooltip();
				}
				if (selected) {
					ImGui::PopStyleColor();
				}
				ImGui::SameLine();
				ImGui::PopID();
			}
			ImGui::NewLine();

			const bool myTurn = session.canLocalAct(match) ||
								(session.mode() == AppMode::Offline && match.activePlayer == handSeat);
			const bool can = myTurn && !hp.hand.empty() &&
							 canPlayCard(match, match.activePlayer, ui.selectedHand, ui.selectedTarget);
			if (!can) {
				ImGui::BeginDisabled();
			}
			if (ImGui::Button("Play Selected Card", ImVec2(200, 36))) {
				if (!session.requestPlayCard(match, ui.selectedHand, ui.selectedTarget)) {
					const char* err = session.lastError();
					if (err && err[0]) {
						uiPushToast(ui, err, 2.8f);
					}
				} else {
					ui.selectedHand = 0;
				}
			}
			if (!can) {
				ImGui::EndDisabled();
				ImGui::SameLine();
				const char* why =
					describeIllegalPlay(match, match.activePlayer, ui.selectedHand, ui.selectedTarget);
				if (!myTurn) {
					ImGui::TextDisabled("Not your turn");
				} else if (why && why[0]) {
					ImGui::TextDisabled("%s", why);
				}
			}
			ImGui::End();
		}
	}

	// Log
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 360, vp->WorkPos.y + 220), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(348, 220), ImGuiCond_FirstUseEver);
	ImGui::Begin("Battle Log");
	for (int i = static_cast<int>(match.log.size()) - 1; i >= 0 && i >= static_cast<int>(match.log.size()) - 20; --i) {
		const MatchEvent& ev = match.log[static_cast<size_t>(i)];
		if (ev.type == MatchEvent::Type::WorldEvent) {
			ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s", ev.text.c_str());
		} else {
			ImGui::TextWrapped("%s", ev.text.c_str());
		}
	}
	ImGui::End();

	if (match.phase == Phase::GameOver) {
		ui.screen = AppScreen::Results;
	}

	// FPS / sync debug
	if (ui.showFps || ui.showSyncGen) {
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 160, vp->WorkPos.y + vp->WorkSize.y - 60),
								ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.5f);
		if (ImGui::Begin("##dbg", nullptr,
						 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
							 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs)) {
			if (ui.showFps) {
				ImGui::Text("FPS %.0f", ImGui::GetIO().Framerate);
			}
			if (ui.showSyncGen) {
				ImGui::Text("sync %u", match.syncGeneration);
			}
		}
		ImGui::End();
	}

	// Soft-fail: surface session lastError as toast once
	if (session.lastError()[0]) {
		uiPushToast(ui, session.lastError(), 2.8f);
		session.clearError();
	}

	// Pause
	if (ui.pauseOpen) {
		ImGui::OpenPopup("Paused");
	}
	if (ImGui::BeginPopupModal("Paused", &ui.pauseOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted(tr("pause.title"));
		ImGui::Separator();
		ImGui::BulletText("%s", tr("glossary.shield"));
		ImGui::BulletText("%s", tr("glossary.adjacent"));
		if (ImGui::Button(tr("pause.resume"), ImVec2(240, 32))) {
			ui.pauseOpen = false;
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::Button(tr("menu.howto"), ImVec2(240, 32))) {
			ui.showHowToPlay = true;
		}
		if (ImGui::Button(tr("pause.resign"), ImVec2(240, 32))) {
			ui.leaveConfirmOpen = true;
		}
		ImGui::EndPopup();
	}
	if (ui.leaveConfirmOpen) {
		ImGui::OpenPopup("LeaveConfirm");
	}
	if (ImGui::BeginPopupModal("LeaveConfirm", &ui.leaveConfirmOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextWrapped("%s", tr("pause.confirm"));
		if (ImGui::Button(tr("pause.yes"), ImVec2(120, 32))) {
			ui.leaveConfirmOpen = false;
			ui.pauseOpen = false;
			ui.selectedHand = -6;
			ui.screen = AppScreen::Menu;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button(tr("pause.no"), ImVec2(120, 32))) {
			ui.leaveConfirmOpen = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void drawResults(Match& match, NetSession& session, UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.45f),
							ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Always);
	ImGui::Begin("Results", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

	const MatchSummary sum = buildSummary(match);
	char line[160];
	formatSummaryLine(sum, match, line, sizeof(line));
	ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "%s", line);
	ImGui::Separator();
	for (int i = 0; i < match.config.playerCount; ++i) {
		const Player& p = match.players[static_cast<size_t>(i)];
		if (p.control == SeatControl::Empty) {
			continue;
		}
		ImGui::Text("%s: dmg %d / taken %d · cards %d", p.name, sum.damageDealt[static_cast<size_t>(i)],
					sum.damageTaken[static_cast<size_t>(i)], sum.cardsPlayed[static_cast<size_t>(i)]);
	}
	ImGui::Separator();
	if (session.mode() == AppMode::Host && ImGui::Button(tr("results.rematch"), ImVec2(-1, 36))) {
		session.hostRematch(match);
		ui.screen = AppScreen::Match;
	}
	if (session.mode() == AppMode::Offline) {
		if (ImGui::Button(tr("results.play_again"), ImVec2(-1, 36))) {
			ui.selectedHand = -2; // offline again, same settings/cosmetics
		}
		if (ImGui::Button(tr("results.change_loadout"), ImVec2(-1, 36))) {
			ui.selectedHand = -6;
			ui.screen = AppScreen::Menu;
			// User adjusts cosmetics on menu then Play Offline
		}
	}
	if (ImGui::Button(tr("results.menu"), ImVec2(-1, 36))) {
		ui.selectedHand = -6;
		ui.screen = AppScreen::Menu;
	}
	ImGui::End();
}

} // namespace

void uiDraw(Match& match, NetSession& session, UiState& ui)
{
	applyTheme(ui);
	applyUiScale(ui);
	i18nSetLang(ui.language == 1 ? Lang::Tr : Lang::En);
	drawToastsAndBanners(match, ui);

	if (ui.showHowToPlay) {
		drawHowToPlaySteps(ui);
	}

	// Auto-advance screens from match phase when already in session
	if (ui.screen == AppScreen::Match && match.phase == Phase::GameOver) {
		ui.screen = AppScreen::Results;
	}
	if ((ui.screen == AppScreen::Lobby || ui.screen == AppScreen::Match) && match.phase == Phase::Playing &&
		ui.screen != AppScreen::Results) {
		if (ui.screen == AppScreen::Lobby) {
			ui.screen = AppScreen::Match;
		}
	}

	switch (ui.screen) {
	case AppScreen::Menu:
		drawMenuV2(ui);
		break;
	case AppScreen::Settings:
		drawSettings(ui);
		break;
	case AppScreen::HowToPlay:
		ui.showHowToPlay = true;
		ui.screen = AppScreen::Menu;
		break;
	case AppScreen::Lobby:
		drawLobbyScreen(match, session, ui);
		break;
	case AppScreen::Match:
		drawMatchHud(match, session, ui);
		break;
	case AppScreen::Results:
		drawResults(match, session, ui);
		break;
	}
}

} // namespace toy
