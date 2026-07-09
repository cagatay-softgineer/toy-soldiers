#pragma once

#include "game/match.h"

namespace toy {

// Human-readable why a card play is illegal (v0.6).
// Returns static/empty string if play would be legal (does not mutate match).
const char* describeIllegalPlay(const Match& match, int playerIndex, int handIndex, int targetPlayer);

} // namespace toy
