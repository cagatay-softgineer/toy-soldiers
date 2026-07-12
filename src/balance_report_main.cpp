#include "game/events.h"
#include "game/match.h"
#include "game/rules.h"
#include "game/summary.h"

#include <cstdio>
#include <cstring>

// Monte-Carlo balance smoke: auto-play many seeds, report length / winner / events.
int main()
{
	using namespace toy;

	constexpr int kTrials = 80;
	constexpr MapId kMaps[] = { MapId::LivingRoom, MapId::Desert, MapId::Backyard };
	constexpr TowerType kTowers[] = { TowerType::MachineGun, TowerType::Sniper };

	int totalTurns = 0;
	int totalEvents = 0;
	int finished = 0;
	int winsBySeat[kMaxPlayers] = {};
	int winsByTower[2] = {}; // 0 MG, 1 Sniper
	int tooLong = 0;

	// v0.9 #150: per-trial CSV for the balance dashboard.
	FILE* csv = std::fopen("balance_report.csv", "wb");
	if (csv) {
		std::fprintf(csv, "trial,seed,map,turns,winnerSeat,winnerTower,worldEvents\n");
	}

	for (int t = 0; t < kTrials; ++t) {
		MatchConfig cfg;
		cfg.playerCount = 4;
		cfg.fillEmptyWithAI = true;
		cfg.eventsEnabled = true;
		cfg.seed = 1000u + static_cast<uint32_t>(t) * 17u;
		cfg.mapId = kMaps[t % 3];

		Match m;
		startMatch(m, cfg);
		// Alternating tower types for seats
		// Two MG + two Sniper, shuffled across seats (avoids seat×tower confound).
		TowerType assign[4] = { TowerType::MachineGun, TowerType::MachineGun, TowerType::Sniper, TowerType::Sniper };
		for (int i = 3; i > 0; --i) {
			const int j = static_cast<int>(cfg.seed % static_cast<uint32_t>(i + 1));
			const TowerType tmp = assign[i];
			assign[i] = assign[j];
			assign[j] = tmp;
			cfg.seed = cfg.seed * 1664525u + 1013904223u;
		}
		for (int i = 0; i < m.config.playerCount; ++i) {
			m.players[static_cast<size_t>(i)].tower = assign[i];
			const int maxHp = (assign[i] == TowerType::MachineGun) ? 36 : 34;
			m.players[static_cast<size_t>(i)].towerMaxHp = maxHp;
			m.players[static_cast<size_t>(i)].towerHp = maxHp;
		}

		int safety = 0;
		while (m.phase == Phase::Playing && safety < 600) {
			if (!autoPlayBest(m)) {
				break;
			}
			++safety;
		}

		if (m.phase != Phase::GameOver) {
			++tooLong;
			if (csv) {
				std::fprintf(csv, "%d,%u,%s,%d,-1,timeout,0\n", t, cfg.seed, mapName(cfg.mapId), m.turnNumber);
			}
			continue;
		}
		++finished;
		const MatchSummary sum = buildSummary(m);
		totalTurns += sum.turns;
		totalEvents += sum.worldEvents;
		if (sum.winner >= 0 && sum.winner < kMaxPlayers) {
			++winsBySeat[sum.winner];
			const TowerType tw = m.players[static_cast<size_t>(sum.winner)].tower;
			++winsByTower[tw == TowerType::Sniper ? 1 : 0];
		}
		if (csv) {
			std::fprintf(csv, "%d,%u,%s,%d,%d,%s,%d\n", t, cfg.seed, mapName(cfg.mapId), sum.turns, sum.winner,
						 sum.winner >= 0 ? towerTypeName(m.players[static_cast<size_t>(sum.winner)].tower) : "draw",
						 sum.worldEvents);
		}
	}
	if (csv) {
		std::fclose(csv);
		std::printf("CSV written: balance_report.csv\n");
	}

	std::printf("=== Toy Soldiers M5 Balance Report ===\n");
	std::printf("Trials: %d  finished: %d  timeout: %d\n", kTrials, finished, tooLong);
	if (finished > 0) {
		std::printf("Avg turns: %.1f\n", static_cast<double>(totalTurns) / finished);
		std::printf("Avg world-event log lines: %.1f\n", static_cast<double>(totalEvents) / finished);
		std::printf("Wins by seat: ");
		for (int i = 0; i < kMaxPlayers; ++i) {
			std::printf("%d=%d  ", i, winsBySeat[i]);
		}
		std::printf("\n");
		std::printf("Wins by tower: MG=%d  Sniper=%d\n", winsByTower[0], winsByTower[1]);
		const double sniperRate = static_cast<double>(winsByTower[1]) / finished;
		std::printf("Sniper win rate: %.0f%% (target band ~40–60%%)\n", sniperRate * 100.0);
		const double avgTurns = static_cast<double>(totalTurns) / finished;
		std::printf("Game length: %s\n",
					(avgTurns >= 25.0 && avgTurns <= 70.0) ? "OK band" : "OUT of preferred 25–70 turns");
	}

	// Gate: enough finished games
	if (finished < kTrials / 2) {
		std::printf("FAIL: too many timeouts\n");
		return 1;
	}
	std::printf("OK balance report\n");
	return 0;
}
