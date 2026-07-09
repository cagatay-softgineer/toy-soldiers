#include "game/match.h"
#include "game/rules.h"
#include "physics/table_scene.h"

#include <cstdio>

// Headless smoke: start a match, auto-play until someone wins, step physics.
int main()
{
	toy::MatchConfig cfg;
	cfg.playerCount = 4;
	cfg.seed = 12345;
	cfg.freeTargeting = true;
	cfg.fillEmptyWithAI = true;

	toy::Match match;
	toy::startMatch(match, cfg);

	toy::TableScene scene;
	scene.create(match);

	std::printf("Toy Soldiers headless sim — seed %u, %d players\n", cfg.seed, cfg.playerCount);

	const int maxTurns = 400;
	int safety = 0;
	while (match.phase == toy::Phase::Playing && safety < maxTurns) {
		const bool ok = toy::autoPlayBest(match);
		if (!ok) {
			std::printf("autoPlayBest failed at turn %d\n", match.turnNumber);
			return 1;
		}
		scene.consumeImpulse(match);
		scene.step(1.0f / 30.0f);
		scene.syncFromMatch(match);
		++safety;
	}

	if (match.phase != toy::Phase::GameOver) {
		std::printf("FAIL: match did not finish in %d auto-plays\n", maxTurns);
		scene.destroy();
		return 2;
	}

	if (match.winner >= 0) {
		std::printf("Winner: %s (tower HP %d) after %d turns\n",
					match.players[static_cast<size_t>(match.winner)].name,
					match.players[static_cast<size_t>(match.winner)].towerHp, match.turnNumber);
	} else {
		std::printf("Draw after %d turns\n", match.turnNumber);
	}

	std::printf("Last log lines:\n");
	const int start = static_cast<int>(match.log.size()) > 8 ? static_cast<int>(match.log.size()) - 8 : 0;
	for (int i = start; i < static_cast<int>(match.log.size()); ++i) {
		std::printf("  %s\n", match.log[static_cast<size_t>(i)].text.c_str());
	}

	scene.destroy();
	std::printf("OK\n");
	return 0;
}
