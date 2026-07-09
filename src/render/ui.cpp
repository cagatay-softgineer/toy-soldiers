#include "render/ui.h"

#include "game/cards.h"
#include "game/cosmetics.h"
#include "game/events.h"
#include "game/rules.h"
#include "game/summary.h"

#include "imgui.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_imgui.h"

#include <cstdio>

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

void drawLobby(Match& match, NetSession& session, UiState& ui)
{
	ImGui::Separator();
	ImGui::TextUnformatted("Lobby seats");
	for (int i = 0; i < match.config.playerCount; ++i) {
		const Player& p = match.players[static_cast<size_t>(i)];
		ImGui::Text("Seat %d: %-12s  [%s]  %s  tower=%s", i, p.name[0] ? p.name : "(empty)",
					seatControlName(p.control), p.ready ? "READY" : "…", towerTypeName(p.tower));
	}

	// Map pick — host / offline lobby authority
	if ((session.mode() == AppMode::Host || session.mode() == AppMode::Offline) && match.phase == Phase::Lobby) {
		ImGui::Separator();
		ImGui::TextUnformatted("Map / room theme (M3)");
		ui.mapIndex = static_cast<int>(match.config.mapId);
		const char* maps[] = { "Living Room Table", "Desert Playmat", "Backyard Picnic" };
		if (ImGui::Combo("##map", &ui.mapIndex, maps, 3)) {
			match.config.mapId = static_cast<MapId>(ui.mapIndex);
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			} else {
				bumpSync(match);
			}
		}
		ImGui::TextDisabled("%s", mapBlurb(match.config.mapId));
		if (ImGui::Checkbox("World events enabled", &ui.eventsEnabled)) {
			match.config.eventsEnabled = ui.eventsEnabled;
			if (session.mode() == AppMode::Host) {
				session.hostPushState(match);
			} else {
				bumpSync(match);
			}
		}
	} else if (match.phase == Phase::Lobby) {
		ImGui::Text("Map: %s", mapName(match.config.mapId));
		ImGui::TextDisabled("%s", mapBlurb(match.config.mapId));
	}

	if (session.mode() == AppMode::Host && match.phase == Phase::Lobby) {
		ImGui::Checkbox("Fill empty seats with AI", &ui.fillAI);
		match.config.fillEmptyWithAI = ui.fillAI;
		if (ImGui::Button("Start Match", ImVec2(160, 32))) {
			session.hostStartMatch(match);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Needs ≥1 human; empties → AI if checked");
	}

	// Lobby: all modes. Offline: also mid-match (hotseat fashion show).
	const bool canEditLoadout =
		(match.phase == Phase::Lobby &&
		 (session.mode() == AppMode::Host || session.mode() == AppMode::Client || session.mode() == AppMode::Offline)) ||
		(session.mode() == AppMode::Offline && match.phase == Phase::Playing);

	if (canEditLoadout && (session.localSeat() >= 0 || session.mode() == AppMode::Offline)) {
		const int seat = (session.mode() == AppMode::Offline)
							 ? (match.phase == Phase::Playing ? match.activePlayer : 0)
							 : session.localSeat();
		if (seat < 0 || seat >= match.config.playerCount) {
			// skip
		} else {
		Player& me = match.players[static_cast<size_t>(seat)];

		ImGui::Separator();
		ImGui::TextUnformatted("Your loadout");
		ImGui::TextUnformatted("Tower type (gameplay):");
		const TowerType cur = me.tower;
		if (ImGui::RadioButton("Machine Gun", cur == TowerType::MachineGun)) {
			session.requestSetTower(match, TowerType::MachineGun);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Sniper", cur == TowerType::Sniper)) {
			session.requestSetTower(match, TowerType::Sniper);
		}

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "Cosmetics (M4 — no power)");
		ui.plasticIndex = static_cast<int>(me.cosmetics.plastic);
		ui.towerSkinIndex = static_cast<int>(me.cosmetics.towerSkin);
		ui.accessoryIndex = static_cast<int>(me.cosmetics.accessory);

		// Build name lists once per frame (small).
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
		}

		// Color swatch
		const b3Vec3 rgb = plasticColorRgb(me.cosmetics.plastic);
		ImGui::ColorButton("##swatch", ImVec4(rgb.x, rgb.y, rgb.z, 1.0f), ImGuiColorEditFlags_NoTooltip,
						   ImVec2(48, 24));
		ImGui::SameLine();
		ImGui::TextDisabled("%s + %s + %s", plasticColorName(me.cosmetics.plastic),
							towerSkinName(me.cosmetics.towerSkin), accessoryName(me.cosmetics.accessory));
		} // seat valid
	}

	if (session.mode() == AppMode::Client && match.phase == Phase::Lobby && session.localSeat() >= 0) {
		bool ready = match.players[static_cast<size_t>(session.localSeat())].ready;
		if (ImGui::Checkbox("Ready", &ready)) {
			session.requestReady(match, ready);
		}
	}
}

} // namespace

void uiDraw(Match& match, NetSession& session, UiState& ui)
{
	const ImGuiViewport* vp = ImGui::GetMainViewport();

	// Toast when world events fire (detect via sync + last log line).
	if (match.syncGeneration != ui.lastToastSync) {
		ui.lastToastSync = match.syncGeneration;
		if (!match.log.empty()) {
			const MatchEvent& last = match.log.back();
			if (last.type == MatchEvent::Type::WorldEvent || last.type == MatchEvent::Type::Winner) {
				std::snprintf(ui.toast, sizeof(ui.toast), "%s", last.text.c_str());
				ui.toastTimer = 3.2f;
			}
		}
	}
	if (ui.toastTimer > 0.0f) {
		ui.toastTimer -= ImGui::GetIO().DeltaTime;
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 48.0f),
								ImGuiCond_Always, ImVec2(0.5f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.82f);
		if (ImGui::Begin("##toast", nullptr,
						 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
							 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs)) {
			ImGui::TextColored(ImVec4(0.85f, 0.95f, 1.0f, 1.0f), "%s", ui.toast);
		}
		ImGui::End();
	}

	// How to play (first run)
	if (ui.showHowToPlay) {
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
								ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Always);
		ImGui::Begin("How to play — Oyuncak Asker", &ui.showHowToPlay, ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::TextWrapped(
			"Command plastic toy soldiers on a tabletop. Each turn play one card: attack enemy towers, "
			"shield yourself, or draw/heal. Last tower standing wins.");
		ImGui::BulletText("LMB drag = orbit camera · Scroll = zoom");
		ImGui::BulletText("Pick a target, then a card, then Play");
		ImGui::BulletText("Shield halves damage; Scout Peek buffs your next attack");
		ImGui::BulletText("Maps add room props + world events (sandstorm, rain, flood, cat)");
		ImGui::BulletText("Cosmetics are fashion only — no combat power");
		ImGui::BulletText("Host Lobby for LAN multiplayer (port %u)", static_cast<unsigned>(kDefaultPort));
		ImGui::BulletText("Keys: Space = auto-play · R = reset · 1–5 = hand · Enter = play");
		if (ImGui::Button("Got it — let's play", ImVec2(-1, 36))) {
			ui.showHowToPlay = false;
		}
		ImGui::End();
	}

	// --- Network / mode panel ---
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12, vp->WorkPos.y + 12), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);
	ImGui::Begin("Oyuncak Asker — M5 Polish", nullptr,
				 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

	ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.36f, 1.0f), "Toy Soldiers Tabletop");
	ImGui::Text("Mode: %s", modeName(session.mode()));
	ImGui::TextWrapped("Status: %s", session.status());
	ImGui::Text("Phase: %s | Seat: %d | Sync: %u", phaseName(match.phase), session.localSeat(),
				match.syncGeneration);
	ImGui::Text("Map: %s", mapName(match.config.mapId));
	ImGui::Checkbox("Auto-orbit camera", &ui.autoOrbit);
	ImGui::SameLine();
	if (ImGui::SmallButton("Help")) {
		ui.showHowToPlay = true;
	}

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
		if (ap.attackBonusNext > 0) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.5f, 1.0f), "(atk +%d ready)", ap.attackBonusNext);
		}
		if (eventForcesAdjacent(match)) {
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "Sandstorm: adjacent targets only!");
		}
		if (session.canLocalAct(match) ||
			(session.mode() == AppMode::Offline && !ap.eliminated)) {
			const bool myHotseat = session.mode() == AppMode::Offline || session.canLocalAct(match);
			if (myHotseat && (session.mode() == AppMode::Offline || match.activePlayer == session.localSeat())) {
				ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), ">>> YOUR TURN — play one card <<<");
			}
		} else if (session.mode() != AppMode::Offline) {
			ImGui::TextDisabled("Waiting for %s…", ap.name);
		}
	} else if (match.phase == Phase::GameOver) {
		const MatchSummary sum = buildSummary(match);
		char line[160];
		formatSummaryLine(sum, match, line, sizeof(line));
		ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "%s", line);
		ImGui::Separator();
		ImGui::TextUnformatted("Post-match stats");
		for (int i = 0; i < match.config.playerCount; ++i) {
			const Player& p = match.players[static_cast<size_t>(i)];
			if (p.control == SeatControl::Empty) {
				continue;
			}
			ImGui::Text("%s: dmg %d dealt / %d taken · %d cards", p.name, sum.damageDealt[static_cast<size_t>(i)],
						sum.damageTaken[static_cast<size_t>(i)], sum.cardsPlayed[static_cast<size_t>(i)]);
		}
		if (session.mode() == AppMode::Host && ImGui::Button("Rematch")) {
			session.hostRematch(match);
		}
		if (session.mode() == AppMode::Offline && ImGui::Button("New Hotseat Match")) {
			MatchConfig cfg = match.config;
			cfg.seed = match.rng + 11u;
			session.startOffline(match, cfg);
		}
	}

	ImGui::Separator();
	ImGui::InputText("Display name", ui.playerName, sizeof(ui.playerName));

	if (session.mode() == AppMode::Offline) {
		if (ImGui::Button("Host Lobby")) {
			MatchConfig cfg;
			cfg.playerCount = 4;
			cfg.fillEmptyWithAI = ui.fillAI;
			cfg.mapId = static_cast<MapId>(ui.mapIndex);
			cfg.eventsEnabled = ui.eventsEnabled;
			cfg.seed = 42 + static_cast<uint32_t>(match.syncGeneration);
			std::snprintf(ui.hostName, sizeof(ui.hostName), "%s", ui.playerName);
			session.hostLobby(match, cfg, ui.hostName, static_cast<uint16_t>(ui.hostPort));
			Cosmetics cos;
			cos.plastic = static_cast<PlasticColor>(ui.plasticIndex);
			cos.towerSkin = static_cast<TowerSkin>(ui.towerSkinIndex);
			cos.accessory = static_cast<Accessory>(ui.accessoryIndex);
			session.requestSetCosmetics(match, cos);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		ImGui::InputInt("##hostport", &ui.hostPort);
		if (ui.hostPort < 1024) {
			ui.hostPort = kDefaultPort;
		}
		if (ui.hostPort > 65535) {
			ui.hostPort = 65535;
		}

		ImGui::InputText("Join IP", ui.joinHost, sizeof(ui.joinHost));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		ImGui::InputInt("##joinport", &ui.joinPort);
		if (ImGui::Button("Join Host")) {
			session.joinLobby(match, ui.joinHost, static_cast<uint16_t>(ui.joinPort), ui.playerName);
		}
		ImGui::SameLine();
		if (ImGui::Button("New Hotseat")) {
			MatchConfig cfg;
			cfg.playerCount = 4;
			cfg.mapId = static_cast<MapId>(ui.mapIndex);
			cfg.eventsEnabled = ui.eventsEnabled;
			cfg.seed = 100 + match.syncGeneration;
			session.startOffline(match, cfg);
			Cosmetics cos;
			cos.plastic = static_cast<PlasticColor>(ui.plasticIndex);
			cos.towerSkin = static_cast<TowerSkin>(ui.towerSkinIndex);
			cos.accessory = static_cast<Accessory>(ui.accessoryIndex);
			for (int i = 1; i < match.config.playerCount; ++i) {
				match.players[static_cast<size_t>(i)].cosmetics = defaultCosmeticsForSeat(i);
			}
			session.requestSetCosmetics(match, cos); // seat 0 + rebuild flag
			ui.selectedHand = 0;
			ui.selectedTarget = 1;
		}
	} else {
		if (ImGui::Button("Disconnect → Offline")) {
			MatchConfig cfg;
			cfg.playerCount = 4;
			session.startOffline(match, cfg);
		}
	}

	if (match.phase == Phase::Lobby) {
		drawLobby(match, session, ui);
	}

	if (session.mode() == AppMode::Offline && match.phase == Phase::Playing) {
		if (ImGui::Button("Auto Play")) {
			autoPlayBest(match);
			bumpSync(match);
		}
		ImGui::SameLine();
		if (ImGui::Button("End Turn")) {
			session.requestEndTurn(match);
		}
		// Debug force events (offline only)
		if (ImGui::TreeNode("Debug force event")) {
			if (ImGui::Button("Sandstorm")) {
				forceWorldEvent(match, EventKind::Sandstorm);
				bumpSync(match);
			}
			ImGui::SameLine();
			if (ImGui::Button("Rain")) {
				forceWorldEvent(match, EventKind::Rain);
				bumpSync(match);
			}
			ImGui::SameLine();
			if (ImGui::Button("Flood")) {
				forceWorldEvent(match, EventKind::Flood, match.activePlayer);
				bumpSync(match);
			}
			ImGui::SameLine();
			if (ImGui::Button("Cat")) {
				forceWorldEvent(match, EventKind::Cat);
				bumpSync(match);
			}
			ImGui::TreePop();
		}
	}

	if (session.mode() == AppMode::Host && match.phase == Phase::Playing) {
		if (ImGui::Button("End My Turn")) {
			session.requestEndTurn(match);
		}
	}

	ImGui::End();

	// --- Towers ---
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 280, vp->WorkPos.y + 12), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(268, 0), ImGuiCond_Always);
	ImGui::Begin("Towers", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
	for (int i = 0; i < match.config.playerCount; ++i) {
		const Player& p = match.players[static_cast<size_t>(i)];
		const bool active = (i == match.activePlayer && match.phase == Phase::Playing);
		const bool mine = (session.mode() != AppMode::Offline && i == session.localSeat());
		if (active) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.77f, 0.36f, 1.0f));
		} else if (mine) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 1.0f, 1.0f));
		}
		ImGui::Text("%s%s%s", p.name, p.eliminated ? " [OUT]" : "", mine ? " (you)" : "");
		if (active || mine) {
			ImGui::PopStyleColor();
		}
		ImGui::ProgressBar(p.towerMaxHp > 0 ? static_cast<float>(p.towerHp) / static_cast<float>(p.towerMaxHp) : 0.0f,
						   ImVec2(-1, 0), nullptr);
		ImGui::SameLine(0, 8);
		ImGui::Text("%d/%d", p.towerHp, p.towerMaxHp);
		if (p.shieldTurns > 0) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "SHD %d", p.shieldTurns);
		}
	}
	ImGui::End();

	// --- Hand ---
	if (match.phase == Phase::Playing) {
		const int handSeat = (session.mode() == AppMode::Offline) ? match.activePlayer : session.localSeat();
		if (handSeat >= 0 && handSeat < match.config.playerCount) {
			Player& hp = match.players[static_cast<size_t>(handSeat)];
			ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12, vp->WorkPos.y + vp->WorkSize.y - 250), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - 24, 238), ImGuiCond_Always);
			ImGui::Begin("Hand", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

			ImGui::Text("Showing: %s | Active turn: %s", hp.name,
						match.players[static_cast<size_t>(match.activePlayer)].name);
			if (hp.attackBonusNext > 0) {
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.5f, 1.0f), "| Next attack +%d", hp.attackBonusNext);
			}

			// Keep target valid
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
			if (ui.selectedHand >= static_cast<int>(hp.hand.size())) {
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
					const int preview =
						computeAttackDamage(match, match.activePlayer, ui.selectedTarget, *def);
					std::snprintf(btn, sizeof(btn), "[%s] %s\n~%d dmg · %s", cardClassName(def->klass), def->name,
								  preview, def->description);
				} else {
					std::snprintf(btn, sizeof(btn), "[%s] %s\n%s", cardClassName(def->klass), def->name,
								  def->description);
				}
				if (ImGui::Button(btn, ImVec2(178, 78))) {
					ui.selectedHand = h;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s", def->description);
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
			if (ImGui::Button("Play Selected Card  (Enter)", ImVec2(220, 36))) {
				if (session.requestPlayCard(match, ui.selectedHand, ui.selectedTarget)) {
					ui.selectedHand = 0;
				}
			}
			if (!can) {
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (!myTurn) {
					ImGui::TextDisabled("Not your turn");
				} else {
					ImGui::TextDisabled("Illegal target / empty hand");
				}
			}
			ImGui::End();
		}
	}

	// --- Log ---
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 360, vp->WorkPos.y + 220), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(348, 240), ImGuiCond_FirstUseEver);
	ImGui::Begin("Battle Log");
	for (int i = static_cast<int>(match.log.size()) - 1; i >= 0 && i >= static_cast<int>(match.log.size()) - 24; --i) {
		const MatchEvent& ev = match.log[static_cast<size_t>(i)];
		if (ev.type == MatchEvent::Type::WorldEvent) {
			ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s", ev.text.c_str());
		} else {
			ImGui::TextWrapped("%s", ev.text.c_str());
		}
	}
	ImGui::End();

	// --- Help ---
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12, vp->WorkPos.y + 280), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
	ImGui::Begin("Controls / M5");
	ImGui::BulletText("Host Lobby = authority (no dedicated server)");
	ImGui::BulletText("MG tower = 36 HP · Sniper = 34 HP +2 attack damage");
	ImGui::BulletText("Scout Peek buff lasts until you attack");
	ImGui::BulletText("Shield halves damage; blocks flood chip");
	ImGui::BulletText("LMB orbit · Scroll zoom · Space auto · R reset");
	ImGui::BulletText("1–5 select hand card · Enter play");
	ImGui::BulletText("Default port %u", static_cast<unsigned>(kDefaultPort));
	ImGui::End();
}

} // namespace toy
