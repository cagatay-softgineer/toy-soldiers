#pragma once

#include "game/types.h"

namespace toy {

struct Match; // defined in match.h

// Pure rule helpers — no network, no rendering.

bool isValidTarget(const Match& match, int actor, int target, const CardDef& card);
bool canPlayCard(const Match& match, int playerIndex, int handIndex, int targetPlayer);
int computeAttackDamage(const Match& match, int actor, int target, const CardDef& card);

// Apply a validated play. Returns false if illegal.
bool applyCardPlay(Match& match, int handIndex, int targetPlayer);

// Advance turn to next living player; draw a card for them.
void endTurn(Match& match);

// Draw up to `count` cards for player; reshuffle discard if needed.
void drawCards(Match& match, int playerIndex, int count);

// Check elimination + win condition. Sets phase / winner.
void evaluateEndState(Match& match);

} // namespace toy
