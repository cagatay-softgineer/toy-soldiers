#include "game/snapshot.h"

#include <cstdio>
#include <cstring>

namespace toy {

namespace {

constexpr uint32_t kSnapMagic = 0x32594F54u; // 'TOY2'

struct Writer {
	std::vector<uint8_t> buf;

	void u8(uint8_t v) { buf.push_back(v); }
	void u16(uint16_t v)
	{
		buf.push_back(static_cast<uint8_t>(v & 0xff));
		buf.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
	}
	void u32(uint32_t v)
	{
		u16(static_cast<uint16_t>(v & 0xffff));
		u16(static_cast<uint16_t>((v >> 16) & 0xffff));
	}
	void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
	void bytes(const void* p, size_t n)
	{
		const auto* b = static_cast<const uint8_t*>(p);
		buf.insert(buf.end(), b, b + n);
	}
	void str24(const char* s)
	{
		char tmp[24] = {};
		if (s) {
			std::snprintf(tmp, sizeof(tmp), "%s", s);
		}
		bytes(tmp, 24);
	}
	void cards(const std::vector<CardInstance>& cards)
	{
		u16(static_cast<uint16_t>(cards.size()));
		for (const CardInstance& c : cards) {
			i32(c.defId);
			i32(c.instanceId);
		}
	}
};

struct Reader {
	const uint8_t* p = nullptr;
	const uint8_t* end = nullptr;
	bool ok = true;

	explicit Reader(const uint8_t* data, size_t size)
		: p(data)
		, end(data + size)
	{
	}

	size_t left() const { return static_cast<size_t>(end - p); }

	uint8_t u8()
	{
		if (left() < 1) {
			ok = false;
			return 0;
		}
		return *p++;
	}
	uint16_t u16()
	{
		const uint8_t a = u8();
		const uint8_t b = u8();
		return static_cast<uint16_t>(a | (b << 8));
	}
	uint32_t u32()
	{
		const uint16_t a = u16();
		const uint16_t b = u16();
		return static_cast<uint32_t>(a | (static_cast<uint32_t>(b) << 16));
	}
	int32_t i32() { return static_cast<int32_t>(u32()); }
	void bytes(void* out, size_t n)
	{
		if (left() < n) {
			ok = false;
			std::memset(out, 0, n);
			return;
		}
		std::memcpy(out, p, n);
		p += n;
	}
	void str24(char* out24)
	{
		bytes(out24, 24);
		out24[23] = '\0';
	}
	void cards(std::vector<CardInstance>& out)
	{
		const uint16_t n = u16();
		out.clear();
		out.reserve(n);
		for (uint16_t i = 0; i < n && ok; ++i) {
			CardInstance c;
			c.defId = i32();
			c.instanceId = i32();
			out.push_back(c);
		}
	}
};

void writePlayer(Writer& w, const Player& p)
{
	w.str24(p.name);
	w.u8(static_cast<uint8_t>(p.tower));
	w.i32(p.towerHp);
	w.i32(p.towerMaxHp);
	w.i32(p.shieldTurns);
	w.i32(p.attackBonusNext);
	w.u8(p.eliminated ? 1 : 0);
	w.u8(static_cast<uint8_t>(p.control));
	w.u8(p.ready ? 1 : 0);
	// M4 cosmetics
	w.u8(static_cast<uint8_t>(p.cosmetics.plastic));
	w.u8(static_cast<uint8_t>(p.cosmetics.towerSkin));
	w.u8(static_cast<uint8_t>(p.cosmetics.accessory));
	// v0.7
	w.i32(p.team);
	w.u8(p.firstAttackDone ? 1 : 0);
	w.u8(p.skipNextTurn ? 1 : 0);
	w.u8(static_cast<uint8_t>(p.persona));
	w.i32(p.bannedDefs[0]);
	w.i32(p.bannedDefs[1]);
	w.i32(p.extraDefs[0]);
	w.i32(p.extraDefs[1]);
	w.cards(p.hand);
	w.cards(p.deck);
	w.cards(p.discard);
	w.u16(static_cast<uint16_t>(p.soldiers.size()));
	for (const Soldier& s : p.soldiers) {
		w.i32(s.id);
		w.i32(s.owner);
		w.u8(s.alive ? 1 : 0);
	}
}

bool readPlayer(Reader& r, Player& p)
{
	r.str24(p.name);
	p.tower = static_cast<TowerType>(r.u8());
	p.towerHp = r.i32();
	p.towerMaxHp = r.i32();
	p.shieldTurns = r.i32();
	p.attackBonusNext = r.i32();
	p.eliminated = r.u8() != 0;
	p.control = static_cast<SeatControl>(r.u8());
	p.ready = r.u8() != 0;
	p.cosmetics.plastic = static_cast<PlasticColor>(r.u8());
	p.cosmetics.towerSkin = static_cast<TowerSkin>(r.u8());
	p.cosmetics.accessory = static_cast<Accessory>(r.u8());
	p.team = r.i32();
	p.firstAttackDone = r.u8() != 0;
	p.skipNextTurn = r.u8() != 0;
	p.persona = static_cast<AiPersona>(r.u8());
	p.bannedDefs[0] = r.i32();
	p.bannedDefs[1] = r.i32();
	p.extraDefs[0] = r.i32();
	p.extraDefs[1] = r.i32();
	r.cards(p.hand);
	r.cards(p.deck);
	r.cards(p.discard);
	const uint16_t sc = r.u16();
	p.soldiers.clear();
	p.soldiers.reserve(sc);
	for (uint16_t i = 0; i < sc && r.ok; ++i) {
		Soldier s;
		s.id = r.i32();
		s.owner = r.i32();
		s.alive = r.u8() != 0;
		s.bodyIndex = -1;
		p.soldiers.push_back(s);
	}
	return r.ok;
}

} // namespace

std::vector<uint8_t> serializeMatch(const Match& match)
{
	Writer w;
	w.u32(kSnapMagic);
	w.u16(kProtocolVersion);
	w.u8(static_cast<uint8_t>(match.phase));
	w.i32(match.activePlayer);
	w.i32(match.turnNumber);
	w.i32(match.winner);
	w.u32(match.rng);
	w.i32(match.nextInstanceId);
	w.i32(match.nextSoldierId);
	w.u32(match.syncGeneration);
	w.u32(match.matchId);

	w.i32(match.config.playerCount);
	w.u8(match.config.freeTargeting ? 1 : 0);
	w.u32(match.config.seed);
	w.u8(match.config.fillEmptyWithAI ? 1 : 0);
	w.u8(static_cast<uint8_t>(match.config.mapId));
	w.u8(match.config.eventsEnabled ? 1 : 0);
	// v0.7 config
	w.u8(static_cast<uint8_t>(match.config.mode));
	w.u8(static_cast<uint8_t>(match.config.aiLevel));
	w.i32(match.config.turnTimerSeconds);
	w.u8(match.config.paradeRest ? 1 : 0);

	// World event (M3)
	w.u8(static_cast<uint8_t>(match.world.kind));
	w.i32(match.world.remainingTurns);
	w.i32(match.world.focusSeat);
	w.u8(match.world.warning ? 1 : 0);
	w.i32(match.eventCooldown);
	// v0.7 Hot Potato
	w.i32(match.crownSeat);
	// v0.8
	w.u8(match.lobbyLocked ? 1 : 0);
	// v1.2 (protocol v7)
	w.u8(match.bannerColor);
	for (int i = 0; i < kMaxPlayers; ++i) {
		w.bytes(match.peerIp[i], sizeof(match.peerIp[i]));
	}

	for (int i = 0; i < kMaxPlayers; ++i) {
		writePlayer(w, match.players[static_cast<size_t>(i)]);
		w.i32(match.players[static_cast<size_t>(i)].id);
	}

	// Last 24 log lines
	const int logN = static_cast<int>(match.log.size());
	const int start = logN > 24 ? logN - 24 : 0;
	w.u16(static_cast<uint16_t>(logN - start));
	for (int i = start; i < logN; ++i) {
		const MatchEvent& e = match.log[static_cast<size_t>(i)];
		w.u8(static_cast<uint8_t>(e.type));
		w.i32(e.actor);
		w.i32(e.target);
		w.i32(e.value);
		const uint16_t len = static_cast<uint16_t>(e.text.size() > 200 ? 200 : e.text.size());
		w.u16(len);
		w.bytes(e.text.data(), len);
	}

	w.i32(match.pendingPhysicsImpulse.targetPlayer);
	// float as bits
	uint32_t bits = 0;
	static_assert(sizeof(float) == 4, "float");
	std::memcpy(&bits, &match.pendingPhysicsImpulse.strength, 4);
	w.u32(bits);
	w.i32(match.pendingPhysicsImpulse.frames);

	return w.buf;
}

bool deserializeMatch(Match& match, const uint8_t* data, size_t size)
{
	Reader r(data, size);
	if (r.u32() != kSnapMagic) {
		return false;
	}
	if (r.u16() != kProtocolVersion) {
		return false;
	}

	Match m;
	m.phase = static_cast<Phase>(r.u8());
	m.activePlayer = r.i32();
	m.turnNumber = r.i32();
	m.winner = r.i32();
	m.rng = r.u32();
	m.nextInstanceId = r.i32();
	m.nextSoldierId = r.i32();
	m.syncGeneration = r.u32();
	m.matchId = r.u32();

	m.config.playerCount = r.i32();
	m.config.freeTargeting = r.u8() != 0;
	m.config.seed = r.u32();
	m.config.fillEmptyWithAI = r.u8() != 0;
	m.config.mapId = static_cast<MapId>(r.u8());
	m.config.eventsEnabled = r.u8() != 0;
	m.config.mode = static_cast<GameMode>(r.u8());
	m.config.aiLevel = static_cast<AiLevel>(r.u8());
	m.config.turnTimerSeconds = r.i32();
	m.config.paradeRest = r.u8() != 0;

	m.world.kind = static_cast<EventKind>(r.u8());
	m.world.remainingTurns = r.i32();
	m.world.focusSeat = r.i32();
	m.world.warning = r.u8() != 0;
	m.eventCooldown = r.i32();
	m.crownSeat = r.i32();
	m.lobbyLocked = r.u8() != 0;
	m.bannerColor = r.u8();
	for (int i = 0; i < kMaxPlayers; ++i) {
		r.bytes(m.peerIp[i], sizeof(m.peerIp[i]));
		m.peerIp[i][sizeof(m.peerIp[i]) - 1] = '\0';
	}

	for (int i = 0; i < kMaxPlayers; ++i) {
		if (!readPlayer(r, m.players[static_cast<size_t>(i)])) {
			return false;
		}
		m.players[static_cast<size_t>(i)].id = r.i32();
	}

	const uint16_t logCount = r.u16();
	m.log.clear();
	m.log.reserve(logCount);
	for (uint16_t i = 0; i < logCount && r.ok; ++i) {
		MatchEvent e;
		e.type = static_cast<MatchEvent::Type>(r.u8());
		e.actor = r.i32();
		e.target = r.i32();
		e.value = r.i32();
		const uint16_t len = r.u16();
		if (r.left() < len) {
			return false;
		}
		e.text.assign(reinterpret_cast<const char*>(r.p), len);
		r.p += len;
		m.log.push_back(std::move(e));
	}

	m.pendingPhysicsImpulse.targetPlayer = r.i32();
	const uint32_t bits = r.u32();
	std::memcpy(&m.pendingPhysicsImpulse.strength, &bits, 4);
	m.pendingPhysicsImpulse.frames = r.i32();

	if (!r.ok) {
		return false;
	}
	match = std::move(m);
	return true;
}

// --- v1.2 #96: player-level delta snapshots ---
// Payload: magic 'TOYD', u16 version, u32 baseSync, u32 matchId, [core block],
// u8 dirtyMask, [dirty players], [log tail], [impulse]. Client applies only when
// its current state matches (matchId, baseSync); anything else asks for a resync.

namespace {

constexpr uint32_t kDeltaMagic = 0x44594F54u; // 'TOYD'

void writeCore(Writer& w, const Match& match)
{
	w.u8(static_cast<uint8_t>(match.phase));
	w.i32(match.activePlayer);
	w.i32(match.turnNumber);
	w.i32(match.winner);
	w.u32(match.rng);
	w.i32(match.nextInstanceId);
	w.i32(match.nextSoldierId);
	w.u32(match.syncGeneration);
	w.u32(match.matchId);
	w.i32(match.config.playerCount);
	w.u8(match.config.freeTargeting ? 1 : 0);
	w.u32(match.config.seed);
	w.u8(match.config.fillEmptyWithAI ? 1 : 0);
	w.u8(static_cast<uint8_t>(match.config.mapId));
	w.u8(match.config.eventsEnabled ? 1 : 0);
	w.u8(static_cast<uint8_t>(match.config.mode));
	w.u8(static_cast<uint8_t>(match.config.aiLevel));
	w.i32(match.config.turnTimerSeconds);
	w.u8(match.config.paradeRest ? 1 : 0);
	w.u8(static_cast<uint8_t>(match.world.kind));
	w.i32(match.world.remainingTurns);
	w.i32(match.world.focusSeat);
	w.u8(match.world.warning ? 1 : 0);
	w.i32(match.eventCooldown);
	w.i32(match.crownSeat);
	w.u8(match.lobbyLocked ? 1 : 0);
	w.u8(match.bannerColor);
}

void readCore(Reader& r, Match& m)
{
	m.phase = static_cast<Phase>(r.u8());
	m.activePlayer = r.i32();
	m.turnNumber = r.i32();
	m.winner = r.i32();
	m.rng = r.u32();
	m.nextInstanceId = r.i32();
	m.nextSoldierId = r.i32();
	m.syncGeneration = r.u32();
	m.matchId = r.u32();
	m.config.playerCount = r.i32();
	m.config.freeTargeting = r.u8() != 0;
	m.config.seed = r.u32();
	m.config.fillEmptyWithAI = r.u8() != 0;
	m.config.mapId = static_cast<MapId>(r.u8());
	m.config.eventsEnabled = r.u8() != 0;
	m.config.mode = static_cast<GameMode>(r.u8());
	m.config.aiLevel = static_cast<AiLevel>(r.u8());
	m.config.turnTimerSeconds = r.i32();
	m.config.paradeRest = r.u8() != 0;
	m.world.kind = static_cast<EventKind>(r.u8());
	m.world.remainingTurns = r.i32();
	m.world.focusSeat = r.i32();
	m.world.warning = r.u8() != 0;
	m.eventCooldown = r.i32();
	m.crownSeat = r.i32();
	m.lobbyLocked = r.u8() != 0;
	m.bannerColor = r.u8();
}

void writeLogTail(Writer& w, const Match& match)
{
	const int logN = static_cast<int>(match.log.size());
	const int start = logN > 24 ? logN - 24 : 0;
	w.u16(static_cast<uint16_t>(logN - start));
	for (int i = start; i < logN; ++i) {
		const MatchEvent& e = match.log[static_cast<size_t>(i)];
		w.u8(static_cast<uint8_t>(e.type));
		w.i32(e.actor);
		w.i32(e.target);
		w.i32(e.value);
		const uint16_t len = static_cast<uint16_t>(e.text.size() > 200 ? 200 : e.text.size());
		w.u16(len);
		w.bytes(e.text.data(), len);
	}
}

bool readLogTail(Reader& r, Match& m)
{
	const uint16_t logCount = r.u16();
	m.log.clear();
	m.log.reserve(logCount);
	for (uint16_t i = 0; i < logCount && r.ok; ++i) {
		MatchEvent e;
		e.type = static_cast<MatchEvent::Type>(r.u8());
		e.actor = r.i32();
		e.target = r.i32();
		e.value = r.i32();
		const uint16_t len = r.u16();
		if (r.left() < len) {
			return false;
		}
		e.text.assign(reinterpret_cast<const char*>(r.p), len);
		r.p += len;
		m.log.push_back(std::move(e));
	}
	return r.ok;
}

std::vector<uint8_t> playerBlob(const Player& p)
{
	Writer w;
	writePlayer(w, p);
	return w.buf;
}

} // namespace

std::vector<uint8_t> serializeMatchDelta(const Match& match, const Match& prev)
{
	Writer w;
	w.u32(kDeltaMagic);
	w.u16(kProtocolVersion);
	w.u32(prev.syncGeneration);
	writeCore(w, match);

	uint8_t dirty = 0;
	std::vector<uint8_t> blobs[kMaxPlayers];
	for (int i = 0; i < kMaxPlayers; ++i) {
		blobs[i] = playerBlob(match.players[static_cast<size_t>(i)]);
		const std::vector<uint8_t> prevBlob = playerBlob(prev.players[static_cast<size_t>(i)]);
		if (blobs[i] != prevBlob || match.players[static_cast<size_t>(i)].id != prev.players[static_cast<size_t>(i)].id) {
			dirty |= static_cast<uint8_t>(1 << i);
		}
	}
	w.u8(dirty);
	for (int i = 0; i < kMaxPlayers; ++i) {
		if (dirty & (1 << i)) {
			w.bytes(blobs[i].data(), blobs[i].size());
			w.i32(match.players[static_cast<size_t>(i)].id);
		}
	}
	writeLogTail(w, match);
	w.i32(match.pendingPhysicsImpulse.targetPlayer);
	uint32_t bits = 0;
	std::memcpy(&bits, &match.pendingPhysicsImpulse.strength, 4);
	w.u32(bits);
	w.i32(match.pendingPhysicsImpulse.frames);
	return w.buf;
}

DeltaResult applyMatchDelta(Match& match, const uint8_t* data, size_t size)
{
	Reader r(data, size);
	if (r.u32() != kDeltaMagic || r.u16() != kProtocolVersion) {
		return DeltaResult::Bad;
	}
	const uint32_t baseSync = r.u32();
	if (baseSync != match.syncGeneration) {
		return DeltaResult::NeedResync; // #92 path: base mismatch — ask for a full snapshot
	}
	const uint32_t prevMatchId = match.matchId;
	readCore(r, match);
	if (match.matchId != prevMatchId) {
		return DeltaResult::NeedResync; // deltas never span matches
	}
	const uint8_t dirty = r.u8();
	for (int i = 0; i < kMaxPlayers; ++i) {
		if (dirty & (1 << i)) {
			if (!readPlayer(r, match.players[static_cast<size_t>(i)])) {
				return DeltaResult::Bad;
			}
			match.players[static_cast<size_t>(i)].id = r.i32();
		}
	}
	if (!readLogTail(r, match)) {
		return DeltaResult::Bad;
	}
	match.pendingPhysicsImpulse.targetPlayer = r.i32();
	const uint32_t bits = r.u32();
	std::memcpy(&match.pendingPhysicsImpulse.strength, &bits, 4);
	match.pendingPhysicsImpulse.frames = r.i32();
	return r.ok ? DeltaResult::Ok : DeltaResult::Bad;
}

std::vector<uint8_t> serializeLobby(const Match& match)
{
	// Lobby uses the same full snapshot for simplicity (phase == Lobby).
	return serializeMatch(match);
}

bool deserializeLobby(Match& match, const uint8_t* data, size_t size)
{
	return deserializeMatch(match, data, size);
}

} // namespace toy
