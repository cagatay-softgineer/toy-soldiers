#include "game/replay.h"

#include <cstdio>
#include <cstring>

namespace toy {

namespace {
constexpr uint32_t kReplayMagic = 0x52594F54u; // 'TOYR' LE
}

void ReplayRecorder::begin(const MatchConfig& config)
{
	active_ = true;
	finished_ = false;
	config_ = config;
	actions_.clear();
	actions_.reserve(256);
}

void ReplayRecorder::recordPlay(int handIndex, int targetPlayer)
{
	if (!active_) {
		return;
	}
	Action a;
	a.isPlay = true;
	a.handIndex = handIndex;
	a.targetPlayer = targetPlayer;
	actions_.push_back(a);
}

void ReplayRecorder::recordEndTurn()
{
	if (!active_) {
		return;
	}
	Action a;
	a.isPlay = false;
	actions_.push_back(a);
}

void ReplayRecorder::finish(int winner, int turnNumber, const int finalHp[kMaxPlayers])
{
	winner_ = winner;
	turnNumber_ = turnNumber;
	for (int i = 0; i < kMaxPlayers; ++i) {
		finalHp_[i] = finalHp[i];
	}
	finished_ = true;
}

bool ReplayRecorder::save(const char* path) const
{
	if (!path || !finished_) {
		return false;
	}
	FILE* f = std::fopen(path, "wb");
	if (!f) {
		return false;
	}
	auto w32 = [f](uint32_t v) { std::fwrite(&v, sizeof(v), 1, f); };
	auto wi32 = [&](int32_t v) { w32(static_cast<uint32_t>(v)); };
	auto w8 = [f](uint8_t v) { std::fwrite(&v, sizeof(v), 1, f); };

	w32(kReplayMagic);
	w32(kProtocolVersion);
	wi32(config_.playerCount);
	w8(config_.freeTargeting ? 1 : 0);
	w32(config_.seed);
	w8(config_.fillEmptyWithAI ? 1 : 0);
	w8(static_cast<uint8_t>(config_.mapId));
	w8(config_.eventsEnabled ? 1 : 0);
	w8(static_cast<uint8_t>(config_.mode));
	w8(static_cast<uint8_t>(config_.aiLevel));
	wi32(config_.turnTimerSeconds);
	w8(config_.paradeRest ? 1 : 0);

	w32(static_cast<uint32_t>(actions_.size()));
	for (const Action& a : actions_) {
		w8(a.isPlay ? 1 : 0);
		wi32(a.handIndex);
		wi32(a.targetPlayer);
	}

	wi32(winner_);
	wi32(turnNumber_);
	for (int i = 0; i < kMaxPlayers; ++i) {
		wi32(finalHp_[i]);
	}

	const bool ok = !std::ferror(f);
	std::fclose(f);
	return ok;
}

bool replayLoad(const char* path, ReplayFile& out)
{
	if (!path) {
		return false;
	}
	FILE* f = std::fopen(path, "rb");
	if (!f) {
		return false;
	}
	auto r32 = [f](bool& ok) -> uint32_t {
		uint32_t v = 0;
		if (std::fread(&v, sizeof(v), 1, f) != 1) {
			ok = false;
		}
		return v;
	};
	auto ri32 = [&](bool& ok) -> int32_t { return static_cast<int32_t>(r32(ok)); };
	auto r8 = [f](bool& ok) -> uint8_t {
		uint8_t v = 0;
		if (std::fread(&v, sizeof(v), 1, f) != 1) {
			ok = false;
		}
		return v;
	};

	bool ok = true;
	if (r32(ok) != kReplayMagic) {
		std::fclose(f);
		return false;
	}
	r32(ok); // version — informational; format is stable across v1.2 protocol bumps
	out.config.playerCount = ri32(ok);
	out.config.freeTargeting = r8(ok) != 0;
	out.config.seed = r32(ok);
	out.config.fillEmptyWithAI = r8(ok) != 0;
	out.config.mapId = static_cast<MapId>(r8(ok));
	out.config.eventsEnabled = r8(ok) != 0;
	out.config.mode = static_cast<GameMode>(r8(ok));
	out.config.aiLevel = static_cast<AiLevel>(r8(ok));
	out.config.turnTimerSeconds = ri32(ok);
	out.config.paradeRest = r8(ok) != 0;

	const uint32_t n = r32(ok);
	if (!ok || n > 100000) {
		std::fclose(f);
		return false;
	}
	out.actions.clear();
	out.actions.reserve(n);
	for (uint32_t i = 0; i < n && ok; ++i) {
		ReplayFile::Action a;
		a.isPlay = r8(ok) != 0;
		a.handIndex = ri32(ok);
		a.targetPlayer = ri32(ok);
		out.actions.push_back(a);
	}
	out.winner = ri32(ok);
	out.turnNumber = ri32(ok);
	for (int i = 0; i < kMaxPlayers; ++i) {
		out.finalHp[i] = ri32(ok);
	}
	std::fclose(f);
	return ok;
}

} // namespace toy
