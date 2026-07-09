#pragma once

#include "game/match.h"

#include "box3d/box3d.h"
#include "box3d/id.h"
#include "box3d/math_functions.h"
#include "box3d/types.h"

#include <array>
#include <vector>

namespace toy {

struct BodyVisual {
	b3BodyId body{};
	b3Vec3 halfExtents{ 0.1f, 0.1f, 0.1f };
	b3Vec3 color{ 0.4f, 0.8f, 0.4f };
	int playerId = -1;
	enum class Kind : uint8_t { Table, Tower, Soldier, Rim, Decor, Accessory } kind = Kind::Table;
	bool valid = false;
};

// Box3D scene for the childhood tabletop battlefield.
// +Y up. Table centered at origin, towers at four sides.
class TableScene {
public:
	void create(const Match& match);
	void destroy();
	void step(float dt);
	void syncFromMatch(const Match& match);
	void consumeImpulse(Match& match);

	b3WorldId world() const { return worldId_; }
	const std::vector<BodyVisual>& visuals() const { return visuals_; }

	// Seat world positions (table edge midpoints) for 4 players: N E S W.
	static b3Vec3 seatPosition(int playerIndex, float tableHalf = 1.6f);
	static b3Vec3 seatColor(int playerIndex);

private:
	b3BodyId createStaticBox(b3Pos pos, float hx, float hy, float hz, b3Vec3 color, BodyVisual::Kind kind, int player);
	b3BodyId createDynamicBox(b3Pos pos, float hx, float hy, float hz, float density, b3Vec3 color, BodyVisual::Kind kind,
							  int player);

	b3WorldId worldId_{};
	bool alive_ = false;
	std::vector<BodyVisual> visuals_;
	std::array<b3BodyId, kMaxPlayers> towerBodies_{};
	std::array<std::array<b3BodyId, kSoldiersPerPlayer>, kMaxPlayers> soldierBodies_{};
};

} // namespace toy
