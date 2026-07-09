#include "game/cosmetics.h"

#include "game/cards.h"

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
	}
	return "?";
}

const char* towerSkinName(TowerSkin s)
{
	switch (s) {
	case TowerSkin::Block: return "Toy Block";
	case TowerSkin::Sandcastle: return "Sandcastle";
	case TowerSkin::Rocket: return "Bottle Rocket";
	case TowerSkin::Fort: return "Cardboard Fort";
	}
	return "?";
}

const char* accessoryName(Accessory a)
{
	switch (a) {
	case Accessory::None: return "None";
	case Accessory::PartyHat: return "Party Hat";
	case Accessory::SantaHat: return "Santa Hat";
	case Accessory::Bandana: return "Bandana";
	case Accessory::StarMedal: return "Star Medal";
	}
	return "?";
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
	}
	return { 0.5f, 0.5f, 0.5f };
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

} // namespace toy
