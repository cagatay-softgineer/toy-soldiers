#pragma once

#include "game/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace toy {

// v1.2 #107: .toyrec replay recording — dispute review, not a media format. Records
// every applied action (both human and AI) in application order, independent of the
// AI heuristics that chose them, so a replay reproduces the match even if the AI code
// changes later. Deliberately standalone (no Match dependency) so match.h/session.h
// can forward-declare it without a circular include.
class ReplayRecorder {
public:
	void begin(const MatchConfig& config);
	// #63/#151 etc. all resolve to one of these two primitives.
	void recordPlay(int handIndex, int targetPlayer);
	void recordEndTurn();
	// Trailer checksum (winner, turns, per-seat final HP) written at save time so a
	// verifier can confirm the replay reproduces the exact same outcome.
	void finish(int winner, int turnNumber, const int finalHp[kMaxPlayers]);
	bool save(const char* path) const;
	bool active() const { return active_; }

private:
	struct Action {
		bool isPlay = false; // false = EndTurn
		int handIndex = 0;
		int targetPlayer = 0;
	};

	bool active_ = false;
	MatchConfig config_{};
	std::vector<Action> actions_;
	int winner_ = -1;
	int turnNumber_ = 0;
	int finalHp_[kMaxPlayers] = {};
	bool finished_ = false;
};

// Loaded record — config, actions, and the trailer to check against.
struct ReplayFile {
	MatchConfig config{};
	struct Action {
		bool isPlay = false;
		int handIndex = 0;
		int targetPlayer = 0;
	};
	std::vector<Action> actions;
	int winner = -1;
	int turnNumber = 0;
	int finalHp[kMaxPlayers] = {};
};

bool replayLoad(const char* path, ReplayFile& out);

} // namespace toy
