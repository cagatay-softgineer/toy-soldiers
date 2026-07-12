#include "render/ui.h"

#include "app/i18n.h"
#include "app/version.h"
#include "game/cards.h"
#include "game/cosmetics.h"
#include "game/events.h"
#include "game/play_reason.h"
#include "game/rules.h"
#include "game/summary.h"
#include "physics/table_scene.h" // v1.1 #141 felt dye names

#include "imgui.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_imgui.h"

#include <cstdio>
#include <cstdlib>
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
	ui.reducedMotion = s.reducedMotion;
	ui.coachTips = s.coachTips;
	ui.matchesCompleted = s.matchesCompleted;
	ui.wins = s.wins;
	ui.sfxVolume = s.sfxVolume;
	ui.musicVolume = s.musicVolume;
	ui.tutorialDone = s.tutorialDone;
	ui.hostedLobbies = s.hostedLobbies;
	ui.missionFlags = s.missionFlags;
	for (int i = 0; i < 7; ++i) {
		ui.mapPlays[i] = s.mapPlays[i];
	}
	ui.largeLogFont = s.largeLogFont;
	ui.feltDyeIndex = s.feltDyeIndex;
	for (int i = 0; i < Settings::kRecentHostMax; ++i) {
		std::snprintf(ui.recentHosts[i], sizeof(ui.recentHosts[i]), "%s", s.recentHosts[i]);
	}
	if (ui.reducedMotion) {
		ui.autoOrbit = false;
	}
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
	s.reducedMotion = ui.reducedMotion;
	s.coachTips = ui.coachTips;
	s.matchesCompleted = ui.matchesCompleted;
	s.wins = ui.wins;
	s.sfxVolume = ui.sfxVolume;
	s.musicVolume = ui.musicVolume;
	s.tutorialDone = ui.tutorialDone;
	s.hostedLobbies = ui.hostedLobbies;
	s.missionFlags = ui.missionFlags;
	for (int i = 0; i < 7; ++i) {
		s.mapPlays[i] = ui.mapPlays[i];
	}
	s.largeLogFont = ui.largeLogFont;
	s.feltDyeIndex = ui.feltDyeIndex;
	for (int i = 0; i < Settings::kRecentHostMax; ++i) {
		std::snprintf(s.recentHosts[i], sizeof(s.recentHosts[i]), "%s", ui.recentHosts[i]);
	}
}

void uiPushToast(UiState& ui, const char* text, float seconds)
{
	if (!text) {
		return;
	}
	std::snprintf(ui.toast, sizeof(ui.toast), "%s", text);
	// P2 #36: reduced motion shortens toasts
	float t = seconds;
	if (ui.reducedMotion) {
		t = (t > 1.2f) ? 1.2f : t * 0.55f;
		if (t < 0.6f) {
			t = 0.6f;
		}
	}
	ui.toastTimer = t;
}

int uiTurnTimerSeconds(int timerIndex)
{
	switch (timerIndex) {
	case 1: return 30;
	case 2: return 45;
	case 3: return 60;
	default: return 0;
	}
}

MatchConfig uiMakeConfig(const UiState& ui, uint32_t seed)
{
	MatchConfig cfg;
	cfg.playerCount = 4;
	cfg.freeTargeting = ui.freeTargeting; // #70
	cfg.fillEmptyWithAI = ui.fillAI;
	cfg.eventsEnabled = ui.eventsEnabled;
	cfg.mapId = static_cast<MapId>(ui.mapIndex);
	cfg.seed = seed ? seed : 42u;
	cfg.mode = static_cast<GameMode>(ui.modeIndex);
	cfg.aiLevel = static_cast<AiLevel>(ui.aiLevelIndex);
	cfg.turnTimerSeconds = uiTurnTimerSeconds(ui.turnTimerIndex);
	cfg.paradeRest = ui.paradeRest;
	applyModeToConfig(cfg);
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

// v1.0 #169: game-mode picker with Sandbox gated behind the tutorial.
bool drawModeCombo(const char* label, int& modeIndex, bool sandboxUnlocked)
{
	bool changed = false;
	if (modeIndex < 0 || modeIndex >= static_cast<int>(GameMode::Count)) {
		modeIndex = 0;
	}
	if (ImGui::BeginCombo(label, gameModeName(static_cast<GameMode>(modeIndex)))) {
		for (int i = 0; i < static_cast<int>(GameMode::Count); ++i) {
			const bool isSandbox = static_cast<GameMode>(i) == GameMode::Sandbox;
			const bool locked = isSandbox && !sandboxUnlocked;
			char item[96];
			std::snprintf(item, sizeof(item), "%s%s##mode%d", gameModeName(static_cast<GameMode>(i)),
						  locked ? tr("mode.locked_sandbox") : "", i);
			if (locked) {
				ImGui::BeginDisabled();
			}
			if (ImGui::Selectable(item, modeIndex == i)) {
				modeIndex = i;
				changed = true;
			}
			if (locked) {
				ImGui::EndDisabled();
			}
		}
		ImGui::EndCombo();
	}
	return changed;
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

// Card keywords for tooltips — driven by the v0.7 keyword flags (#42).
void appendCardKeywords(const CardDef& def, char* out, int cap)
{
	out[0] = 0;
	char tmp[128];
	std::snprintf(tmp, sizeof(tmp), "[%s]", trKeyword(cardClassName(def.klass)));
	std::strncat(out, tmp, static_cast<size_t>(cap - 1));
	struct KwName {
		uint16_t bit;
		const char* name;
	};
	const KwName kws[] = {
		{ KwShield, "Shield" }, { KwPierce, "Pierce" },   { KwDraw, "Draw" }, { KwHeal, "Heal" },
		{ KwAdjacentOnly, "AdjacentOnly" }, { KwAOE, "AOE" }, { KwEvent, "Event" },
	};
	for (const KwName& k : kws) {
		if (def.keywords & k.bit) {
			std::strncat(out, " · ", static_cast<size_t>(cap - std::strlen(out) - 1));
			std::strncat(out, trKeyword(k.name), static_cast<size_t>(cap - std::strlen(out) - 1));
		}
	}
}

// Rarity tint for card buttons (P2 #50 lite).
ImVec4 rarityColor(Rarity r, bool selected)
{
	if (selected) {
		return ImVec4(0.35f, 0.45f, 0.25f, 1.0f);
	}
	switch (r) {
	case Rarity::Rare: return ImVec4(0.18f, 0.30f, 0.48f, 1.0f);
	case Rarity::Epic: return ImVec4(0.35f, 0.22f, 0.48f, 1.0f);
	case Rarity::Legendary: return ImVec4(0.50f, 0.38f, 0.10f, 1.0f);
	default: return ImGui::GetStyle().Colors[ImGuiCol_Button];
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

	// #49: public "last played card" banner.
	if (match.syncGeneration != ui.lastCardSync) {
		ui.lastCardSync = match.syncGeneration;
		for (int i = static_cast<int>(match.log.size()) - 1; i >= 0; --i) {
			const MatchEvent& e = match.log[static_cast<size_t>(i)];
			if (e.type == MatchEvent::Type::TurnStart) {
				break; // only a play from the current resolution
			}
			if (e.type == MatchEvent::Type::CardPlayed) {
				std::snprintf(ui.lastCardBanner, sizeof(ui.lastCardBanner), "%s", e.text.c_str());
				ui.lastCardTimer = ui.reducedMotion ? 1.0f : 2.0f;
				break;
			}
		}
	}

	// Turn banner (shorter when reduced motion)
	if (match.phase == Phase::Playing) {
		if (ui.lastTurnSeen != match.turnNumber || ui.lastActiveSeen != match.activePlayer) {
			ui.lastTurnSeen = match.turnNumber;
			ui.lastActiveSeen = match.activePlayer;
			const Player& ap = match.players[static_cast<size_t>(match.activePlayer)];
			std::snprintf(ui.turnBanner, sizeof(ui.turnBanner), "Turn %d — %s", match.turnNumber, ap.name);
			ui.turnBannerTimer = ui.reducedMotion ? 0.4f : 0.85f;
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

	// #49 last-played card, below the turn banner — v1.1 #126: it now arcs up from
	// the hand area to its resting slot instead of popping in.
	if (ui.lastCardTimer > 0.0f && match.phase == Phase::Playing) {
		ui.lastCardTimer -= dt;
		float y = vp->WorkPos.y + 136.0f;
		float x = vp->WorkPos.x + vp->WorkSize.x * 0.5f;
		if (!ui.reducedMotion) {
			const float total = 2.0f;
			const float flight = 0.35f;
			const float elapsed = total - ui.lastCardTimer;
			if (elapsed < flight) {
				float t = elapsed / flight;
				t = 1.0f - (1.0f - t) * (1.0f - t); // ease-out
				const float startY = vp->WorkPos.y + vp->WorkSize.y - 270.0f; // hand area
				y = startY + (vp->WorkPos.y + 136.0f - startY) * t;
				x += (1.0f - t) * 40.0f; // slight sideways drift for the "arc" feel
			}
		}
		ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.7f);
		if (ImGui::Begin("##lastcard", nullptr,
						 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
							 ImGuiWindowFlags_NoInputs)) {
			ImGui::TextColored(ImVec4(0.75f, 0.9f, 0.75f, 1.0f), "%s", ui.lastCardBanner);
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
	ImGui::TextWrapped("2. Goal — Destroy enemy main towers. Last tower standing wins (2v2: last team).");
	ImGui::TextWrapped("3. Turn — Pick a target, pick one card, Play. Then turn passes.");
	ImGui::TextWrapped("4. Shield / Scout — Shield halves damage (Pierce ignores it). Scout +1 lasts until your next attack.");
	ImGui::TextWrapped("5. Towers — MG tank, Sniper +2, Shield Bearer starts shielded (-1 atk), Sapper +1 opener.");
	ImGui::TextWrapped("6. World — Maps add events (sandstorm, rain, flood, cat, dog, blackout); Event cards cast your own. Cosmetics are fashion only.");
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
	// v0.7 #38: tower pick with stats comparison, #40 passive tooltips.
	ImGui::TextUnformatted(tr("lobby.tower_type"));
	if (ImGui::BeginTable("##towers", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Tower");
		ImGui::TableSetupColumn("HP");
		ImGui::TableSetupColumn("Passive");
		ImGui::TableHeadersRow();
		for (int t = 0; t < static_cast<int>(TowerType::Count); ++t) {
			const TowerType tt = static_cast<TowerType>(t);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(t);
			if (ImGui::RadioButton(towerTypeName(tt), me.tower == tt)) {
				session.requestSetTower(match, tt);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", towerPassive(tt));
			}
			ImGui::PopID();
			ImGui::TableNextColumn();
			const int hp = (match.config.mode == GameMode::QuickDuel) ? 24 : towerBaseHp(tt);
			ImGui::Text("%d", hp);
			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", towerPassive(tt));
		}
		ImGui::EndTable();
	}

	// v0.7 #47: deck builder lite — ban 2 / add 2 from sideboard.
	if (ImGui::TreeNode("Deck tweaks (ban 2 / add 2)")) {
		const auto catalog = cardCatalog();
		const int catN = static_cast<int>(catalog.size());
		auto defCombo = [&](const char* label, int& defId) {
			char preview[64];
			const CardDef* cur = findCard(defId);
			std::snprintf(preview, sizeof(preview), "%s", cur ? cur->name : "None");
			bool changed = false;
			if (ImGui::BeginCombo(label, preview)) {
				if (ImGui::Selectable("None", defId == 0)) {
					defId = 0;
					changed = true;
				}
				for (int i = 0; i < catN; ++i) {
					char item[96];
					std::snprintf(item, sizeof(item), "%s [%s]", catalog[static_cast<size_t>(i)].name,
								  cardClassName(catalog[static_cast<size_t>(i)].klass));
					if (ImGui::Selectable(item, defId == catalog[static_cast<size_t>(i)].id)) {
						defId = catalog[static_cast<size_t>(i)].id;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			return changed;
		};
		int ban0 = me.bannedDefs[0], ban1 = me.bannedDefs[1];
		int ex0 = me.extraDefs[0], ex1 = me.extraDefs[1];
		bool modChanged = false;
		modChanged |= defCombo("Ban 1", ban0);
		modChanged |= defCombo("Ban 2", ban1);
		modChanged |= defCombo("Add 1", ex0);
		modChanged |= defCombo("Add 2", ex1);
		ImGui::TextDisabled("Bans remove all copies; adds put one extra copy in. Max 2 Event cards per deck.");
		if (modChanged) {
			const int banned[2] = { ban0, ban1 };
			const int extras[2] = { ex0, ex1 };
			session.requestSetDeckMods(match, banned, extras);
		}
		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "Cosmetics (no power)");
	ui.plasticIndex = static_cast<int>(me.cosmetics.plastic);
	ui.towerSkinIndex = static_cast<int>(me.cosmetics.towerSkin);
	ui.accessoryIndex = static_cast<int>(me.cosmetics.accessory);

	// v0.9 #146: locked items visible but unselectable, with the unlock rule shown.
	bool cosChanged = false;
	auto lockedCombo = [&](const char* label, int& index, int count, auto nameFn, auto unlockedFn, auto ruleFn) {
		char preview[96];
		std::snprintf(preview, sizeof(preview), "%s", nameFn(index));
		bool changed = false;
		if (ImGui::BeginCombo(label, preview)) {
			for (int i = 0; i < count; ++i) {
				const bool unlocked = unlockedFn(i, ui.matchesCompleted, ui.wins);
				char item[128];
				if (unlocked) {
					std::snprintf(item, sizeof(item), "%s##%s%d", nameFn(i), label, i);
				} else {
					char req[48];
					ruleFn(i, req, static_cast<int>(sizeof(req)));
					std::snprintf(item, sizeof(item), "%s  (locked: %s)##%s%d", nameFn(i), req, label, i);
				}
				if (!unlocked) {
					ImGui::BeginDisabled();
				}
				if (ImGui::Selectable(item, index == i)) {
					index = i;
					changed = true;
				}
				if (!unlocked) {
					ImGui::EndDisabled();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	};

	cosChanged |= lockedCombo(
		"Plastic color", ui.plasticIndex, plasticColorCount(),
		[](int i) { return plasticColorName(static_cast<PlasticColor>(i)); },
		[](int i, int m, int w) { return plasticUnlocked(static_cast<PlasticColor>(i), m, w); },
		[](int i, char* out, int cap) { plasticUnlockText(static_cast<PlasticColor>(i), out, cap); });
	cosChanged |= lockedCombo(
		"Tower skin", ui.towerSkinIndex, towerSkinCount(),
		[](int i) { return towerSkinName(static_cast<TowerSkin>(i)); },
		[](int i, int m, int w) { return towerSkinUnlocked(static_cast<TowerSkin>(i), m, w); },
		[](int i, char* out, int cap) { towerSkinUnlockText(static_cast<TowerSkin>(i), out, cap); });
	cosChanged |= lockedCombo(
		"Accessory", ui.accessoryIndex, accessoryCount(),
		[](int i) { return accessoryName(static_cast<Accessory>(i)); },
		[](int i, int m, int w) { return accessoryUnlocked(static_cast<Accessory>(i), m, w); },
		[](int i, char* out, int cap) { accessoryUnlockText(static_cast<Accessory>(i), out, cap); });

	// v0.9 #145: one-click themed sets.
	if (ImGui::TreeNode("Cosmetic sets")) {
		for (int si = 0; si < cosmeticSetCount(); ++si) {
			const CosmeticSet& set = cosmeticSet(si);
			const bool unlocked = cosmeticSetUnlocked(si, ui.matchesCompleted, ui.wins);
			char label[96];
			std::snprintf(label, sizeof(label), "%s%s##set%d", set.name, unlocked ? "" : " (locked)", si);
			if (!unlocked) {
				ImGui::BeginDisabled();
			}
			if (ImGui::SmallButton(label)) {
				ui.plasticIndex = static_cast<int>(set.plastic);
				ui.towerSkinIndex = static_cast<int>(set.towerSkin);
				ui.accessoryIndex = static_cast<int>(set.accessory);
				cosChanged = true;
			}
			if (!unlocked) {
				ImGui::EndDisabled();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s + %s + %s", plasticColorName(set.plastic), towerSkinName(set.towerSkin),
								  accessoryName(set.accessory));
			}
		}
		ImGui::TreePop();
	}

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

// Menu intents via selectedHand: -1 idle, -2 offline, -3 host, -4 join,
// -7 join-by-room-code (main consumes).
void drawMenuV2(LanDiscovery& discovery, UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.42f),
							ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Always);
	ImGui::Begin("Oyuncak Asker Masa Savasi", nullptr,
				 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "%s", tr("app.title"));
	ImGui::TextDisabled("v%s %s  ·  protocol %u", kVersionString, kVersionCodename, static_cast<unsigned>(kProtocolVersion));
	ImGui::Separator();

	ImGui::InputText(tr("menu.display_name"), ui.playerName, sizeof(ui.playerName));

	// v1.0 #168: tutorial front and center for new players.
	if (ImGui::Button(tr("menu.tutorial"), ImVec2(-1, ui.tutorialDone ? 32.0f : 42.0f))) {
		ui.selectedHand = -8;
	}
	if (!ui.tutorialDone) {
		ImGui::TextDisabled("%s", tr("menu.tutorial_hint"));
	}
	ImGui::Separator();

	// v0.7 match setup: mode (#52-#57), AI level (#58), turn timer (#56), targeting (#70).
	if (ImGui::CollapsingHeader(tr("menu.match_setup"), ImGuiTreeNodeFlags_DefaultOpen)) {
		if (drawModeCombo(tr("menu.mode"), ui.modeIndex, ui.tutorialDone)) {
			ui.settingsDirty = true;
		}
		ImGui::TextDisabled("%s", gameModeBlurb(static_cast<GameMode>(ui.modeIndex)));
		const char* aiLevels[] = { "Easy", "Normal", "Hard" };
		ImGui::Combo(tr("menu.ai_level"), &ui.aiLevelIndex, aiLevels, 3);
		const char* timers[] = { "Off", "30s", "45s", "60s" };
		ImGui::Combo(tr("menu.turn_timer"), &ui.turnTimerIndex, timers, 4);
		ImGui::Checkbox(tr("menu.free_targeting"), &ui.freeTargeting);
		ImGui::Checkbox(tr("menu.parade_rest"), &ui.paradeRest);
	}
	ImGui::Separator();

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
	// v0.8 #86: join by room code (LAN beacon lookup happens in main).
	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "%s", tr("menu.join_lan"));
	ImGui::SetNextItemWidth(120);
	ImGui::InputText(tr("menu.room_code"), ui.roomCodeInput, sizeof(ui.roomCodeInput),
					 ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
	ImGui::SameLine();
	if (ImGui::Button(tr("menu.join_code"), ImVec2(-1, 0))) {
		ui.selectedHand = -7;
		ui.lastMode = static_cast<int>(LastMode::Join);
		ui.settingsDirty = true;
	}

	// #85: live LAN lobby list from UDP beacons.
	const auto& found = discovery.entries();
	if (found.empty()) {
		ImGui::TextDisabled("%s", tr("menu.lan_scanning"));
	} else {
		for (size_t i = 0; i < found.size(); ++i) {
			const net::BeaconInfo& b = found[i].info;
			char label[160];
			std::snprintf(label, sizeof(label), "%s  [%s]  %s:%u  %u/%u%s##lan%d", b.hostName, b.code, b.fromIp,
						  static_cast<unsigned>(b.tcpPort), b.seatsTaken, b.seatsTotal,
						  b.version != kProtocolVersion ? "  (version mismatch)" : "", static_cast<int>(i));
			const bool incompatible = b.version != kProtocolVersion;
			if (incompatible) {
				ImGui::BeginDisabled();
			}
			if (ImGui::Button(label, ImVec2(-1, 28))) {
				std::snprintf(ui.joinHost, sizeof(ui.joinHost), "%s", b.fromIp);
				ui.joinPort = b.tcpPort;
				ui.selectedHand = -4;
				ui.lastMode = static_cast<int>(LastMode::Join);
				ui.settingsDirty = true;
			}
			if (incompatible) {
				ImGui::EndDisabled();
			}
		}
	}

	// #110: recent hosts ("name|ip|port").
	bool anyRecent = false;
	for (const auto& rh : ui.recentHosts) {
		if (rh[0]) {
			anyRecent = true;
			break;
		}
	}
	if (anyRecent && ImGui::TreeNode(tr("menu.recent_hosts"))) {
		for (int i = 0; i < 5; ++i) {
			if (!ui.recentHosts[i][0]) {
				continue;
			}
			char name[64] = {}, ip[64] = {};
			int port = kDefaultPort;
			char tmp[96];
			std::snprintf(tmp, sizeof(tmp), "%s", ui.recentHosts[i]);
			char* p1 = std::strchr(tmp, '|');
			if (p1) {
				*p1 = 0;
				char* p2 = std::strchr(p1 + 1, '|');
				if (p2) {
					*p2 = 0;
					std::snprintf(name, sizeof(name), "%s", tmp);
					std::snprintf(ip, sizeof(ip), "%s", p1 + 1);
					port = std::atoi(p2 + 1);
				}
			}
			if (!ip[0]) {
				continue;
			}
			char label[128];
			std::snprintf(label, sizeof(label), "%s — %s:%d##recent%d", name, ip, port, i);
			if (ImGui::SmallButton(label)) {
				std::snprintf(ui.joinHost, sizeof(ui.joinHost), "%s", ip);
				ui.joinPort = port;
				ui.selectedHand = -4;
				ui.lastMode = static_cast<int>(LastMode::Join);
				ui.settingsDirty = true;
			}
		}
		ImGui::TreePop();
	}

	// #87: manual IP tucked behind an advanced collapse.
	if (ImGui::TreeNode(tr("menu.manual_ip"))) {
		ImGui::InputText(tr("menu.join_ip"), ui.joinHost, sizeof(ui.joinHost));
		ImGui::SetNextItemWidth(120);
		ImGui::InputInt("Port##join", &ui.joinPort);
		if (ImGui::Button(tr("menu.join"), ImVec2(-1, 32))) {
			ui.selectedHand = -4;
			ui.lastMode = static_cast<int>(LastMode::Join);
			ui.settingsDirty = true;
		}
		ImGui::TreePop();
	}

	ImGui::Separator();
	ImGui::SetNextItemWidth(120);
	ImGui::InputInt("Host listen port", &ui.hostPort);
	ImGui::Checkbox(tr("lobby.fill_ai"), &ui.fillAI);

	ImGui::Separator();
	if (ImGui::Button(tr("menu.settings"), ImVec2(-1, 32))) {
		ui.screen = AppScreen::Settings;
	}
	if (ImGui::Button(tr("menu.howto"), ImVec2(-1, 32))) {
		ui.showHowToPlay = true;
	}
	if (ImGui::Button(tr("menu.codex"), ImVec2(-1, 32))) {
		ui.screen = AppScreen::Codex;
	}
	if (ImGui::Button(tr("menu.profile"), ImVec2(-1, 32))) {
		ui.screen = AppScreen::Profile;
	}
	if (ImGui::Button(tr("menu.credits"), ImVec2(-1, 32))) {
		ui.screen = AppScreen::Credits;
	}
	if (ImGui::Button(tr("menu.quit"), ImVec2(-1, 32))) {
		sapp_request_quit();
	}
	ImGui::End();
}

void drawCredits(UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.42f),
							ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Always);
	ImGui::Begin(tr("credits.title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "%s", tr("app.title"));
	ImGui::TextDisabled("v%s %s", kVersionString, kVersionCodename);
	ImGui::Separator();
	ImGui::TextUnformatted("Game");
	ImGui::BulletText("Design & code — preunec / cagatay-softgineer");
	ImGui::BulletText("Concept board — Toy Soldiers tabletop jam brief");
	ImGui::Spacing();
	ImGui::TextUnformatted("Engine & libraries");
	ImGui::BulletText("Box3D — Erin Catto (physics)");
	ImGui::BulletText("sokol — Andre Weissflog (app / gfx / imgui glue)");
	ImGui::BulletText("Dear ImGui — Omar Cornut");
	ImGui::Spacing();
	ImGui::TextUnformatted("Thanks");
	ImGui::BulletText("Playtesters and jam friends");
	ImGui::BulletText("Open-source community");
	ImGui::Separator();
	ImGui::TextWrapped("Minidumps (if any): %%APPDATA%%/toy-soldiers/crashes/");
	ImGui::TextDisabled("Protocol v%u · Default port %u", static_cast<unsigned>(kProtocolVersion),
						static_cast<unsigned>(kDefaultPort));
	// v1.0 #166/#195: manual update check (fail-open — just opens the project page).
	ImGui::TextDisabled("%s", kSupportContact);
	if (ImGui::Button(tr("credits.updates"), ImVec2(-1, 30))) {
		ui.wantOpenProjectUrl = true;
	}
	if (ImGui::Button(tr("credits.back"), ImVec2(-1, 36))) {
		ui.screen = AppScreen::Menu;
	}
	ImGui::End();
}

// v0.7 P2 #51: offline card codex browser.
void drawCodex(UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
							ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(680, vp->WorkSize.y * 0.85f), ImGuiCond_Always);
	ImGui::Begin(tr("codex.title"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "%s", tr("codex.header"));
	ImGui::TextDisabled("%d cards · keywords: Shield, Pierce, Draw, Heal, AdjacentOnly, AOE, Event",
						static_cast<int>(cardCatalog().size()));
	ImGui::Separator();

	if (ImGui::BeginTable("##codex", 5,
						  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
							  ImGuiTableFlags_SizingStretchProp,
						  ImVec2(0, -44))) {
		ImGui::TableSetupColumn("Card");
		ImGui::TableSetupColumn("Class");
		ImGui::TableSetupColumn("Rarity");
		ImGui::TableSetupColumn("Keywords");
		ImGui::TableSetupColumn("Effect");
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();
		for (const CardDef& def : cardCatalog()) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			const ImVec4 col = rarityColor(def.rarity, false);
			if (def.rarity != Rarity::Common) {
				ImGui::TextColored(ImVec4(col.x + 0.35f, col.y + 0.35f, col.z + 0.35f, 1.0f), "%s", def.name);
			} else {
				ImGui::TextUnformatted(def.name);
			}
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(trKeyword(cardClassName(def.klass)));
			ImGui::TableNextColumn();
			const char* rarities[] = { "Common", "Rare", "Epic", "Legendary" };
			ImGui::TextUnformatted(rarities[static_cast<size_t>(def.rarity)]);
			ImGui::TableNextColumn();
			char kw[96];
			appendCardKeywords(def, kw, sizeof(kw));
			ImGui::TextUnformatted(kw);
			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", cardDescription(def));
		}
		ImGui::EndTable();
	}
	if (ImGui::Button(tr("credits.back"), ImVec2(-1, 36))) {
		ui.screen = AppScreen::Menu;
	}
	ImGui::End();
}

// v1.0 #170/#171/#185/#186: local profile — stats, badges, missions, match history.
void drawProfile(UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
							ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(560, vp->WorkSize.y * 0.85f), ImGuiCond_Always);
	ImGui::Begin(tr("profile.title"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

	// #185: headline stats.
	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "%s", ui.playerName);
	int favMap = 0;
	int totalPlays = 0;
	for (int i = 0; i < 7; ++i) {
		totalPlays += ui.mapPlays[i];
		if (ui.mapPlays[i] > ui.mapPlays[favMap]) {
			favMap = i;
		}
	}
	ImGui::Text("%s: %d   ·   %s: %d", tr("profile.matches"), ui.matchesCompleted, tr("profile.wins"), ui.wins);
	if (totalPlays > 0) {
		ImGui::Text("%s: %s (%d)", tr("profile.fav_map"), mapName(static_cast<MapId>(favMap)), ui.mapPlays[favMap]);
	} else {
		ImGui::TextDisabled("%s: —", tr("profile.fav_map"));
	}
	ImGui::TextDisabled("%s %d · %s", tr("profile.hosted"), ui.hostedLobbies,
						ui.tutorialDone ? tr("profile.tut_done") : tr("profile.tut_todo"));

	// #171: local badges.
	ImGui::Separator();
	ImGui::TextUnformatted(tr("profile.badges"));
	struct Badge {
		const char* key;
		bool earned;
	};
	const Badge badges[] = {
		{ "badge.graduate", ui.tutorialDone },
		{ "badge.first_win", ui.wins >= 1 },
		{ "badge.veteran", ui.wins >= 10 },
		{ "badge.regular", ui.matchesCompleted >= 10 },
		{ "badge.devoted", ui.matchesCompleted >= 25 },
		{ "badge.party_host", ui.hostedLobbies >= 5 },
	};
	for (const Badge& b : badges) {
		if (b.earned) {
			ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "[*] %s", tr(b.key));
		} else {
			ImGui::TextDisabled("[ ] %s", tr(b.key));
		}
	}

	// #170: challenge missions (bitmask).
	ImGui::Separator();
	ImGui::TextUnformatted(tr("profile.missions"));
	const char* missionKeys[] = { "mission.storm", "mission.medic", "mission.untouchable" };
	for (int i = 0; i < 3; ++i) {
		if (ui.missionFlags & (1 << i)) {
			ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[x] %s", tr(missionKeys[i]));
		} else {
			ImGui::TextDisabled("[ ] %s", tr(missionKeys[i]));
		}
	}

	// #186: last-20 match history (re-read when entering the screen).
	ImGui::Separator();
	ImGui::TextUnformatted(tr("profile.history"));
	static char lines[20][160];
	static int lineCount = -1;
	static AppScreen lastScreen = AppScreen::Menu;
	if (lastScreen != AppScreen::Profile) {
		lineCount = historyReadLast(lines, 20);
	}
	lastScreen = ui.screen;
	if (ImGui::BeginChild("##hist", ImVec2(0, -44))) {
		if (lineCount <= 0) {
			ImGui::TextDisabled("%s", tr("profile.no_history"));
		}
		for (int i = 0; i < lineCount; ++i) {
			ImGui::TextWrapped("%s", lines[i]);
		}
	}
	ImGui::EndChild();

	if (ImGui::Button(tr("credits.back"), ImVec2(-1, 36))) {
		ui.screen = AppScreen::Menu;
	}
	ImGui::End();
}

const char* coachTipKey(int matchesCompleted)
{
	if (matchesCompleted <= 0) {
		return "coach.tip0";
	}
	if (matchesCompleted == 1) {
		return "coach.tip1";
	}
	return "coach.tip2";
}

void drawCoachTip(UiState& ui)
{
	if (!ui.coachTips || ui.coachTipDismissed || ui.matchesCompleted >= 3) {
		return;
	}
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 16, vp->WorkPos.y + vp->WorkSize.y - 140), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.92f);
	if (ImGui::Begin("##coach", nullptr,
					 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
						 ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "Coach");
		ImGui::TextWrapped("%s", tr(coachTipKey(ui.matchesCompleted)));
		if (ImGui::Button(tr("coach.dismiss"), ImVec2(120, 28))) {
			ui.coachTipDismissed = true;
		}
	}
	ImGui::End();
}

void drawTimelineScrubber(Match& match, UiState& ui)
{
	if (!ui.showTimeline || match.log.empty()) {
		return;
	}
	const int n = static_cast<int>(match.log.size());
	if (ui.timelineIndex < 0 || ui.timelineIndex >= n) {
		ui.timelineIndex = n - 1;
	}
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y - 12),
							ImGuiCond_Always, ImVec2(0.5f, 1.0f));
	ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.9f);
	if (ImGui::Begin(tr("timeline.title"), &ui.showTimeline, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("%s %d / %d", tr("timeline.scrub"), ui.timelineIndex + 1, n);
		if (ImGui::SliderInt("##scrub", &ui.timelineIndex, 0, n - 1)) {
			// read-only navigate
		}
		const MatchEvent& ev = match.log[static_cast<size_t>(ui.timelineIndex)];
		ImGui::Separator();
		if (ev.type == MatchEvent::Type::WorldEvent) {
			ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s", ev.text.c_str());
		} else if (ev.type == MatchEvent::Type::Winner) {
			ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "%s", ev.text.c_str());
		} else {
			ImGui::TextWrapped("%s", ev.text.c_str());
		}
		if (ImGui::SmallButton("<<")) {
			ui.timelineIndex = 0;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("<") && ui.timelineIndex > 0) {
			--ui.timelineIndex;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(">") && ui.timelineIndex < n - 1) {
			++ui.timelineIndex;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(">>")) {
			ui.timelineIndex = n - 1;
		}
	}
	ImGui::End();
}

// JSON path next to settings.ini
void settingsJsonPath(char* out, int cap)
{
	const char* sp = settingsPath();
	std::snprintf(out, static_cast<size_t>(cap), "%s", sp);
	// replace .ini with .json or append
	char* dot = std::strrchr(out, '.');
	if (dot && (std::strcmp(dot, ".ini") == 0)) {
		std::snprintf(dot, static_cast<size_t>(cap - static_cast<int>(dot - out)), ".json");
	} else {
		std::strncat(out, ".json", static_cast<size_t>(cap - std::strlen(out) - 1));
	}
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
	if (ImGui::Checkbox(tr("settings.auto_orbit"), &ui.autoOrbit)) {
		ui.settingsDirty = true;
	}
	if (ui.reducedMotion && ui.autoOrbit) {
		ui.autoOrbit = false;
	}
	// v0.9 #134: real per-category volumes.
	if (ImGui::SliderFloat(tr("settings.master_vol"), &ui.masterVolume, 0.0f, 1.0f)) {
		ui.settingsDirty = true;
	}
	if (ImGui::SliderFloat(tr("settings.sfx_vol"), &ui.sfxVolume, 0.0f, 1.0f)) {
		ui.settingsDirty = true;
	}
	if (ImGui::SliderFloat(tr("settings.music_vol"), &ui.musicVolume, 0.0f, 1.0f)) {
		ui.settingsDirty = true;
	}
	if (ImGui::Checkbox(tr("settings.reduced_motion"), &ui.reducedMotion)) {
		if (ui.reducedMotion) {
			ui.autoOrbit = false;
		}
		ui.settingsDirty = true;
	}
	if (ImGui::Checkbox(tr("settings.coach_tips"), &ui.coachTips)) {
		ui.settingsDirty = true;
	}
	ImGui::TextDisabled("Matches completed (local): %d", ui.matchesCompleted);

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
	// v1.1 #112: Twitch-friendly battle log.
	if (ImGui::Checkbox(tr("settings.large_log"), &ui.largeLogFont)) {
		ui.settingsDirty = true;
	}
	// v1.1 #141: local felt dye (display only — other players see their own choice).
	{
		const int n = TableScene::feltDyeCount();
		if (ui.feltDyeIndex < 0 || ui.feltDyeIndex >= n) {
			ui.feltDyeIndex = 0;
		}
		if (ImGui::BeginCombo(tr("settings.felt_dye"), TableScene::feltDyeName(ui.feltDyeIndex))) {
			for (int i = 0; i < n; ++i) {
				if (ImGui::Selectable(TableScene::feltDyeName(i), ui.feltDyeIndex == i)) {
					ui.feltDyeIndex = i;
					ui.settingsDirty = true;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::TextDisabled("%s", tr("settings.felt_dye_hint"));
	}

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

	ImGui::Separator();
	char jsonPath[512];
	settingsJsonPath(jsonPath, static_cast<int>(sizeof(jsonPath)));
	ImGui::TextDisabled("JSON: %s", jsonPath);
	if (ImGui::Button(tr("settings.export_json"), ImVec2(-1, 28))) {
		Settings snap;
		uiCaptureSettings(ui, snap);
		if (settingsExportJson(snap, jsonPath)) {
			uiPushToast(ui, "Settings exported to JSON", 2.5f);
		} else {
			uiPushToast(ui, "Export failed", 2.5f);
		}
	}
	if (ImGui::Button(tr("settings.import_json"), ImVec2(-1, 28))) {
		Settings loaded;
		settingsReset(loaded);
		if (settingsImportJson(loaded, jsonPath)) {
			uiApplySettings(ui, loaded);
			ui.settingsDirty = true;
			ui.wantApplyDisplay = true;
			uiPushToast(ui, "Settings imported from JSON", 2.5f);
		} else {
			uiPushToast(ui, "Import failed (file missing?)", 2.5f);
		}
	}

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
	ImGui::Text("Game: %s — %s", gameModeName(match.config.mode), gameModeBlurb(match.config.mode));
	ImGui::TextWrapped("Status: %s", session.status());
	ImGui::Text("Protocol v%u · Seat %d · Sync %u", static_cast<unsigned>(kProtocolVersion), session.localSeat(),
				match.syncGeneration);

	// v0.8 #82/#85: room code + copy join info (host only).
	if (session.mode() == AppMode::Host && session.roomCode()[0]) {
		ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "Room code: %s  (port %u)", session.roomCode(),
						   static_cast<unsigned>(session.listenPort()));
		ImGui::SameLine();
		if (ImGui::SmallButton(tr("lobby.copy"))) {
			char clip[96];
			std::snprintf(clip, sizeof(clip), "Room %s — port %u", session.roomCode(),
						  static_cast<unsigned>(session.listenPort()));
			ImGui::SetClipboardText(clip);
		}
	}
	// #83: client-side connection freshness.
	if (session.mode() == AppMode::Client) {
		const float age = session.hostPacketAge();
		if (age >= 0.0f) {
			const bool stale = age > 3.0f;
			ImGui::TextColored(stale ? ImVec4(1.0f, 0.5f, 0.4f, 1.0f) : ImVec4(0.5f, 0.8f, 0.5f, 1.0f),
							   "Last host packet: %.1fs ago", age);
		}
	}

	ImGui::Separator();
	// v0.8 #78: seat cards with ready checkmarks (+ping/kick for host, #79/#83).
	const bool amHost = session.mode() == AppMode::Host;
	const int seatCols = amHost ? 6 : 5;
	if (ImGui::BeginTable("##seats", seatCols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Seat", ImGuiTableColumnFlags_WidthFixed, 40.0f);
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthFixed, 62.0f);
		ImGui::TableSetupColumn("Tower");
		ImGui::TableSetupColumn("Ready", ImGuiTableColumnFlags_WidthFixed, 48.0f);
		if (amHost) {
			ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		}
		ImGui::TableHeadersRow();
		for (int i = 0; i < match.config.playerCount; ++i) {
			const Player& p = match.players[static_cast<size_t>(i)];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%d%s", i, i == session.localSeat() ? "*" : "");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(p.control == SeatControl::Empty ? "(empty)" : p.name);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(seatControlName(p.control));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(p.control == SeatControl::Empty ? "-" : towerTypeName(p.tower));
			ImGui::TableNextColumn();
			if (p.control == SeatControl::Empty) {
				ImGui::TextDisabled("-");
			} else if (p.ready) {
				ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[x]");
			} else {
				ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[ ]");
			}
			if (amHost) {
				ImGui::TableNextColumn();
				if (p.control == SeatControl::HumanRemote) {
					const int ping = session.seatPingMs(i);
					if (ping >= 0) {
						ImGui::Text("%d ms", ping);
					} else {
						ImGui::TextDisabled("…");
					}
					ImGui::SameLine();
					char kick[24];
					std::snprintf(kick, sizeof(kick), "%s##k%d", tr("lobby.kick"), i);
					if (ImGui::SmallButton(kick)) {
						session.hostKick(match, i);
					}
				} else {
					ImGui::TextDisabled("-");
				}
			}
		}
		ImGui::EndTable();
	}
	// #79: open/close lobby.
	if (amHost && match.phase == Phase::Lobby) {
		bool locked = match.lobbyLocked;
		if (ImGui::Checkbox(tr("lobby.locked"), &locked)) {
			session.hostSetLobbyLocked(match, locked);
		}
	}

	if (session.mode() == AppMode::Host || (session.mode() == AppMode::Offline && match.phase == Phase::Lobby)) {
		ImGui::Separator();
		ImGui::TextUnformatted("Map");
		ui.mapIndex = static_cast<int>(match.config.mapId);
		// v0.9 #136-#139: all 7 rooms.
		const char* maps[static_cast<int>(MapId::Count)];
		for (int mi = 0; mi < static_cast<int>(MapId::Count); ++mi) {
			maps[mi] = mapName(static_cast<MapId>(mi));
		}
		if (ImGui::Combo("##map", &ui.mapIndex, maps, static_cast<int>(MapId::Count))) {
			match.config.mapId = static_cast<MapId>(ui.mapIndex);
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			}
			ui.settingsDirty = true;
		}
		// #140/#156: loading blurb with recommended events.
		ImGui::TextDisabled("%s", mapBlurb(match.config.mapId));
		if (ImGui::Checkbox(tr("lobby.world_events"), &ui.eventsEnabled)) {
			match.config.eventsEnabled = ui.eventsEnabled;
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			}
		}
		// v0.7: host-side game setup in lobby (Sandbox gated by tutorial, #169).
		ui.modeIndex = static_cast<int>(match.config.mode);
		if (drawModeCombo(tr("menu.mode"), ui.modeIndex, ui.tutorialDone)) {
			match.config.mode = static_cast<GameMode>(ui.modeIndex);
			applyModeToConfig(match.config);
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			}
		}
		ui.aiLevelIndex = static_cast<int>(match.config.aiLevel);
		const char* aiLevels[] = { "Easy", "Normal", "Hard" };
		if (ImGui::Combo(tr("menu.ai_level"), &ui.aiLevelIndex, aiLevels, 3)) {
			match.config.aiLevel = static_cast<AiLevel>(ui.aiLevelIndex);
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			}
		}
		// #70: targeting rule toggle.
		bool freeT = match.config.freeTargeting;
		if (ImGui::Checkbox(tr("menu.free_targeting"), &freeT)) {
			match.config.freeTargeting = freeT;
			ui.freeTargeting = freeT;
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			}
		}
		int timerIdx = (match.config.turnTimerSeconds == 30)   ? 1
					   : (match.config.turnTimerSeconds == 45) ? 2
					   : (match.config.turnTimerSeconds == 60) ? 3
															   : 0;
		const char* timers[] = { "Off", "30s", "45s", "60s" };
		if (ImGui::Combo(tr("menu.turn_timer"), &timerIdx, timers, 4)) {
			match.config.turnTimerSeconds = uiTurnTimerSeconds(timerIdx);
			ui.turnTimerIndex = timerIdx;
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			}
		}
		bool parade = match.config.paradeRest;
		if (ImGui::Checkbox(tr("menu.parade_rest"), &parade)) {
			match.config.paradeRest = parade;
			ui.paradeRest = parade;
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			}
		}
		if (session.mode() == AppMode::Host) {
			ImGui::Checkbox(tr("lobby.fill_ai"), &ui.fillAI);
			match.config.fillEmptyWithAI = ui.fillAI;
			// #80: every seated human must be ready before start.
			bool allReady = true;
			for (int i = 0; i < match.config.playerCount; ++i) {
				const Player& p = match.players[static_cast<size_t>(i)];
				if ((p.control == SeatControl::HumanLocal || p.control == SeatControl::HumanRemote) && !p.ready) {
					allReady = false;
					break;
				}
			}
			if (!allReady) {
				ImGui::BeginDisabled();
			}
			if (ImGui::Button(tr("lobby.start"), ImVec2(-1, 40))) {
				if (session.hostStartMatch(match)) {
					ui.screen = AppScreen::Match;
				} else if (session.lastError()[0]) {
					uiPushToast(ui, session.lastError(), 2.8f);
					session.clearError();
				}
			}
			if (!allReady) {
				ImGui::EndDisabled();
				ImGui::TextDisabled("%s", tr("lobby.waiting_ready"));
			}
		}
	}

	if (session.mode() == AppMode::Offline && match.phase == Phase::Lobby) {
		if (ImGui::Button(tr("lobby.start_offline"), ImVec2(-1, 40))) {
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
		if (ImGui::Checkbox(tr("lobby.ready"), &ready)) {
			session.requestReady(match, ready);
		}
	}

	// v0.8 #81/#108: lobby chat + emote wheel (networked sessions only).
	if (session.mode() != AppMode::Offline && match.phase == Phase::Lobby) {
		ImGui::Separator();
		ImGui::TextUnformatted(tr("lobby.chat"));
		int shown = 0;
		for (int i = static_cast<int>(match.log.size()) - 1; i >= 0 && shown < 6; --i) {
			const MatchEvent& ev = match.log[static_cast<size_t>(i)];
			if (ev.type != MatchEvent::Type::Info) {
				continue;
			}
			ImGui::TextWrapped("%s", ev.text.c_str());
			++shown;
		}
		ImGui::SetNextItemWidth(-70);
		const bool enterSend = ImGui::InputText("##chat", ui.chatInput, sizeof(ui.chatInput),
												ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		if ((ImGui::Button(tr("lobby.send"), ImVec2(-1, 0)) || enterSend) && ui.chatInput[0]) {
			if (session.requestChat(match, ui.chatInput)) {
				ui.chatInput[0] = '\0';
			}
		}
		if (ImGui::SmallButton("o7##em")) {
			session.requestChat(match, "o7 *salutes*");
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("haha##em")) {
			session.requestChat(match, "*laughs in plastic*");
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(">:(##em")) {
			session.requestChat(match, ">:( *angry toy noises*");
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
	if (ImGui::Button(tr("lobby.leave"), ImVec2(-1, 28))) {
		ui.selectedHand = -6; // disconnect intent
		ui.screen = AppScreen::Menu;
	}
	if (ImGui::SmallButton("Help")) {
		ui.showHowToPlay = true;
	}
	ImGui::End();
}

// v1.0 #168: guided first match — bottom-center step card.
void drawTutorialOverlay(Match& match, UiState& ui)
{
	if (ui.tutorialStep < 0 || ui.tutorialStep > 6) {
		return;
	}
	// Step 3 auto-advances the moment the player has played any card.
	if (ui.tutorialStep == 3 && !match.players[0].discard.empty()) {
		ui.tutorialStep = 4;
	}

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y - 264),
							ImGuiCond_Always, ImVec2(0.5f, 1.0f));
	ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.94f);
	if (ImGui::Begin("##tutorial", nullptr,
					 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
						 ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "%s  %d/7", tr("tut.title"), ui.tutorialStep + 1);
		char key[16];
		std::snprintf(key, sizeof(key), "tut.%d", ui.tutorialStep);
		ImGui::TextWrapped("%s", tr(key));
		ImGui::Spacing();
		const bool waitStep = (ui.tutorialStep == 3 || ui.tutorialStep == 6);
		if (!waitStep) {
			if (ImGui::Button(tr("tut.next"), ImVec2(140, 30))) {
				++ui.tutorialStep;
			}
			ImGui::SameLine();
		} else {
			ImGui::TextDisabled("%s", tr(ui.tutorialStep == 3 ? "tut.wait_play" : "tut.wait_win"));
		}
		if (ImGui::SmallButton(tr("tut.skip"))) {
			ui.tutorialStep = -1; // hide overlay; main still finishes the tutorial at match end
		}
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
	ImGui::Text("%s · AI %s", gameModeName(match.config.mode), aiLevelName(match.config.aiLevel));
	if (match.config.turnTimerSeconds > 0 && ui.turnTimeLeft >= 0.0f && match.phase == Phase::Playing) {
		const bool urgent = ui.turnTimeLeft < 6.0f;
		ImGui::TextColored(urgent ? ImVec4(1.0f, 0.45f, 0.35f, 1.0f) : ImVec4(0.7f, 0.8f, 0.9f, 1.0f),
						   "Turn timer: %.0fs", ui.turnTimeLeft);
	}
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
		if (ImGui::Button(tr("match.auto"))) {
			autoPlayBest(match);
			bumpSync(match);
		}
		ImGui::SameLine();
		if (ImGui::Button(tr("match.end_turn"))) {
			session.requestEndTurn(match);
		}
	}
	if (ImGui::Button(tr("match.pause"))) {
		ui.pauseOpen = true;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Help")) {
		ui.showHowToPlay = true;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton(ui.showEventHistory ? "Hide events" : "Events")) {
		ui.showEventHistory = !ui.showEventHistory; // #68
	}

	// Sandbox (#57): free event spawns for the authority side.
	if (match.config.mode == GameMode::Sandbox && session.mode() != AppMode::Client &&
		match.phase == Phase::Playing) {
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "Sandbox spawns");
		const EventKind kinds[] = { EventKind::Sandstorm, EventKind::Rain,  EventKind::Flood, EventKind::Cat,
									EventKind::Dog,       EventKind::Blackout, EventKind::None };
		for (int k = 0; k < 7; ++k) {
			if (k % 4 != 0) {
				ImGui::SameLine();
			}
			const char* label = (kinds[k] == EventKind::None) ? "Clear" : eventKindName(kinds[k]);
			if (ImGui::SmallButton(label)) {
				forceWorldEvent(match, kinds[k]);
				bumpSync(match);
				if (session.mode() == AppMode::Host) {
					session.hostPushState(match);
				}
			}
		}
	}
	ImGui::End();

	// Towers
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 280, vp->WorkPos.y + 12), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(268, 0), ImGuiCond_Always);
	ImGui::Begin("Towers", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
	const int localSeatForFog = (session.mode() == AppMode::Offline) ? match.activePlayer : session.localSeat();
	const bool fogged = eventHidesHp(match); // #67 blackout
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
		char tags[48] = {};
		if (match.config.mode == GameMode::Teams2v2 && p.team >= 0) {
			std::strncat(tags, p.team == 0 ? " [A]" : " [B]", sizeof(tags) - std::strlen(tags) - 1);
		}
		if (match.config.mode == GameMode::HotPotato && match.crownSeat == i) {
			std::strncat(tags, " [CROWN]", sizeof(tags) - std::strlen(tags) - 1);
		}
		ImGui::Text("%s%s%s%s", p.name, tags, p.eliminated ? " [OUT]" : "", mine ? " (you)" : "");
		ImGui::PopStyleColor();
		const bool hideThis = fogged && i != localSeatForFog;
		if (hideThis) {
			ImGui::ProgressBar(1.0f, ImVec2(-1, 0), "??");
			ImGui::SameLine(0, 8);
			ImGui::TextDisabled("?\?/?\?"); // escaped: "??/" is a trigraph to GCC
		} else {
			ImGui::ProgressBar(p.towerMaxHp > 0 ? static_cast<float>(p.towerHp) / static_cast<float>(p.towerMaxHp)
												: 0.f,
							   ImVec2(-1, 0), nullptr);
			ImGui::SameLine(0, 8);
			ImGui::Text("%d/%d", p.towerHp, p.towerMaxHp);
		}
		if (p.shieldTurns > 0) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "SHD %d", p.shieldTurns);
		}
	}
	ImGui::End();

	// #68: event history panel (world events only, latest first).
	if (ui.showEventHistory) {
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 360, vp->WorkPos.y + 12),
								ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(348, 200), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(tr("events.title"), &ui.showEventHistory)) {
			int shown = 0;
			for (int i = static_cast<int>(match.log.size()) - 1; i >= 0 && shown < 16; --i) {
				const MatchEvent& ev = match.log[static_cast<size_t>(i)];
				if (ev.type != MatchEvent::Type::WorldEvent) {
					continue;
				}
				const char* icon = "[*]";
				switch (static_cast<EventKind>(ev.value)) {
				case EventKind::Sandstorm: icon = "[SAND]"; break;
				case EventKind::Rain: icon = "[RAIN]"; break;
				case EventKind::Flood: icon = "[FLOOD]"; break;
				case EventKind::Cat: icon = "[CAT]"; break;
				case EventKind::Dog: icon = "[DOG]"; break;
				case EventKind::Blackout: icon = "[DARK]"; break;
				default: break;
				}
				ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s", icon);
				ImGui::SameLine();
				ImGui::TextWrapped("%s", ev.text.c_str());
				++shown;
			}
			if (shown == 0) {
				ImGui::TextDisabled("No world events yet.");
			}
		}
		ImGui::End();
	}

	// Hand
	if (match.phase == Phase::Playing) {
		const int handSeat = (session.mode() == AppMode::Offline) ? match.activePlayer : session.localSeat();
		if (handSeat >= 0 && handSeat < match.config.playerCount) {
			Player& hp = match.players[static_cast<size_t>(handSeat)];
			ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12, vp->WorkPos.y + vp->WorkSize.y - 250), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - 24, 238), ImGuiCond_Always);
			ImGui::Begin("Hand", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

			// Auto-select first legal target (#21) — skips teammates in 2v2 (#72).
			if (ui.selectedTarget == handSeat || ui.selectedTarget < 0 ||
				ui.selectedTarget >= match.config.playerCount ||
				match.players[static_cast<size_t>(ui.selectedTarget)].eliminated ||
				sameTeam(match, handSeat, ui.selectedTarget)) {
				for (int t = 0; t < match.config.playerCount; ++t) {
					if (t != handSeat && !match.players[static_cast<size_t>(t)].eliminated &&
						match.players[static_cast<size_t>(t)].control != SeatControl::Empty &&
						!sameTeam(match, handSeat, t)) {
						ui.selectedTarget = t;
						break;
					}
				}
			}

			ImGui::TextUnformatted(tr("match.target"));
			const bool fogTargets = eventHidesHp(match);
			const CardDef* selDef =
				(ui.selectedHand >= 0 && ui.selectedHand < static_cast<int>(hp.hand.size()))
					? findCard(hp.hand[static_cast<size_t>(ui.selectedHand)].defId)
					: nullptr;
			const bool singleTargetAttack =
				selDef && selDef->klass == CardClass::Attack && !(selDef->keywords & KwAOE);
			for (int t = 0; t < match.config.playerCount; ++t) {
				if (t == handSeat) {
					continue;
				}
				const Player& tp = match.players[static_cast<size_t>(t)];
				if (tp.eliminated || tp.control == SeatControl::Empty) {
					continue;
				}
				if (sameTeam(match, handSeat, t)) {
					continue; // #72: teammates are not targets
				}
				// #71: range indicator — grey out targets the selected attack cannot reach.
				const bool inRange = !singleTargetAttack || handSeat != match.activePlayer ||
									 canPlayCard(match, match.activePlayer, ui.selectedHand, t);
				ImGui::SameLine();
				char label[48];
				if (fogTargets) {
					std::snprintf(label, sizeof(label), "%s (?\?)%s##tgt", tp.name, tp.shieldTurns > 0 ? " shd" : "");
				} else {
					std::snprintf(label, sizeof(label), "%s (%d)%s##tgt", tp.name, tp.towerHp,
								  tp.shieldTurns > 0 ? " shd" : "");
				}
				if (!inRange) {
					ImGui::BeginDisabled();
				}
				if (ImGui::RadioButton(label, ui.selectedTarget == t)) {
					ui.selectedTarget = t;
				}
				if (!inRange) {
					ImGui::EndDisabled();
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
				// v0.7 P2 #50 lite: rarity tint (selection wins).
				ImGui::PushStyleColor(ImGuiCol_Button, rarityColor(def->rarity, selected));
				char btn[128];
				if (def->klass == CardClass::Attack && (def->keywords & KwAOE)) {
					std::snprintf(btn, sizeof(btn), "[%s] %s\nhits both neighbors", cardClassName(def->klass),
								  def->name);
				} else if (def->klass == CardClass::Attack && ui.selectedTarget >= 0 &&
						   ui.selectedTarget < match.config.playerCount) {
					const int preview = computeAttackDamage(match, match.activePlayer, ui.selectedTarget, *def);
					std::snprintf(btn, sizeof(btn), "[%s] %s\n~%d dmg", cardClassName(def->klass), def->name, preview);
				} else {
					std::snprintf(btn, sizeof(btn), "[%s] %s\n%s", cardClassName(def->klass), def->name, cardDescription(*def));
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
					ImGui::TextWrapped("%s", cardDescription(*def));
					ImGui::EndTooltip();
				}
				ImGui::PopStyleColor();
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
			if (ImGui::Button(tr("match.play"), ImVec2(200, 36))) {
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

	// Battle log + optional live scrubber (P2 #25)
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 360, vp->WorkPos.y + 220), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(348, 260), ImGuiCond_FirstUseEver);
	ImGui::Begin("Battle Log");
	// v1.1 #112: larger stream-readable log text.
	ImGui::SetWindowFontScale(ui.largeLogFont ? 1.35f : 1.0f);
	if (!match.log.empty()) {
		const int n = static_cast<int>(match.log.size());
		if (ui.timelineIndex < 0 || ui.timelineIndex >= n) {
			ui.timelineIndex = n - 1;
		}
		ImGui::Text("%s %d/%d", tr("timeline.scrub"), ui.timelineIndex + 1, n);
		ImGui::SliderInt("##live_scrub", &ui.timelineIndex, 0, n - 1);
		const MatchEvent& focus = match.log[static_cast<size_t>(ui.timelineIndex)];
		if (focus.type == MatchEvent::Type::WorldEvent) {
			ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s", focus.text.c_str());
		} else {
			ImGui::TextWrapped("%s", focus.text.c_str());
		}
		ImGui::Separator();
		ImGui::TextDisabled("Recent");
	}
	for (int i = static_cast<int>(match.log.size()) - 1; i >= 0 && i >= static_cast<int>(match.log.size()) - 12; --i) {
		const MatchEvent& ev = match.log[static_cast<size_t>(i)];
		const bool hi = (i == ui.timelineIndex);
		if (hi) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.77f, 0.36f, 1.0f));
		}
		if (ev.type == MatchEvent::Type::WorldEvent) {
			ImGui::TextColored(hi ? ImVec4(0.96f, 0.77f, 0.36f, 1.0f) : ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s",
							   ev.text.c_str());
		} else {
			ImGui::TextWrapped("%s", ev.text.c_str());
		}
		if (hi) {
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();

	// Coach tips during early matches (P2 #13)
	drawCoachTip(ui);

	// v1.0 #168: tutorial step card sits above the hand.
	drawTutorialOverlay(match, ui);

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
	// Count completed match once (coach tips + persistence)
	if (!ui.matchCounted) {
		ui.matchCounted = true;
		++ui.matchesCompleted;
		ui.coachTipDismissed = false;
		ui.settingsDirty = true;
		ui.showTimeline = true;
		ui.timelineIndex = match.log.empty() ? -1 : static_cast<int>(match.log.size()) - 1;
	}

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.38f),
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
	// v0.8 #109: rematch happens when every connected human votes yes.
	if (match.phase == Phase::Playing) {
		ui.screen = AppScreen::Match;
		ui.matchCounted = false;
		ui.rematchVoteSent = false;
	}

	ImGui::Separator();
	if (session.mode() == AppMode::Host) {
		int votes = 0, needed = 0;
		session.rematchVoteCounts(match, votes, needed);
		char label[64];
		if (needed > 1) {
			std::snprintf(label, sizeof(label), "%s (%d/%d)", tr("results.rematch"), votes, needed);
		} else {
			std::snprintf(label, sizeof(label), "%s", tr("results.rematch"));
		}
		if (ImGui::Button(label, ImVec2(-1, 36))) {
			session.requestRematchVote(match, true);
			if (match.phase == Phase::Playing) {
				ui.matchCounted = false;
				ui.rematchVoteSent = false;
				ui.screen = AppScreen::Match;
			}
		}
		if (needed > 1) {
			ImGui::TextDisabled("%s", tr("results.vote_hint"));
		}
	}
	if (session.mode() == AppMode::Client) {
		if (ui.rematchVoteSent) {
			ImGui::TextDisabled("%s", tr("results.vote_sent"));
		} else if (ImGui::Button(tr("results.vote"), ImVec2(-1, 36))) {
			if (session.requestRematchVote(match, true)) {
				ui.rematchVoteSent = true;
			}
		}
	}
	if (session.mode() == AppMode::Offline) {
		if (ImGui::Button(tr("results.play_again"), ImVec2(-1, 36))) {
			ui.matchCounted = false;
			ui.selectedHand = -2; // offline again, same settings/cosmetics
		}
		if (ImGui::Button(tr("results.change_loadout"), ImVec2(-1, 36))) {
			ui.matchCounted = false;
			ui.selectedHand = -6;
			ui.screen = AppScreen::Menu;
		}
	}
	if (ImGui::Button(tr("results.menu"), ImVec2(-1, 36))) {
		ui.matchCounted = false;
		ui.selectedHand = -6;
		ui.screen = AppScreen::Menu;
	}
	ImGui::End();

	// P2 #25: read-only match timeline scrubber on results
	drawTimelineScrubber(match, ui);
}

} // namespace

void uiDraw(Match& match, NetSession& session, LanDiscovery& discovery, UiState& ui)
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
		drawMenuV2(discovery, ui);
		break;
	case AppScreen::Settings:
		drawSettings(ui);
		break;
	case AppScreen::HowToPlay:
		ui.showHowToPlay = true;
		ui.screen = AppScreen::Menu;
		break;
	case AppScreen::Credits:
		drawCredits(ui);
		break;
	case AppScreen::Codex:
		drawCodex(ui);
		break;
	case AppScreen::Profile:
		drawProfile(ui);
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
