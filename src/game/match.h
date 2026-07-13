#pragma once

#include "game/types.h"

#include <array>
#include <vector>

namespace toy {

class ReplayRecorder; // game/replay.h — forward-declared to avoid a circular include

struct PhysicsImpulseRequest {
	int targetPlayer = -1;
	float strength = 0.0f;
	int frames = 0;
};

struct Match {
	MatchConfig config;
	Phase phase = Phase::Lobby;
	int activePlayer = 0;
	int turnNumber = 0;
	int winner = -1;
	uint32_t rng = 1; // rule-affecting only: draws, reshuffles, event rolls, crown/focus picks
	// v1.2 #107: separate decision-only stream (AI scoring noise, persona jitter, Easy-AI
	// pick). Never consumed by anything that changes visible game state, so a replay that
	// re-applies recorded actions without running the AI reproduces `rng` bit-for-bit —
	// the whole point of a deterministic replay. Host-local; not part of the wire snapshot.
	uint32_t aiRng = 1;
	int nextInstanceId = 1;
	int nextSoldierId = 1;
	uint32_t syncGeneration = 0; // bumps on every authoritative mutation
	uint32_t matchId = 0;        // bumps when a new match starts

	std::array<Player, kMaxPlayers> players{};
	std::vector<MatchEvent> log;

	PhysicsImpulseRequest pendingPhysicsImpulse;

	// M3 world event system
	WorldEventState world{};
	int eventCooldown = 0; // turns until a new event may roll

	// v0.7 #55 Hot Potato: current crown holder (-1 when mode inactive).
	int crownSeat = -1;

	// v0.8 #79: host closed the lobby to new joins.
	bool lobbyLocked = false;

	// v1.2 #84: host-picked lobby banner color (palette index 0..7).
	uint8_t bannerColor = 0;
	// v1.2 #101/#102: per-seat peer addresses as seen by the host — lets survivors
	// elect and find a new host when the current one vanishes. LAN-scoped by design.
	char peerIp[kMaxPlayers][40] = {};
};

void bumpSync(Match& match);

// v0.7: clamp playerCount etc. to the selected mode (Quick Duel = 2 seats, team modes = 4).
void applyModeToConfig(MatchConfig& config);

// Prepare 4 empty lobby seats (host will claim seat 0).
void initLobby(Match& match, const MatchConfig& config);

// Start playing from lobby (host authority). Fills empty seats with AI if configured.
void startMatchFromLobby(Match& match);

// Offline hotseat: jump straight into a full match.
void startMatch(Match& match, const MatchConfig& config);
void resetMatch(Match& match);

// recorder (v1.2 #107, optional): logs the resolved (handIndex, target) or the
// no-legal-play endTurn so a .toyrec can reproduce this exact match later.
bool autoPlayBest(Match& match, ReplayRecorder* recorder = nullptr);

// Host helper: if active seat is AI, auto-play once.
bool autoPlayIfAITurn(Match& match, ReplayRecorder* recorder = nullptr);

} // namespace toy
