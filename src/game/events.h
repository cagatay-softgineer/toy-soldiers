#pragma once

#include "game/types.h"

namespace toy {

struct Match;

// --- Queries used by the rule engine ---

// Sandstorm (or map rule): attacks may only target adjacent seats.
bool eventForcesAdjacent(const Match& match);

// Rain: reduce outgoing attack damage.
int eventModifyAttackDamage(const Match& match, int baseDamage);

// Flooded seat helper.
bool seatIsFlooded(const Match& match, int seat);

// Human-readable status line for HUD.
void formatWorldEventStatus(const Match& match, char* out, int outCap);

// --- Lifecycle (host authority) ---

// Clear event state when a match starts.
void resetWorldEvents(Match& match);

// Called once after turn advances (endTurn). Handles:
// - flood leak on the new active player
// - cat warning → pounce
// - countdown / expiry
// - probabilistic spawn of a new map-weighted event
void tickWorldEvents(Match& match);

// Force-spawn (tests / debug). Returns false if blocked.
bool forceWorldEvent(Match& match, EventKind kind, int focusSeat = -1);

// Map felt color for rendering (RGB 0–1).
void mapTableColor(MapId map, float& r, float& g, float& b);
void mapFloorColor(MapId map, float& r, float& g, float& b);

} // namespace toy
