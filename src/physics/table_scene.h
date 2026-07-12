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

// v0.9 #127/#128/#158: tiny CPU particle (rendered as a small cube, hard pool cap).
struct FxParticle {
	b3Vec3 pos{};
	b3Vec3 vel{};
	b3Vec3 color{ 1.0f, 1.0f, 1.0f };
	float size = 0.02f;
	float life = 1.0f;
	bool gravity = true;
};

// Box3D scene for the childhood tabletop battlefield.
// +Y up. Table centered at origin, towers at four sides.
class TableScene {
public:
	static constexpr int kMaxParticles = 256; // #158 budget cap

	void create(const Match& match);
	void destroy();
	void step(float dt);
	void syncFromMatch(const Match& match);
	void consumeImpulse(Match& match);
	// v0.7 #76: reset every soldier to its parade position (optional round-start ruleset).
	void paradeRest(const Match& match);
	// v0.9: ambient/event particles, damage flashes, victory confetti. Call once per frame.
	void updateFx(const Match& match, float dt);

	b3WorldId world() const { return worldId_; }
	const std::vector<BodyVisual>& visuals() const { return visuals_; }
	const std::vector<FxParticle>& particles() const { return particles_; }
	// 0..1 white-flash intensity for a tower that just took damage (#125).
	float towerFlash(int seat) const;

	// Seat world positions (table edge midpoints) for 4 players: N E S W.
	static b3Vec3 seatPosition(int playerIndex, float tableHalf = 1.6f);
	static b3Vec3 seatColor(int playerIndex);

private:
	void impulseSeat(int seat, float strength);
	void spawnParticle(const FxParticle& p);
	b3BodyId createStaticBox(b3Pos pos, float hx, float hy, float hz, b3Vec3 color, BodyVisual::Kind kind, int player);
	b3BodyId createDynamicBox(b3Pos pos, float hx, float hy, float hz, float density, b3Vec3 color, BodyVisual::Kind kind,
							  int player);

	b3WorldId worldId_{};
	bool alive_ = false;
	std::vector<BodyVisual> visuals_;
	std::array<b3BodyId, kMaxPlayers> towerBodies_{};
	std::array<std::array<b3BodyId, kSoldiersPerPlayer>, kMaxPlayers> soldierBodies_{};

	// v0.9 FX state
	std::vector<FxParticle> particles_;
	uint32_t fxRng_ = 0xF00DFEEDu;
	std::array<int, kMaxPlayers> lastHp_{ -1, -1, -1, -1 };
	std::array<float, kMaxPlayers> flashTimer_{};
	uint32_t confettiMatchId_ = 0; // fired once per match (#128)
};

} // namespace toy
