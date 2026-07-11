#pragma once

#include "game/types.h"

#include <span>
#include <vector>

namespace toy {

// v0.7 catalog — 23 defs across Attack / Defense / Tactic / Event with keywords.
std::span<const CardDef> cardCatalog();
const CardDef* findCard(int defId);

// Build a shuffled starter deck for one player. Mode picks the recipe (Quick Duel = 15 cards);
// the player's tower injects a signature card and deck-builder bans/extras apply (#41, #47).
std::vector<CardInstance> makeStarterDeck(int playerId, uint32_t& rngState, int& nextInstanceId, GameMode mode,
										  const Player& forPlayer);

// xorshift32 — deterministic, no heap, good enough for jam prototypes.
uint32_t xorshift32(uint32_t& state);
int randRange(uint32_t& state, int lo, int hiInclusive);

void shuffleInPlace(std::vector<CardInstance>& cards, uint32_t& rngState);

} // namespace toy
