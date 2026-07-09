#include "physics/table_scene.h"

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
	mapTableColor(match.config.mapId, tr, tg, tb);
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
	default:
		break;
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
			if (s == 0 && (cos.accessory == Accessory::PartyHat || cos.accessory == Accessory::SantaHat)) {
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
			if (p.eliminated) {
				v.color = { base.x * 0.25f, base.y * 0.25f, base.z * 0.25f };
			} else {
				const float hpT = static_cast<float>(p.towerHp) / static_cast<float>(std::max(1, p.towerMaxHp));
				b3Vec3 c = base;
				if (v.kind == BodyVisual::Kind::Tower) {
					c = { base.x * (0.45f + 0.55f * hpT), base.y * (0.45f + 0.55f * hpT),
						  base.z * (0.45f + 0.55f * hpT) };
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
			mapTableColor(match.config.mapId, tr, tg, tb);
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

	if (target < 0 || target >= match.config.playerCount) {
		return;
	}

	const b3Vec3 seat = seatPosition(target);
	const b3Vec3 inward = b3Normalize({ -seat.x, 0.0f, -seat.z });
	for (int s = 0; s < kSoldiersPerPlayer; ++s) {
		const b3BodyId body = soldierBodies_[static_cast<size_t>(target)][static_cast<size_t>(s)];
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
