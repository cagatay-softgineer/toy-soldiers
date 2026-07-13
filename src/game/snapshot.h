#pragma once

#include "game/match.h"

#include <cstdint>
#include <vector>

namespace toy {

// Host → clients authoritative snapshot. Deterministic enough for turn-based M2.
// Physics is not synced; clients apply impulse cues locally.

std::vector<uint8_t> serializeMatch(const Match& match);
bool deserializeMatch(Match& match, const uint8_t* data, size_t size);

// v1.2 #96: player-level delta against a previous broadcast state. The client applies
// a delta only when its (matchId, syncGeneration) equals the delta's base; any mismatch
// returns NeedResync and the caller falls back to the #92 full-resync path.
enum class DeltaResult : uint8_t { Ok, NeedResync, Bad };
std::vector<uint8_t> serializeMatchDelta(const Match& match, const Match& prev);
DeltaResult applyMatchDelta(Match& match, const uint8_t* data, size_t size);

// Compact lobby-only payload (pre-match seats).
std::vector<uint8_t> serializeLobby(const Match& match);
bool deserializeLobby(Match& match, const uint8_t* data, size_t size);

} // namespace toy
