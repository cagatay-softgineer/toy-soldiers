#include "physics/table_scene.h"

#include "game/cards.h" // xorshift32 for FX randomness
#include "game/cosmetics.h"
#include "game/events.h"

#include "box3d/collision.h"

#include <algorithm>
#include <cmath>

namespace toy {

namespace {

b3BodyId invalidBody()
{
	return b3_nullBodyId;
}

} // namespace

b3Vec3 TableScene::seatPosition(int playerIndex, float tableHalf)
{
	const float edge = tableHalf * 0.82f;
	switch (playerIndex % 4) {
	case 0: return { 0.0f, 0.0f, -edge };
	case 1: return { edge, 0.0f, 0.0f };
	case 2: return { 0.0f, 0.0f, edge };
	default: return { -edge, 0.0f, 0.0f };
	}
}

b3Vec3 TableScene::seatColor(int playerIndex)
{
	return plasticColorRgb(defaultPlasticForSeat(playerIndex));
}

// v1.1 #141: felt dye presets (index 0 = map default).
namespace {
const struct {
	const char* name;
	float r, g, b;
} kFeltDyes[] = {
	{ "Map default", 0, 0, 0 },
	{ "Crimson", 0.62f, 0.16f, 0.18f },
	{ "Royal Blue", 0.16f, 0.28f, 0.62f },
	{ "Charcoal", 0.22f, 0.22f, 0.24f },
	{ "Cream", 0.85f, 0.80f, 0.68f },
	{ "Mint", 0.35f, 0.72f, 0.55f },
};
} // namespace

int TableScene::feltDyeCount()
{
	return static_cast<int>(sizeof(kFeltDyes) / sizeof(kFeltDyes[0]));
}

const char* TableScene::feltDyeName(int dyeIndex)
{
	if (dyeIndex < 0 || dyeIndex >= feltDyeCount()) {
		return kFeltDyes[0].name;
	}
	return kFeltDyes[dyeIndex].name;
}

void TableScene::resolvedTableColor(MapId map, float& r, float& g, float& b) const
{
	if (feltDye_ > 0 && feltDye_ < feltDyeCount()) {
		r = kFeltDyes[feltDye_].r;
		g = kFeltDyes[feltDye_].g;
		b = kFeltDyes[feltDye_].b;
		return;
	}
	mapTableColor(map, r, g, b);
}

void TableScene::create(const Match& match)
{
	destroy();

	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = { 0.0f, -9.81f, 0.0f };
	worldId_ = b3CreateWorld(&worldDef);
	alive_ = true;

	const float tableHalf = 1.6f;
	const float tableThick = 0.06f;
	float tr = 0.28f, tg = 0.55f, tb = 0.35f;
	resolvedTableColor(match.config.mapId, tr, tg, tb);
	createStaticBox({ 0.0f, -tableThick, 0.0f }, tableHalf, tableThick, tableHalf, { tr, tg, tb },
					BodyVisual::Kind::Table, -1);

	const float rimH = 0.08f;
	const float rimT = 0.05f;
	const b3Vec3 wood{ 0.45f, 0.28f, 0.14f };
	createStaticBox({ 0.0f, rimH * 0.5f, -tableHalf - rimT }, tableHalf + rimT * 2.0f, rimH, rimT, wood,
					BodyVisual::Kind::Rim, -1);
	createStaticBox({ 0.0f, rimH * 0.5f, tableHalf + rimT }, tableHalf + rimT * 2.0f, rimH, rimT, wood,
					BodyVisual::Kind::Rim, -1);
	createStaticBox({ -tableHalf - rimT, rimH * 0.5f, 0.0f }, rimT, rimH, tableHalf, wood, BodyVisual::Kind::Rim, -1);
	createStaticBox({ tableHalf + rimT, rimH * 0.5f, 0.0f }, rimT, rimH, tableHalf, wood, BodyVisual::Kind::Rim, -1);

	float fr = 0.35f, fg = 0.30f, fb = 0.25f;
	mapFloorColor(match.config.mapId, fr, fg, fb);
	createStaticBox({ 0.0f, -0.55f, 0.0f }, 4.0f, 0.1f, 4.0f, { fr, fg, fb }, BodyVisual::Kind::Table, -1);

	// --- Room props / atmosphere (M4) ---
	switch (match.config.mapId) {
	case MapId::LivingRoom:
		// Sofa block behind north edge
		createStaticBox({ 0.0f, 0.22f, -tableHalf - 0.55f }, 0.9f, 0.22f, 0.28f, { 0.45f, 0.22f, 0.22f },
						BodyVisual::Kind::Decor, -1);
		// Floor lamp
		createStaticBox({ tableHalf + 0.55f, 0.45f, -tableHalf * 0.3f }, 0.06f, 0.45f, 0.06f, { 0.75f, 0.7f, 0.55f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ tableHalf + 0.55f, 0.95f, -tableHalf * 0.3f }, 0.18f, 0.08f, 0.18f, { 0.95f, 0.9f, 0.6f },
						BodyVisual::Kind::Decor, -1);
		// Toy chest
		createStaticBox({ -tableHalf - 0.5f, 0.18f, tableHalf * 0.2f }, 0.35f, 0.18f, 0.25f, { 0.55f, 0.35f, 0.15f },
						BodyVisual::Kind::Decor, -1);
		break;
	case MapId::Desert:
		// Sand dunes
		createStaticBox({ -2.2f, 0.12f, -1.5f }, 0.7f, 0.12f, 0.5f, { 0.78f, 0.62f, 0.35f }, BodyVisual::Kind::Decor,
						-1);
		createStaticBox({ 2.0f, 0.18f, 1.2f }, 0.9f, 0.18f, 0.55f, { 0.72f, 0.55f, 0.30f }, BodyVisual::Kind::Decor, -1);
		// Cactus
		createStaticBox({ 2.3f, 0.35f, -1.0f }, 0.08f, 0.35f, 0.08f, { 0.25f, 0.55f, 0.28f }, BodyVisual::Kind::Decor,
						-1);
		createStaticBox({ 2.15f, 0.28f, -1.0f }, 0.12f, 0.06f, 0.06f, { 0.25f, 0.55f, 0.28f }, BodyVisual::Kind::Decor,
						-1);
		break;
	case MapId::Backyard:
		// Grass hedge
		createStaticBox({ 0.0f, 0.2f, tableHalf + 0.7f }, 1.4f, 0.2f, 0.15f, { 0.18f, 0.48f, 0.22f },
						BodyVisual::Kind::Decor, -1);
		// Picnic basket
		createStaticBox({ -tableHalf - 0.45f, 0.14f, -0.4f }, 0.22f, 0.14f, 0.16f, { 0.55f, 0.32f, 0.15f },
						BodyVisual::Kind::Decor, -1);
		// Tree trunk stump
		createStaticBox({ tableHalf + 0.6f, 0.2f, 0.8f }, 0.14f, 0.2f, 0.14f, { 0.4f, 0.28f, 0.15f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ tableHalf + 0.6f, 0.48f, 0.8f }, 0.28f, 0.12f, 0.28f, { 0.2f, 0.5f, 0.22f },
						BodyVisual::Kind::Decor, -1);
		break;
	case MapId::KidsBedroom:
		// Bunk bed silhouette (two stacked mattresses + posts)
		createStaticBox({ -tableHalf - 0.7f, 0.25f, -0.6f }, 0.5f, 0.08f, 0.3f, { 0.55f, 0.40f, 0.60f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ -tableHalf - 0.7f, 0.62f, -0.6f }, 0.5f, 0.08f, 0.3f, { 0.45f, 0.35f, 0.55f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ -tableHalf - 1.1f, 0.35f, -0.6f }, 0.05f, 0.35f, 0.05f, { 0.5f, 0.38f, 0.28f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ -tableHalf - 0.3f, 0.35f, -0.6f }, 0.05f, 0.35f, 0.05f, { 0.5f, 0.38f, 0.28f },
						BodyVisual::Kind::Decor, -1);
		// Night light — small warm glowing block (#136)
		createStaticBox({ tableHalf + 0.5f, 0.3f, 0.4f }, 0.06f, 0.10f, 0.06f, { 1.0f, 0.85f, 0.45f },
						BodyVisual::Kind::Decor, -1);
		// Scattered toy blocks
		createStaticBox({ 0.6f, 0.06f, tableHalf + 0.5f }, 0.07f, 0.06f, 0.07f, { 0.9f, 0.3f, 0.3f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ 0.85f, 0.05f, tableHalf + 0.65f }, 0.06f, 0.05f, 0.06f, { 0.3f, 0.5f, 0.9f },
						BodyVisual::Kind::Decor, -1);
		break;
	case MapId::Garage:
		// Pegboard with tools (back wall slab)
		createStaticBox({ 0.0f, 0.6f, -tableHalf - 0.5f }, 1.2f, 0.5f, 0.05f, { 0.45f, 0.40f, 0.35f },
						BodyVisual::Kind::Decor, -1);
		// Hanging wrench + hammer silhouettes
		createStaticBox({ -0.5f, 0.65f, -tableHalf - 0.42f }, 0.04f, 0.18f, 0.03f, { 0.6f, 0.62f, 0.66f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ 0.4f, 0.7f, -tableHalf - 0.42f }, 0.05f, 0.12f, 0.03f, { 0.55f, 0.35f, 0.2f },
						BodyVisual::Kind::Decor, -1);
		// Paint cans
		createStaticBox({ tableHalf + 0.5f, 0.14f, -0.3f }, 0.10f, 0.14f, 0.10f, { 0.7f, 0.7f, 0.72f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ tableHalf + 0.72f, 0.10f, -0.1f }, 0.08f, 0.10f, 0.08f, { 0.8f, 0.45f, 0.2f },
						BodyVisual::Kind::Decor, -1);
		// Oil stain on the bench (#137)
		createStaticBox({ 0.5f, 0.004f, 0.4f }, 0.28f, 0.003f, 0.20f, { 0.12f, 0.11f, 0.10f },
						BodyVisual::Kind::Decor, -1);
		break;
	case MapId::PicnicBlanket:
		// Watermelon slice + lemonade jug
		createStaticBox({ -tableHalf - 0.5f, 0.10f, 0.5f }, 0.20f, 0.10f, 0.08f, { 0.85f, 0.25f, 0.30f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ tableHalf + 0.45f, 0.20f, 0.6f }, 0.10f, 0.20f, 0.10f, { 0.95f, 0.85f, 0.45f },
						BodyVisual::Kind::Decor, -1);
		// Sandwich crumbs (ant bait, #138)
		createStaticBox({ 0.9f, 0.02f, -0.9f }, 0.05f, 0.02f, 0.05f, { 0.85f, 0.7f, 0.4f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ -0.7f, 0.015f, 0.9f }, 0.04f, 0.015f, 0.04f, { 0.85f, 0.72f, 0.42f },
						BodyVisual::Kind::Decor, -1);
		break;
	case MapId::ChristmasTable:
		// Mini tree (green tiers + trunk)
		createStaticBox({ -tableHalf - 0.6f, 0.15f, -0.5f }, 0.06f, 0.15f, 0.06f, { 0.4f, 0.26f, 0.12f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ -tableHalf - 0.6f, 0.40f, -0.5f }, 0.30f, 0.12f, 0.30f, { 0.10f, 0.45f, 0.18f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ -tableHalf - 0.6f, 0.62f, -0.5f }, 0.20f, 0.10f, 0.20f, { 0.12f, 0.50f, 0.20f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ -tableHalf - 0.6f, 0.80f, -0.5f }, 0.10f, 0.08f, 0.10f, { 0.14f, 0.55f, 0.22f },
						BodyVisual::Kind::Decor, -1);
		// Gift boxes
		createStaticBox({ tableHalf + 0.5f, 0.10f, 0.3f }, 0.12f, 0.10f, 0.12f, { 0.85f, 0.15f, 0.2f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ tableHalf + 0.72f, 0.07f, 0.55f }, 0.08f, 0.07f, 0.08f, { 0.95f, 0.8f, 0.25f },
						BodyVisual::Kind::Decor, -1);
		// Candle
		createStaticBox({ 0.0f, 0.12f, -tableHalf - 0.4f }, 0.04f, 0.12f, 0.04f, { 0.95f, 0.9f, 0.8f },
						BodyVisual::Kind::Decor, -1);
		createStaticBox({ 0.0f, 0.27f, -tableHalf - 0.4f }, 0.02f, 0.03f, 0.02f, { 1.0f, 0.7f, 0.3f },
						BodyVisual::Kind::Decor, -1);
		break;
	default:
		break;
	}

	// v0.9 #120-lite: felt variation patches — subtle two-tone panels break up the flat top.
	{
		float tr2, tg2, tb2;
		resolvedTableColor(match.config.mapId, tr2, tg2, tb2);
		const bool checker = (match.config.mapId == MapId::PicnicBlanket);
		for (int px = 0; px < 4; ++px) {
			for (int pz = 0; pz < 4; ++pz) {
				if ((px + pz) % 2 == 0) {
					continue;
				}
				const float cx = (static_cast<float>(px) - 1.5f) * (tableHalf * 0.5f);
				const float cz = (static_cast<float>(pz) - 1.5f) * (tableHalf * 0.5f);
				b3Vec3 c = checker ? b3Vec3{ 0.92f, 0.90f, 0.86f } // blanket checker: cream squares
								   : b3Vec3{ tr2 * 0.92f, tg2 * 0.92f, tb2 * 0.92f };
				createStaticBox({ cx, 0.0015f, cz }, tableHalf * 0.24f, 0.0012f, tableHalf * 0.24f, c,
								BodyVisual::Kind::Decor, -1);
			}
		}
	}

	for (int i = 0; i < match.config.playerCount; ++i) {
		const Player& pl = match.players[static_cast<size_t>(i)];
		if (pl.control == SeatControl::Empty && match.phase == Phase::Lobby) {
			// Still show preview seats in lobby.
		}
		const b3Vec3 seat = seatPosition(i, tableHalf);
		const Cosmetics& cos = pl.cosmetics;
		const b3Vec3 soldierCol = soldierColorRgb(cos);
		const b3Vec3 towerCol = towerColorRgb(cos);

		float thx, thy, thz;
		towerHalfExtents(cos.towerSkin, thx, thy, thz);
		towerBodies_[static_cast<size_t>(i)] =
			createStaticBox({ seat.x, thy, seat.z }, thx, thy, thz, towerCol, BodyVisual::Kind::Tower, i);

		// Accessory on tower (hat / medal) — static child visual
		if (cos.accessory != Accessory::None) {
			const b3Vec3 accCol = accessoryColorRgb(cos);
			const float ay = thy * 2.0f + 0.05f;
			switch (cos.accessory) {
			case Accessory::PartyHat:
			case Accessory::SantaHat:
				createStaticBox({ seat.x, ay, seat.z }, 0.06f, 0.07f, 0.06f, accCol, BodyVisual::Kind::Accessory, i);
				// pom-pom / tip
				createStaticBox({ seat.x, ay + 0.1f, seat.z }, 0.03f, 0.03f, 0.03f, { 0.95f, 0.95f, 0.95f },
								BodyVisual::Kind::Accessory, i);
				break;
			case Accessory::Bandana:
				createStaticBox({ seat.x, thy * 2.0f - 0.02f, seat.z + thz * 0.6f }, thx * 0.9f, 0.025f, 0.03f, accCol,
								BodyVisual::Kind::Accessory, i);
				break;
			case Accessory::StarMedal:
				createStaticBox({ seat.x, thy * 1.1f, seat.z + thz + 0.02f }, 0.05f, 0.05f, 0.02f, accCol,
								BodyVisual::Kind::Accessory, i);
				break;
			// --- v0.9 #144 wave ---
			case Accessory::PirateHat:
				// wide brim + crown
				createStaticBox({ seat.x, ay, seat.z }, 0.10f, 0.02f, 0.10f, accCol, BodyVisual::Kind::Accessory, i);
				createStaticBox({ seat.x, ay + 0.05f, seat.z }, 0.06f, 0.04f, 0.06f, accCol,
								BodyVisual::Kind::Accessory, i);
				break;
			case Accessory::ChefHat:
				// tall white toque
				createStaticBox({ seat.x, ay + 0.02f, seat.z }, 0.05f, 0.03f, 0.05f, accCol,
								BodyVisual::Kind::Accessory, i);
				createStaticBox({ seat.x, ay + 0.10f, seat.z }, 0.07f, 0.06f, 0.07f, accCol,
								BodyVisual::Kind::Accessory, i);
				break;
			case Accessory::Antenna:
				// mast + tip blinker
				createStaticBox({ seat.x, ay + 0.08f, seat.z }, 0.012f, 0.12f, 0.012f, accCol,
								BodyVisual::Kind::Accessory, i);
				createStaticBox({ seat.x, ay + 0.22f, seat.z }, 0.025f, 0.025f, 0.025f, { 0.95f, 0.25f, 0.25f },
								BodyVisual::Kind::Accessory, i);
				break;
			case Accessory::Cape:
				// draped panel behind the tower
				createStaticBox({ seat.x, thy, seat.z - thz - 0.03f }, thx * 1.1f, thy * 0.9f, 0.02f, accCol,
								BodyVisual::Kind::Accessory, i);
				break;
			case Accessory::Backpack:
				// pack on the tower's back
				createStaticBox({ seat.x, thy * 1.1f, seat.z - thz - 0.05f }, thx * 0.7f, 0.08f, 0.05f, accCol,
								BodyVisual::Kind::Accessory, i);
				break;
			case Accessory::Flag:
				// pole + team-color banner
				createStaticBox({ seat.x + thx + 0.04f, thy * 1.6f, seat.z }, 0.012f, thy * 1.6f, 0.012f,
								{ 0.55f, 0.45f, 0.3f }, BodyVisual::Kind::Accessory, i);
				createStaticBox({ seat.x + thx + 0.13f, thy * 2.8f, seat.z }, 0.08f, 0.05f, 0.01f, accCol,
								BodyVisual::Kind::Accessory, i);
				break;
			default:
				break;
			}
		}

		const b3Vec3 inward = b3Normalize({ -seat.x, 0.0f, -seat.z });
		for (int s = 0; s < kSoldiersPerPlayer; ++s) {
			const float side = (s - 1.5f) * 0.14f;
			const b3Vec3 sideAxis = b3Normalize(b3Cross({ 0.0f, 1.0f, 0.0f }, inward));
			const b3Vec3 pos = {
				seat.x + inward.x * 0.35f + sideAxis.x * side,
				0.09f,
				seat.z + inward.z * 0.35f + sideAxis.z * side,
			};
			soldierBodies_[static_cast<size_t>(i)][static_cast<size_t>(s)] =
				createDynamicBox(pos, 0.035f, 0.09f, 0.025f, 0.4f, soldierCol, BodyVisual::Kind::Soldier, i);

			// Tiny hat on first soldier if accessory is hat-type
			if (s == 0 && (cos.accessory == Accessory::PartyHat || cos.accessory == Accessory::SantaHat ||
						   cos.accessory == Accessory::PirateHat || cos.accessory == Accessory::ChefHat)) {
				createDynamicBox({ pos.x, 0.20f, pos.z }, 0.03f, 0.025f, 0.03f, 0.15f, accessoryColorRgb(cos),
								 BodyVisual::Kind::Accessory, i);
			}
		}
	}
}

void TableScene::destroy()
{
	if (alive_) {
		b3DestroyWorld(worldId_);
		alive_ = false;
		worldId_ = {};
	}
	visuals_.clear();
	for (auto& t : towerBodies_) {
		t = invalidBody();
	}
	for (auto& row : soldierBodies_) {
		for (auto& s : row) {
			s = invalidBody();
		}
	}
	particles_.clear();
	lastHp_ = { -1, -1, -1, -1 };
	flashTimer_ = {};
}

// --- v0.9 FX (#125, #127, #128, #158) ---

void TableScene::spawnParticle(const FxParticle& p)
{
	if (static_cast<int>(particles_.size()) >= kMaxParticles) {
		return; // #158: hard budget cap
	}
	particles_.push_back(p);
}

float TableScene::towerFlash(int seat) const
{
	if (seat < 0 || seat >= kMaxPlayers) {
		return 0.0f;
	}
	const float t = flashTimer_[static_cast<size_t>(seat)];
	return t > 0.0f ? (t / 0.3f) : 0.0f;
}

void TableScene::updateFx(const Match& match, float dt)
{
	if (!alive_) {
		return;
	}
	auto frand = [this](float lo, float hi) {
		return lo + (hi - lo) * (static_cast<float>(xorshift32(fxRng_) & 0xFFFF) / 65535.0f);
	};

	// #125: white flash on fresh tower damage.
	for (int i = 0; i < match.config.playerCount; ++i) {
		const int hp = match.players[static_cast<size_t>(i)].towerHp;
		if (lastHp_[static_cast<size_t>(i)] >= 0 && hp < lastHp_[static_cast<size_t>(i)]) {
			flashTimer_[static_cast<size_t>(i)] = 0.3f;
		}
		lastHp_[static_cast<size_t>(i)] = hp;
		if (flashTimer_[static_cast<size_t>(i)] > 0.0f) {
			flashTimer_[static_cast<size_t>(i)] -= dt;
		}
	}

	// #127: ambient event particles (spawn rates kept small — budget #158).
	const bool active = match.world.remainingTurns > 0;
	if (active && match.world.kind == EventKind::Rain) {
		for (int i = 0; i < 2; ++i) {
			FxParticle p;
			p.pos = { frand(-1.6f, 1.6f), frand(1.2f, 1.8f), frand(-1.6f, 1.6f) };
			p.vel = { 0.0f, -2.2f, 0.0f };
			const bool snow = match.config.mapId == MapId::ChristmasTable;
			p.color = snow ? b3Vec3{ 0.95f, 0.95f, 1.0f } : b3Vec3{ 0.45f, 0.65f, 0.95f };
			if (snow) {
				p.vel.y = -0.5f;
				p.vel.x = frand(-0.2f, 0.2f);
			}
			p.size = snow ? 0.016f : 0.010f;
			p.life = 1.6f;
			p.gravity = false;
			spawnParticle(p);
		}
	} else if (active && match.world.kind == EventKind::Sandstorm) {
		for (int i = 0; i < 2; ++i) {
			FxParticle p;
			p.pos = { frand(-2.0f, -1.6f), frand(0.05f, 0.6f), frand(-1.6f, 1.6f) };
			p.vel = { frand(1.8f, 3.0f), frand(-0.1f, 0.15f), frand(-0.3f, 0.3f) };
			p.color = { 0.78f, 0.62f, 0.35f };
			p.size = frand(0.012f, 0.028f);
			p.life = 1.4f;
			p.gravity = false;
			spawnParticle(p);
		}
	} else if (active && match.world.kind == EventKind::Flood && match.world.focusSeat >= 0) {
		const b3Vec3 seat = seatPosition(match.world.focusSeat);
		FxParticle p;
		p.pos = { seat.x + frand(-0.3f, 0.3f), 0.02f, seat.z + frand(-0.3f, 0.3f) };
		p.vel = { frand(-0.2f, 0.2f), frand(0.6f, 1.2f), frand(-0.2f, 0.2f) };
		p.color = { 0.35f, 0.55f, 0.95f };
		p.size = 0.014f;
		p.life = 0.8f;
		spawnParticle(p);
	} else if (active && match.world.kind == EventKind::Ants && match.world.focusSeat >= 0) {
		const b3Vec3 seat = seatPosition(match.world.focusSeat);
		FxParticle p;
		p.pos = { seat.x + frand(-0.4f, 0.4f), 0.012f, seat.z + frand(-0.4f, 0.4f) };
		p.vel = { frand(-0.25f, 0.25f), 0.0f, frand(-0.25f, 0.25f) };
		p.color = { 0.12f, 0.08f, 0.06f };
		p.size = 0.008f;
		p.life = 1.2f;
		p.gravity = false;
		spawnParticle(p);
	}

	// #128: victory confetti — one burst per match at the winner's tower.
	if (match.phase == Phase::GameOver && match.winner >= 0 && confettiMatchId_ != match.matchId) {
		confettiMatchId_ = match.matchId;
		const b3Vec3 seat = seatPosition(match.winner);
		const b3Vec3 palette[5] = {
			{ 0.95f, 0.3f, 0.3f }, { 0.3f, 0.7f, 0.95f }, { 0.95f, 0.85f, 0.3f },
			{ 0.4f, 0.9f, 0.5f },  { 0.9f, 0.5f, 0.9f },
		};
		for (int i = 0; i < 120; ++i) {
			FxParticle p;
			p.pos = { seat.x, 0.5f, seat.z };
			p.vel = { frand(-1.4f, 1.4f), frand(1.5f, 3.2f), frand(-1.4f, 1.4f) };
			p.color = palette[xorshift32(fxRng_) % 5];
			p.size = frand(0.012f, 0.024f);
			p.life = frand(1.2f, 2.4f);
			spawnParticle(p);
		}
	}

	// Integrate + expire.
	for (size_t i = 0; i < particles_.size();) {
		FxParticle& p = particles_[i];
		p.life -= dt;
		if (p.gravity) {
			p.vel.y -= 5.0f * dt;
		}
		p.pos.x += p.vel.x * dt;
		p.pos.y += p.vel.y * dt;
		p.pos.z += p.vel.z * dt;
		if (p.life <= 0.0f || p.pos.y < -0.4f) {
			particles_[i] = particles_.back();
			particles_.pop_back();
		} else {
			++i;
		}
	}
}

void TableScene::step(float dt)
{
	if (!alive_) {
		return;
	}
	const float timeStep = 1.0f / 60.0f;
	int steps = static_cast<int>(dt / timeStep + 0.5f);
	if (steps < 1) {
		steps = 1;
	}
	if (steps > 4) {
		steps = 4;
	}
	for (int i = 0; i < steps; ++i) {
		b3World_Step(worldId_, timeStep, 4);
	}
}

void TableScene::syncFromMatch(const Match& match)
{
	if (!alive_) {
		return;
	}
	for (BodyVisual& v : visuals_) {
		if ((v.kind == BodyVisual::Kind::Tower || v.kind == BodyVisual::Kind::Soldier ||
			 v.kind == BodyVisual::Kind::Accessory) &&
			v.playerId >= 0 && v.playerId < match.config.playerCount) {
			const Player& p = match.players[static_cast<size_t>(v.playerId)];
			b3Vec3 base;
			if (v.kind == BodyVisual::Kind::Tower) {
				base = towerColorRgb(p.cosmetics);
			} else if (v.kind == BodyVisual::Kind::Accessory) {
				base = accessoryColorRgb(p.cosmetics);
			} else {
				base = soldierColorRgb(p.cosmetics);
			}
			// v1.2 #147: local custom-hex plastic for my own seat (soldiers only).
			if (hexOverrideOn_ && v.playerId == hexOverrideSeat_ && v.kind == BodyVisual::Kind::Soldier) {
				base = { hexR_, hexG_, hexB_ };
			}
			if (p.eliminated) {
				v.color = { base.x * 0.25f, base.y * 0.25f, base.z * 0.25f };
			} else {
				const float hpT = static_cast<float>(p.towerHp) / static_cast<float>(std::max(1, p.towerMaxHp));
				b3Vec3 c = base;
				if (v.kind == BodyVisual::Kind::Tower) {
					c = { base.x * (0.45f + 0.55f * hpT), base.y * (0.45f + 0.55f * hpT),
						  base.z * (0.45f + 0.55f * hpT) };
					// v0.9 #125 crack tiers: below 50% the plastic bruises red, below 25% it chars.
					if (hpT < 0.25f) {
						c = { c.x * 0.55f + 0.18f, c.y * 0.45f, c.z * 0.45f };
					} else if (hpT < 0.5f) {
						c = { c.x * 0.8f + 0.1f, c.y * 0.75f, c.z * 0.75f };
					}
					// #125 damage flash: pop to white right after a hit.
					const float flash = towerFlash(v.playerId);
					if (flash > 0.0f) {
						const float k = flash * 0.85f;
						c = { c.x + (1.0f - c.x) * k, c.y + (1.0f - c.y) * k, c.z + (1.0f - c.z) * k };
					}
				}
				if (seatIsFlooded(match, v.playerId) && v.kind == BodyVisual::Kind::Tower) {
					c.x = c.x * 0.45f + 0.15f;
					c.y = c.y * 0.55f + 0.25f;
					c.z = c.z * 0.35f + 0.65f;
				}
				v.color = c;
			}
		}
		if (v.kind == BodyVisual::Kind::Table && v.playerId < 0 && v.halfExtents.x > 1.0f) {
			float tr, tg, tb;
			resolvedTableColor(match.config.mapId, tr, tg, tb);
			if (match.world.kind == EventKind::Sandstorm && match.world.remainingTurns > 0) {
				tr = tr * 0.7f + 0.35f;
				tg = tg * 0.7f + 0.28f;
				tb = tb * 0.5f + 0.08f;
			} else if (match.world.kind == EventKind::Rain && match.world.remainingTurns > 0) {
				tr = tr * 0.65f;
				tg = tg * 0.75f;
				tb = tb * 0.7f + 0.25f;
			}
			v.color = { tr, tg, tb };
		}
	}
}

void TableScene::consumeImpulse(Match& match)
{
	if (!alive_ || match.pendingPhysicsImpulse.frames <= 0) {
		return;
	}
	const int target = match.pendingPhysicsImpulse.targetPlayer;
	const float strength = match.pendingPhysicsImpulse.strength;
	match.pendingPhysicsImpulse.frames = 0;
	match.pendingPhysicsImpulse.targetPlayer = -1;

	// Dog event (#66): every seat's soldiers get shoved.
	if (target == kImpulseAllSeats) {
		for (int i = 0; i < match.config.playerCount; ++i) {
			impulseSeat(i, strength);
		}
		return;
	}

	if (target < 0 || target >= match.config.playerCount) {
		return;
	}
	impulseSeat(target, strength);
}

void TableScene::impulseSeat(int seat, float strength)
{
	const b3Vec3 seatPos = seatPosition(seat);
	const b3Vec3 inward = b3Normalize({ -seatPos.x, 0.0f, -seatPos.z });
	for (int s = 0; s < kSoldiersPerPlayer; ++s) {
		const b3BodyId body = soldierBodies_[static_cast<size_t>(seat)][static_cast<size_t>(s)];
		if (!b3Body_IsValid(body)) {
			continue;
		}
		const b3Vec3 impulse = {
			-inward.x * strength * 0.15f,
			strength * 0.08f,
			-inward.z * strength * 0.15f,
		};
		b3Body_ApplyLinearImpulseToCenter(body, impulse, true);
		b3Body_ApplyAngularImpulse(body, { 0.02f * strength, 0.05f * strength, -0.02f * strength }, true);
	}
}

void TableScene::paradeRest(const Match& match)
{
	// #76 (optional ruleset): stand fallen soldiers back at attention each round.
	if (!alive_) {
		return;
	}
	const float tableHalf = 1.6f;
	for (int i = 0; i < match.config.playerCount; ++i) {
		const b3Vec3 seat = seatPosition(i, tableHalf);
		const b3Vec3 inward = b3Normalize({ -seat.x, 0.0f, -seat.z });
		const b3Vec3 sideAxis = b3Normalize(b3Cross({ 0.0f, 1.0f, 0.0f }, inward));
		for (int s = 0; s < kSoldiersPerPlayer; ++s) {
			const b3BodyId body = soldierBodies_[static_cast<size_t>(i)][static_cast<size_t>(s)];
			if (!b3Body_IsValid(body)) {
				continue;
			}
			const float side = (s - 1.5f) * 0.14f;
			const b3Pos pos = {
				seat.x + inward.x * 0.35f + sideAxis.x * side,
				0.09f,
				seat.z + inward.z * 0.35f + sideAxis.z * side,
			};
			b3Body_SetTransform(body, pos, b3Quat_identity);
			b3Body_SetLinearVelocity(body, { 0.0f, 0.0f, 0.0f });
			b3Body_SetAngularVelocity(body, { 0.0f, 0.0f, 0.0f });
		}
	}
}

b3BodyId TableScene::createStaticBox(b3Pos pos, float hx, float hy, float hz, b3Vec3 color, BodyVisual::Kind kind,
									 int player)
{
	b3BodyDef def = b3DefaultBodyDef();
	def.type = b3_staticBody;
	def.position = pos;
	const b3BodyId body = b3CreateBody(worldId_, &def);

	b3BoxHull box = b3MakeBoxHull(hx, hy, hz);
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.6f;
	b3CreateHullShape(body, &shapeDef, &box.base);

	BodyVisual v;
	v.body = body;
	v.halfExtents = { hx, hy, hz };
	v.color = color;
	v.playerId = player;
	v.kind = kind;
	v.valid = true;
	visuals_.push_back(v);
	return body;
}

b3BodyId TableScene::createDynamicBox(b3Pos pos, float hx, float hy, float hz, float density, b3Vec3 color,
									  BodyVisual::Kind kind, int player)
{
	b3BodyDef def = b3DefaultBodyDef();
	def.type = b3_dynamicBody;
	def.position = pos;
	const b3BodyId body = b3CreateBody(worldId_, &def);

	b3BoxHull box = b3MakeBoxHull(hx, hy, hz);
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = density;
	shapeDef.baseMaterial.friction = 0.55f;
	shapeDef.baseMaterial.restitution = 0.1f;
	b3CreateHullShape(body, &shapeDef, &box.base);

	BodyVisual v;
	v.body = body;
	v.halfExtents = { hx, hy, hz };
	v.color = color;
	v.playerId = player;
	v.kind = kind;
	v.valid = true;
	visuals_.push_back(v);
	return body;
}

} // namespace toy
