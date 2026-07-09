#pragma once

#include "game/match.h"

#include <cstdint>
#include <vector>

namespace toy {

// Host → clients authoritative snapshot. Deterministic enough for turn-based M2.
// Physics is not synced; clients apply impulse cues locally.

std::vector<uint8_t> serializeMatch(const Match& match);
bool deserializeMatch(Match& match, const uint8_t* data, size_t size);

// Compact lobby-only payload (pre-match seats).
std::vector<uint8_t> serializeLobby(const Match& match);
bool deserializeLobby(Match& match, const uint8_t* data, size_t size);

} // namespace toy
