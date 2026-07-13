#include "game/match.h"
#include "game/rules.h"
#include "game/snapshot.h"

#include <cstdio>
#include <cstring>

// v1.2 #97: lockstep determinism harness. Stricter than the golden-seed replay gate —
// that one only compares the final outcome; this one steps two independent sims one
// action at a time and diffs the FULL serialized state after every single action, so a
// divergence is caught at the exact turn it happens instead of just "somewhere".

using namespace toy;

namespace {

int g_fails = 0;

#define CHECK(cond, msg)                                                                                               \
	do {                                                                                                               \
		if (!(cond)) {                                                                                                 \
			std::printf("FAIL: %s\n", msg);                                                                            \
			++g_fails;                                                                                                 \
		}                                                                                                              \
	} while (0)

bool lockstepRun(uint32_t seed, GameMode mode, int maxSteps)
{
	MatchConfig cfg;
	cfg.playerCount = 4;
	cfg.seed = seed;
	cfg.eventsEnabled = true;
	cfg.fillEmptyWithAI = true;
	cfg.mode = mode;

	Match a, b;
	startMatch(a, cfg);
	startMatch(b, cfg);

	// Instances just constructed must already agree byte-for-byte.
	const auto snapA0 = serializeMatch(a);
	const auto snapB0 = serializeMatch(b);
	if (snapA0 != snapB0) {
		std::printf("FAIL: %s seed %u diverged at match start (before any action)\n", gameModeName(mode), seed);
		return false;
	}

	int step = 0;
	while (a.phase == Phase::Playing && step < maxSteps) {
		const bool okA = autoPlayBest(a);
		const bool okB = autoPlayBest(b);
		if (okA != okB) {
			std::printf("FAIL: %s seed %u step %d: one sim's autoPlayBest returned differently\n", gameModeName(mode),
						seed, step);
			return false;
		}
		const auto snapA = serializeMatch(a);
		const auto snapB = serializeMatch(b);
		if (snapA != snapB) {
			std::printf("FAIL: %s seed %u step %d: full-state divergence (turn %d vs %d, sync %u vs %u)\n",
						gameModeName(mode), seed, step, a.turnNumber, b.turnNumber, a.syncGeneration, b.syncGeneration);
			return false;
		}
		++step;
	}
	if (a.phase != Phase::GameOver) {
		std::printf("FAIL: %s seed %u did not finish within %d steps\n", gameModeName(mode), seed, maxSteps);
		return false;
	}
	return true;
}

} // namespace

int main()
{
	const uint32_t seeds[] = { 3u, 71u, 2026u, 0xC0FFEEu };
	const GameMode modes[] = { GameMode::ClassicFFA, GameMode::QuickDuel, GameMode::Teams2v2, GameMode::HotPotato };

	for (GameMode mode : modes) {
		for (uint32_t seed : seeds) {
			const bool ok = lockstepRun(seed, mode, 600);
			CHECK(ok, "lockstep run");
			if (ok) {
				std::printf("OK lockstep %s seed %u\n", gameModeName(mode), seed);
			}
		}
	}

	if (g_fails) {
		std::printf("%d failure(s)\n", g_fails);
		return 1;
	}
	std::printf("OK toy_lockstep_test\n");
	return 0;
}
