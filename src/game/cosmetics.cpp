#include "game/cosmetics.h"

#include "game/cards.h"

#include <cstdio>

namespace toy {

const char* plasticColorName(PlasticColor c)
{
	switch (c) {
	case PlasticColor::ClassicGreen: return "Classic Green";
	case PlasticColor::ArmyOlive: return "Army Olive";
	case PlasticColor::Blue: return "Toy Blue";
	case PlasticColor::Red: return "Toy Red";
	case PlasticColor::Yellow: return "Toy Yellow";
	case PlasticColor::Purple: return "Grape Purple";
	case PlasticColor::Orange: return "Safety Orange";
	case PlasticColor::White: return "Milk White";
	case PlasticColor::Pink: return "Bubblegum";
	case PlasticColor::Camo: return "Camo Mix";
	case PlasticColor::JellyLime: return "Jelly Lime";
	case PlasticColor::JellyBerry: return "Jelly Berry";
	case PlasticColor::GoldChrome: return "Fake Gold Chrome";
	case PlasticColor::SilverChrome: return "Fake Silver Chrome";
	case PlasticColor::Teal: return "Pool Teal";
	case PlasticColor::Navy: return "Midnight Navy";
	case PlasticColor::Chocolate: return "Chocolate";
	case PlasticColor::Coal: return "Coal Black";
	case PlasticColor::GlowMint: return "Glow Mint";
	case PlasticColor::Lavender: return "Lavender";
	default: return "?";
	}
}

const char* towerSkinName(TowerSkin s)
{
	switch (s) {
	case TowerSkin::Block: return "Toy Block";
	case TowerSkin::Sandcastle: return "Sandcastle";
	case TowerSkin::Rocket: return "Bottle Rocket";
	case TowerSkin::Fort: return "Cardboard Fort";
	case TowerSkin::BrickStack: return "Brick Stack";
	case TowerSkin::TinCan: return "Tin Can";
	case TowerSkin::BookStack: return "Book Stack";
	case TowerSkin::DiceTower: return "Dice Tower";
	default: return "?";
	}
}

const char* accessoryName(Accessory a)
{
	switch (a) {
	case Accessory::None: return "None";
	case Accessory::PartyHat: return "Party Hat";
	case Accessory::SantaHat: return "Santa Hat";
	case Accessory::Bandana: return "Bandana";
	case Accessory::StarMedal: return "Star Medal";
	case Accessory::PirateHat: return "Pirate Hat";
	case Accessory::ChefHat: return "Chef Hat";
	case Accessory::Antenna: return "Radio Antenna";
	case Accessory::Cape: return "Hero Cape";
	case Accessory::Backpack: return "Field Backpack";
	case Accessory::Flag: return "Team Flag";
	default: return "?";
	}
}

int plasticColorCount()
{
	return static_cast<int>(PlasticColor::Count);
}
int towerSkinCount()
{
	return static_cast<int>(TowerSkin::Count);
}
int accessoryCount()
{
	return static_cast<int>(Accessory::Count);
}

PlasticColor defaultPlasticForSeat(int seat)
{
	switch (seat % 4) {
	case 0: return PlasticColor::ClassicGreen;
	case 1: return PlasticColor::Blue;
	case 2: return PlasticColor::Red;
	default: return PlasticColor::Yellow;
	}
}

Cosmetics defaultCosmeticsForSeat(int seat)
{
	Cosmetics c;
	c.plastic = defaultPlasticForSeat(seat);
	c.towerSkin = TowerSkin::Block;
	c.accessory = Accessory::None;
	return c;
}

b3Vec3 plasticColorRgb(PlasticColor c)
{
	switch (c) {
	case PlasticColor::ClassicGreen: return { 0.22f, 0.78f, 0.38f };
	case PlasticColor::ArmyOlive: return { 0.42f, 0.52f, 0.28f };
	case PlasticColor::Blue: return { 0.28f, 0.52f, 0.95f };
	case PlasticColor::Red: return { 0.92f, 0.28f, 0.30f };
	case PlasticColor::Yellow: return { 0.95f, 0.82f, 0.22f };
	case PlasticColor::Purple: return { 0.62f, 0.38f, 0.92f };
	case PlasticColor::Orange: return { 0.95f, 0.52f, 0.18f };
	case PlasticColor::White: return { 0.92f, 0.92f, 0.90f };
	case PlasticColor::Pink: return { 0.95f, 0.48f, 0.72f };
	case PlasticColor::Camo: return { 0.38f, 0.48f, 0.32f };
	// v0.9 #142 — jellies read bright, chromes read hot.
	case PlasticColor::JellyLime: return { 0.62f, 0.95f, 0.42f };
	case PlasticColor::JellyBerry: return { 0.85f, 0.35f, 0.75f };
	case PlasticColor::GoldChrome: return { 0.98f, 0.78f, 0.25f };
	case PlasticColor::SilverChrome: return { 0.80f, 0.84f, 0.90f };
	case PlasticColor::Teal: return { 0.15f, 0.72f, 0.70f };
	case PlasticColor::Navy: return { 0.14f, 0.20f, 0.48f };
	case PlasticColor::Chocolate: return { 0.42f, 0.26f, 0.14f };
	case PlasticColor::Coal: return { 0.16f, 0.16f, 0.18f };
	case PlasticColor::GlowMint: return { 0.55f, 0.98f, 0.80f };
	case PlasticColor::Lavender: return { 0.72f, 0.62f, 0.92f };
	default: return { 0.5f, 0.5f, 0.5f };
	}
}

b3Vec3 towerColorRgb(const Cosmetics& cos)
{
	b3Vec3 base = plasticColorRgb(cos.plastic);
	switch (cos.towerSkin) {
	case TowerSkin::Sandcastle:
		return { base.x * 0.55f + 0.40f, base.y * 0.45f + 0.38f, base.z * 0.25f + 0.18f };
	case TowerSkin::Rocket:
		return { base.x * 0.35f + 0.55f, base.y * 0.35f + 0.55f, base.z * 0.35f + 0.58f };
	case TowerSkin::Fort:
		return { base.x * 0.5f + 0.28f, base.y * 0.45f + 0.22f, base.z * 0.35f + 0.12f };
	case TowerSkin::BrickStack: // Lego-ish: saturated plastic
		return { base.x * 0.9f + 0.1f, base.y * 0.85f, base.z * 0.85f };
	case TowerSkin::TinCan:
		return { base.x * 0.25f + 0.58f, base.y * 0.25f + 0.60f, base.z * 0.25f + 0.63f };
	case TowerSkin::BookStack:
		return { base.x * 0.4f + 0.35f, base.y * 0.35f + 0.22f, base.z * 0.3f + 0.14f };
	case TowerSkin::DiceTower:
		return { base.x * 0.2f + 0.75f, base.y * 0.2f + 0.75f, base.z * 0.2f + 0.75f };
	case TowerSkin::Block:
	default:
		return base;
	}
}

b3Vec3 soldierColorRgb(const Cosmetics& cos)
{
	return plasticColorRgb(cos.plastic);
}

b3Vec3 accessoryColorRgb(const Cosmetics& cos)
{
	switch (cos.accessory) {
	case Accessory::PartyHat: return { 0.95f, 0.25f, 0.55f };
	case Accessory::SantaHat: return { 0.85f, 0.12f, 0.12f };
	case Accessory::Bandana: return { 0.15f, 0.15f, 0.55f };
	case Accessory::StarMedal: return { 0.95f, 0.82f, 0.20f };
	case Accessory::PirateHat: return { 0.12f, 0.12f, 0.12f };
	case Accessory::ChefHat: return { 0.95f, 0.95f, 0.92f };
	case Accessory::Antenna: return { 0.55f, 0.58f, 0.62f };
	case Accessory::Cape: return { 0.80f, 0.15f, 0.20f };
	case Accessory::Backpack: return { 0.38f, 0.42f, 0.25f };
	case Accessory::Flag: return plasticColorRgb(cos.plastic);
	default: return plasticColorRgb(cos.plastic);
	}
}

void towerHalfExtents(TowerSkin skin, float& hx, float& hy, float& hz)
{
	switch (skin) {
	case TowerSkin::Sandcastle:
		hx = 0.16f;
		hy = 0.22f;
		hz = 0.16f;
		break;
	case TowerSkin::Rocket:
		hx = 0.08f;
		hy = 0.36f;
		hz = 0.08f;
		break;
	case TowerSkin::Fort:
		hx = 0.18f;
		hy = 0.20f;
		hz = 0.14f;
		break;
	case TowerSkin::BrickStack:
		hx = 0.13f;
		hy = 0.30f;
		hz = 0.13f;
		break;
	case TowerSkin::TinCan:
		hx = 0.11f;
		hy = 0.24f;
		hz = 0.11f;
		break;
	case TowerSkin::BookStack:
		hx = 0.20f;
		hy = 0.18f;
		hz = 0.15f;
		break;
	case TowerSkin::DiceTower:
		hx = 0.14f;
		hy = 0.26f;
		hz = 0.14f;
		break;
	case TowerSkin::Block:
	default:
		hx = 0.12f;
		hy = 0.28f;
		hz = 0.12f;
		break;
	}
}

void mapClearColor(MapId map, float& r, float& g, float& b)
{
	switch (map) {
	case MapId::Desert:
		r = 0.18f;
		g = 0.14f;
		b = 0.10f;
		break;
	case MapId::Backyard:
		r = 0.08f;
		g = 0.12f;
		b = 0.14f;
		break;
	case MapId::KidsBedroom: // night light glow
		r = 0.05f;
		g = 0.06f;
		b = 0.16f;
		break;
	case MapId::Garage:
		r = 0.10f;
		g = 0.10f;
		b = 0.11f;
		break;
	case MapId::PicnicBlanket: // sunny day
		r = 0.16f;
		g = 0.18f;
		b = 0.22f;
		break;
	case MapId::ChristmasTable: // warm candle light
		r = 0.14f;
		g = 0.08f;
		b = 0.06f;
		break;
	case MapId::LivingRoom:
	default:
		r = 0.07f;
		g = 0.09f;
		b = 0.14f;
		break;
	}
}

void applyAiCosmetics(Player& p, uint32_t seed)
{
	uint32_t s = seed ^ (static_cast<uint32_t>(p.id + 1) * 0x9E3779B9u);
	// xorshift-ish
	s ^= s << 13;
	s ^= s >> 17;
	s ^= s << 5;
	p.cosmetics.plastic = static_cast<PlasticColor>(s % static_cast<uint32_t>(PlasticColor::Count));
	p.cosmetics.towerSkin = static_cast<TowerSkin>((s >> 8) % static_cast<uint32_t>(TowerSkin::Count));
	p.cosmetics.accessory = static_cast<Accessory>((s >> 16) % static_cast<uint32_t>(Accessory::Count));
}

// --- v0.9 #146: local unlock track (fashion only — never gameplay) ---

namespace {

// Requirement per item: matches completed OR wins (whichever rule the item uses).
struct UnlockRule {
	int matchesNeeded = 0;
	int winsNeeded = 0;
};

UnlockRule plasticRule(PlasticColor c)
{
	switch (c) {
	case PlasticColor::JellyLime: return { 2, 0 };
	case PlasticColor::JellyBerry: return { 4, 0 };
	case PlasticColor::Teal: return { 6, 0 };
	case PlasticColor::Lavender: return { 8, 0 };
	case PlasticColor::Chocolate: return { 10, 0 };
	case PlasticColor::Navy: return { 0, 1 };
	case PlasticColor::Coal: return { 0, 2 };
	case PlasticColor::SilverChrome: return { 0, 3 };
	case PlasticColor::GlowMint: return { 0, 5 };
	case PlasticColor::GoldChrome: return { 0, 8 };
	default: return { 0, 0 };
	}
}

UnlockRule skinRule(TowerSkin s)
{
	switch (s) {
	case TowerSkin::BrickStack: return { 3, 0 };
	case TowerSkin::TinCan: return { 6, 0 };
	case TowerSkin::BookStack: return { 0, 2 };
	case TowerSkin::DiceTower: return { 0, 4 };
	default: return { 0, 0 };
	}
}

UnlockRule accessoryRule(Accessory a)
{
	switch (a) {
	case Accessory::PirateHat: return { 2, 0 };
	case Accessory::ChefHat: return { 4, 0 };
	case Accessory::Antenna: return { 6, 0 };
	case Accessory::Backpack: return { 0, 1 };
	case Accessory::Cape: return { 0, 3 };
	case Accessory::Flag: return { 0, 5 };
	default: return { 0, 0 };
	}
}

bool ruleMet(const UnlockRule& r, int matches, int wins)
{
	if (r.matchesNeeded > 0) {
		return matches >= r.matchesNeeded;
	}
	if (r.winsNeeded > 0) {
		return wins >= r.winsNeeded;
	}
	return true;
}

void ruleText(const UnlockRule& r, char* out, int cap)
{
	if (r.matchesNeeded > 0) {
		std::snprintf(out, static_cast<size_t>(cap), "play %d matches", r.matchesNeeded);
	} else if (r.winsNeeded > 0) {
		std::snprintf(out, static_cast<size_t>(cap), "win %d matches", r.winsNeeded);
	} else {
		out[0] = '\0';
	}
}

} // namespace

bool plasticUnlocked(PlasticColor c, int matches, int wins)
{
	return ruleMet(plasticRule(c), matches, wins);
}

bool towerSkinUnlocked(TowerSkin s, int matches, int wins)
{
	return ruleMet(skinRule(s), matches, wins);
}

bool accessoryUnlocked(Accessory a, int matches, int wins)
{
	return ruleMet(accessoryRule(a), matches, wins);
}

void plasticUnlockText(PlasticColor c, char* out, int cap)
{
	ruleText(plasticRule(c), out, cap);
}

void towerSkinUnlockText(TowerSkin s, char* out, int cap)
{
	ruleText(skinRule(s), out, cap);
}

void accessoryUnlockText(Accessory a, char* out, int cap)
{
	ruleText(accessoryRule(a), out, cap);
}

// --- v0.9 #145: named cosmetic sets ---

namespace {

const CosmeticSet kSets[] = {
	{ "Birthday Platoon", PlasticColor::Pink, TowerSkin::Block, Accessory::PartyHat },
	{ "North Pole Guard", PlasticColor::White, TowerSkin::DiceTower, Accessory::SantaHat },
	{ "Backyard Pirates", PlasticColor::Navy, TowerSkin::Fort, Accessory::PirateHat },
	{ "Breakfast Brigade", PlasticColor::Yellow, TowerSkin::TinCan, Accessory::ChefHat },
	{ "Study Hall Heroes", PlasticColor::Chocolate, TowerSkin::BookStack, Accessory::Cape },
	{ "Signal Corps", PlasticColor::SilverChrome, TowerSkin::Rocket, Accessory::Antenna },
};

} // namespace

int cosmeticSetCount()
{
	return static_cast<int>(sizeof(kSets) / sizeof(kSets[0]));
}

const CosmeticSet& cosmeticSet(int index)
{
	if (index < 0 || index >= cosmeticSetCount()) {
		index = 0;
	}
	return kSets[static_cast<size_t>(index)];
}

bool cosmeticSetUnlocked(int index, int matches, int wins)
{
	const CosmeticSet& s = cosmeticSet(index);
	return plasticUnlocked(s.plastic, matches, wins) && towerSkinUnlocked(s.towerSkin, matches, wins) &&
		   accessoryUnlocked(s.accessory, matches, wins);
}

} // namespace toy
