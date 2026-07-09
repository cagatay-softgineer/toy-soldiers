#pragma once

#include "game/types.h"

#include <array>
#include <vector>

namespace toy {

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
	uint32_t rng = 1;
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
};

void bumpSync(Match& match);

// Prepare 4 empty lobby seats (host will claim seat 0).
void initLobby(Match& match, const MatchConfig& config);

// Start playing from lobby (host authority). Fills empty seats with AI if configured.
void startMatchFromLobby(Match& match);

// Offline hotseat: jump straight into a full match.
void startMatch(Match& match, const MatchConfig& config);
void resetMatch(Match& match);

bool autoPlayBest(Match& match);

// Host helper: if active seat is AI, auto-play once.
bool autoPlayIfAITurn(Match& match);

} // namespace toy
