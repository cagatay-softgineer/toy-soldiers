#pragma once

#include "game/types.h"

#include <span>
#include <vector>

namespace toy {

// Fixed starter catalog — ~12 core cards for M1 (concept board: 10–15).
std::span<const CardDef> cardCatalog();
const CardDef* findCard(int defId);

// Build a shuffled starter deck for one player.
std::vector<CardInstance> makeStarterDeck(int playerId, uint32_t& rngState, int& nextInstanceId);

// xorshift32 — deterministic, no heap, good enough for jam prototypes.
uint32_t xorshift32(uint32_t& state);
int randRange(uint32_t& state, int lo, int hiInclusive);

void shuffleInPlace(std::vector<CardInstance>& cards, uint32_t& rngState);

} // namespace toy
