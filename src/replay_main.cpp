#include "game/match.h"
#include "game/replay.h"
#include "game/rules.h"

#include <cstdio>
#include <cstring>

// v1.2 #107: .toyrec — record and verify replay files. Also power-user CLI:
//   toy_replay_test --record <path> --seed N       write a self-play recording
//   toy_replay_test --verify <path>                re-simulate + check the trailer
//   toy_replay_test                                 built-in self-test (verify.ps1 gate)

using namespace toy;

namespace {

bool runSelfPlayRecording(uint32_t seed, ReplayRecorder& recorder, Match& out)
{
	MatchConfig cfg;
	cfg.playerCount = 4;
	cfg.seed = seed;
	cfg.eventsEnabled = true;
	cfg.fillEmptyWithAI = true;
	recorder.begin(cfg);
	startMatch(out, cfg);
	int safety = 0;
	while (out.phase == Phase::Playing && safety < 500) {
		if (!autoPlayBest(out, &recorder)) {
			return false;
		}
		++safety;
	}
	if (out.phase != Phase::GameOver) {
		return false;
	}
	int finalHp[kMaxPlayers];
	for (int i = 0; i < kMaxPlayers; ++i) {
		finalHp[i] = out.players[static_cast<size_t>(i)].towerHp;
	}
	recorder.finish(out.winner, out.turnNumber, finalHp);
	return true;
}

// Re-simulate a loaded .toyrec by applying its actions directly (no AI, no session —
// this is intentionally independent of AI heuristics so future AI changes cannot
// invalidate old recordings, satisfying the "dispute review" intent of #107).
bool replayApply(const ReplayFile& rec, Match& out)
{
	startMatch(out, rec.config);
	for (const auto& a : rec.actions) {
		if (out.phase != Phase::Playing) {
			return false; // action past game-over — corrupt or truncated file
		}
		if (a.isPlay) {
			if (!applyCardPlay(out, a.handIndex, a.targetPlayer)) {
				return false;
			}
		} else {
			endTurn(out);
		}
	}
	return true;
}

bool verifyFile(const char* path)
{
	ReplayFile rec;
	if (!replayLoad(path, rec)) {
		std::printf("FAIL: could not load %s\n", path);
		return false;
	}
	Match replayed;
	if (!replayApply(rec, replayed)) {
		std::printf("FAIL: replay of %s diverged before completion\n", path);
		return false;
	}
	bool ok = true;
	if (replayed.phase != Phase::GameOver) {
		std::printf("FAIL: replay did not reach game over\n");
		ok = false;
	}
	if (replayed.winner != rec.winner) {
		std::printf("FAIL: winner mismatch replay=%d recorded=%d\n", replayed.winner, rec.winner);
		ok = false;
	}
	if (replayed.turnNumber != rec.turnNumber) {
		std::printf("FAIL: turn count mismatch replay=%d recorded=%d\n", replayed.turnNumber, rec.turnNumber);
		ok = false;
	}
	for (int i = 0; i < kMaxPlayers; ++i) {
		if (replayed.players[static_cast<size_t>(i)].towerHp != rec.finalHp[i]) {
			std::printf("FAIL: seat %d final HP mismatch replay=%d recorded=%d\n", i,
						replayed.players[static_cast<size_t>(i)].towerHp, rec.finalHp[i]);
			ok = false;
		}
	}
	if (ok) {
		std::printf("OK replay verified: %s (winner=%d turns=%d, %zu actions)\n", path, rec.winner, rec.turnNumber,
					rec.actions.size());
	}
	return ok;
}

} // namespace

int main(int argc, char** argv)
{
	if (argc >= 3 && std::strcmp(argv[1], "--record") == 0) {
		const char* path = argv[2];
		uint32_t seed = 12345u;
		for (int i = 3; i < argc - 1; ++i) {
			if (std::strcmp(argv[i], "--seed") == 0) {
				seed = static_cast<uint32_t>(std::strtoul(argv[i + 1], nullptr, 10));
			}
		}
		ReplayRecorder recorder;
		Match m;
		if (!runSelfPlayRecording(seed, recorder, m)) {
			std::printf("FAIL: self-play did not finish\n");
			return 1;
		}
		if (!recorder.save(path)) {
			std::printf("FAIL: could not write %s\n", path);
			return 1;
		}
		std::printf("OK recorded %s (seed=%u winner=%d turns=%d)\n", path, seed, m.winner, m.turnNumber);
		return 0;
	}

	if (argc >= 3 && std::strcmp(argv[1], "--verify") == 0) {
		return verifyFile(argv[2]) ? 0 : 1;
	}

	// --- default: self-test used by scripts/verify.ps1 ---
	const char* tmp = "toy_replay_selftest.toyrec";
	int fails = 0;
	const uint32_t seeds[] = { 1u, 42u, 999u };
	for (uint32_t seed : seeds) {
		ReplayRecorder recorder;
		Match m;
		if (!runSelfPlayRecording(seed, recorder, m)) {
			std::printf("FAIL: self-play seed %u did not finish\n", seed);
			++fails;
			continue;
		}
		if (!recorder.save(tmp)) {
			std::printf("FAIL: save seed %u\n", seed);
			++fails;
			continue;
		}
		if (!verifyFile(tmp)) {
			++fails;
			continue;
		}
	}

	// A truncated/corrupt file must fail cleanly, not crash.
	{
		ReplayFile bad;
		if (replayLoad("this_file_does_not_exist.toyrec", bad)) {
			std::printf("FAIL: missing file should not load\n");
			++fails;
		} else {
			std::printf("OK missing-file load rejected cleanly\n");
		}
	}
	std::remove(tmp);

	if (fails) {
		std::printf("%d failure(s)\n", fails);
		return 1;
	}
	std::printf("OK toy_replay self-test\n");
	return 0;
}
