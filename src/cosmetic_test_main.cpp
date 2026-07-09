#include "game/cosmetics.h"
#include "game/match.h"
#include "game/snapshot.h"
#include "net/protocol.h"

#include <cstdio>

static int g_fails = 0;
#define CHECK(c, m)                                                                                                    \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			std::printf("FAIL: %s\n", m);                                                                              \
			++g_fails;                                                                                                 \
		}                                                                                                              \
	} while (0)

int main()
{
	using namespace toy;

	// Defaults per seat
	CHECK(defaultPlasticForSeat(0) == PlasticColor::ClassicGreen, "seat0 green");
	CHECK(defaultPlasticForSeat(1) == PlasticColor::Blue, "seat1 blue");

	// RGB distinct
	const b3Vec3 g = plasticColorRgb(PlasticColor::ClassicGreen);
	const b3Vec3 r = plasticColorRgb(PlasticColor::Red);
	CHECK(g.y > g.x && r.x > r.y, "green/red channels");

	// Tower extents
	float hx, hy, hz;
	towerHalfExtents(TowerSkin::Rocket, hx, hy, hz);
	CHECK(hy > hx, "rocket tall");
	towerHalfExtents(TowerSkin::Sandcastle, hx, hy, hz);
	CHECK(hx >= 0.14f, "sandcastle wide");

	// Snapshot v3 cosmetics
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 3;
		cfg.mapId = MapId::LivingRoom;
		startMatch(m, cfg);
		m.players[0].cosmetics.plastic = PlasticColor::Pink;
		m.players[0].cosmetics.towerSkin = TowerSkin::Rocket;
		m.players[0].cosmetics.accessory = Accessory::SantaHat;
		const auto bytes = serializeMatch(m);
		Match m2;
		CHECK(deserializeMatch(m2, bytes.data(), bytes.size()), "deserialize v3");
		CHECK(m2.players[0].cosmetics.plastic == PlasticColor::Pink, "plastic");
		CHECK(m2.players[0].cosmetics.towerSkin == TowerSkin::Rocket, "skin");
		CHECK(m2.players[0].cosmetics.accessory == Accessory::SantaHat, "acc");
		std::printf("OK snapshot cosmetics (%zu bytes)\n", bytes.size());
	}

	// Protocol pack/unpack
	{
		Cosmetics cos{ PlasticColor::Purple, TowerSkin::Fort, Accessory::StarMedal };
		const auto msg = net::makeSetCosmetics(cos);
		net::MsgHeader hdr{};
		const uint8_t* payload = nullptr;
		size_t psz = 0;
		CHECK(net::parseHeader(msg.data(), msg.size(), hdr, payload, psz), "header");
		CHECK(static_cast<net::MsgType>(hdr.type) == net::MsgType::SetCosmetics, "type");
		Cosmetics out{};
		CHECK(net::readSetCosmetics(payload, psz, out), "read");
		CHECK(out.plastic == cos.plastic && out.towerSkin == cos.towerSkin && out.accessory == cos.accessory, "eq");
		std::printf("OK SetCosmetics message\n");
	}

	// AI cosmetics deterministic-ish
	{
		Player p;
		p.id = 2;
		applyAiCosmetics(p, 42);
		Player p2;
		p2.id = 2;
		applyAiCosmetics(p2, 42);
		CHECK(p.cosmetics.plastic == p2.cosmetics.plastic, "ai deterministic");
		std::printf("OK AI cosmetics plastic=%s\n", plasticColorName(p.cosmetics.plastic));
	}

	if (g_fails) {
		std::printf("%d fails\n", g_fails);
		return 1;
	}
	std::printf("OK M4 cosmetic tests\n");
	return 0;
}
