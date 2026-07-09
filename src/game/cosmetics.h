#pragma once

#include "game/types.h"

#include "box3d/math_functions.h"

namespace toy {

// --- Catalog (names for UI) ---

const char* plasticColorName(PlasticColor c);
const char* towerSkinName(TowerSkin s);
const char* accessoryName(Accessory a);

int plasticColorCount();
int towerSkinCount();
int accessoryCount();

// Default seat palette (classic toy soldiers).
PlasticColor defaultPlasticForSeat(int seat);
Cosmetics defaultCosmeticsForSeat(int seat);

// Resolve plastic RGB (glossy toy look, 0–1).
b3Vec3 plasticColorRgb(PlasticColor c);

// Tower base color: skin modulates plastic.
b3Vec3 towerColorRgb(const Cosmetics& cos);
b3Vec3 soldierColorRgb(const Cosmetics& cos);
b3Vec3 accessoryColorRgb(const Cosmetics& cos);

// Tower half-extents by skin (hx, hy, hz).
void towerHalfExtents(TowerSkin skin, float& hx, float& hy, float& hz);

// Room clear / ambient for renderer.
void mapClearColor(MapId map, float& r, float& g, float& b);

// AI seats get a fun random-looking but deterministic cosmetic from seed+seat.
void applyAiCosmetics(Player& p, uint32_t seed);

} // namespace toy
