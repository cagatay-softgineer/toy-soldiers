#pragma once

#include "game/match.h"

#include <array>

namespace toy {

// Derived from match log — no extra network state (M5 polish).
struct MatchSummary {
	int turns = 0;
	int winner = -1;
	int worldEvents = 0;
	std::array<int, kMaxPlayers> damageDealt{};
	std::array<int, kMaxPlayers> damageTaken{};
	std::array<int, kMaxPlayers> cardsPlayed{};
	std::array<int, kMaxPlayers> heals{};
};

MatchSummary buildSummary(const Match& match);

// One-line for logs / UI.
void formatSummaryLine(const MatchSummary& s, const Match& match, char* out, int outCap);

} // namespace toy
