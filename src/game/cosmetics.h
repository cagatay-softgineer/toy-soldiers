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

// --- v0.9 #146: local unlock track (fashion only) ---
bool plasticUnlocked(PlasticColor c, int matches, int wins);
bool towerSkinUnlocked(TowerSkin s, int matches, int wins);
bool accessoryUnlocked(Accessory a, int matches, int wins);
// Human-readable requirement ("play 4 matches"); empty when always unlocked.
void plasticUnlockText(PlasticColor c, char* out, int cap);
void towerSkinUnlockText(TowerSkin s, char* out, int cap);
void accessoryUnlockText(Accessory a, char* out, int cap);

// --- v0.9 #145: named cosmetic sets ---
struct CosmeticSet {
	const char* name;
	PlasticColor plastic;
	TowerSkin towerSkin;
	Accessory accessory;
};
int cosmeticSetCount();
const CosmeticSet& cosmeticSet(int index);
bool cosmeticSetUnlocked(int index, int matches, int wins);

} // namespace toy
