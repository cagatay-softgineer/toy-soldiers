#include "game/match.h"
#include "game/rules.h"

#include <cstdio>

// v0.6 P1 #30: same seed + autoplay sequence → identical rules outcome (no physics).
static bool runSeed(uint32_t seed, toy::Match& out)
{
	using namespace toy;
	MatchConfig cfg;
	cfg.playerCount = 4;
	cfg.seed = seed;
	cfg.eventsEnabled = true;
	cfg.fillEmptyWithAI = true;
	cfg.mapId = MapId::Desert;
	startMatch(out, cfg);
	int safety = 0;
	while (out.phase == Phase::Playing && safety < 500) {
		if (!autoPlayBest(out)) {
			return false;
		}
		++safety;
	}
	return out.phase == Phase::GameOver;
}

int main()
{
	using namespace toy;
	const uint32_t seeds[] = { 1u, 42u, 999u, 12345u, 0xC0FFEEu };
	int fails = 0;
	for (uint32_t seed : seeds) {
		Match a, b;
		if (!runSeed(seed, a) || !runSeed(seed, b)) {
			std::printf("FAIL seed %u did not finish\n", seed);
			++fails;
			continue;
		}
		if (a.winner != b.winner || a.turnNumber != b.turnNumber) {
			std::printf("FAIL seed %u winner/turns mismatch %d/%d vs %d/%d\n", seed, a.winner, a.turnNumber, b.winner,
						b.turnNumber);
			++fails;
			continue;
		}
		for (int i = 0; i < a.config.playerCount; ++i) {
			if (a.players[static_cast<size_t>(i)].towerHp != b.players[static_cast<size_t>(i)].towerHp) {
				std::printf("FAIL seed %u seat %d HP %d vs %d\n", seed, i, a.players[static_cast<size_t>(i)].towerHp,
							b.players[static_cast<size_t>(i)].towerHp);
				++fails;
			}
		}
		std::printf("OK seed %u winner=%d turns=%d\n", seed, a.winner, a.turnNumber);
	}
	if (fails) {
		std::printf("%d golden-seed failures\n", fails);
		return 1;
	}
	std::printf("OK golden seed replay\n");
	return 0;
}
