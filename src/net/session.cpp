#include "net/session.h"

#include "game/cards.h"
#include "game/play_reason.h"
#include "game/rules.h"
#include "game/snapshot.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace toy {

namespace {

constexpr float kPeerTimeoutSec = 8.0f;   // #93: no data for 8s → disconnect
constexpr float kHeartbeatSec = 1.0f;     // #93: host heartbeat cadence
constexpr float kBeaconSec = 1.0f;        // #85: LAN beacon cadence
constexpr float kReclaimWindowSec = 60.0f; // #100: seat reserved after mid-match drop
constexpr float kChatMinIntervalSec = 0.5f; // #81 rate limit
constexpr float kIntentMinIntervalSec = 0.1f; // #106 lite rate limit
constexpr double kBeaconEntryTtl = 5.0;

double secondsBetween(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b)
{
	return std::chrono::duration<double>(b - a).count();
}

} // namespace

// --- LanDiscovery (#85/#86) ---

bool LanDiscovery::start()
{
	net::netInit();
	entries_.clear();
	if (!sock_.openReceiver(kBeaconPort)) {
		net::netShutdown();
		return false;
	}
	return true;
}

void LanDiscovery::stop()
{
	sock_.close();
	entries_.clear();
	net::netShutdown();
}

void LanDiscovery::update()
{
	if (!sock_.valid()) {
		return;
	}
	const auto now = std::chrono::steady_clock::now();
	const double nowSec = std::chrono::duration<double>(now.time_since_epoch()).count();

	uint8_t buf[256];
	char ip[64];
	uint16_t fromPort = 0;
	for (int i = 0; i < 32; ++i) {
		const int n = sock_.recvFrom(buf, sizeof(buf), ip, fromPort);
		if (n <= 0) {
			break;
		}
		net::BeaconInfo info;
		if (!net::parseBeacon(buf, static_cast<size_t>(n), info)) {
			continue;
		}
		std::snprintf(info.fromIp, sizeof(info.fromIp), "%s", ip);
		bool merged = false;
		for (Entry& e : entries_) {
			if (std::strcmp(e.info.fromIp, info.fromIp) == 0 && e.info.tcpPort == info.tcpPort) {
				e.info = info;
				e.lastSeen = nowSec;
				merged = true;
				break;
			}
		}
		if (!merged) {
			Entry e;
			e.info = info;
			e.lastSeen = nowSec;
			entries_.push_back(e);
		}
	}
	entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
								  [nowSec](const Entry& e) { return nowSec - e.lastSeen > kBeaconEntryTtl; }),
				   entries_.end());
}

const net::BeaconInfo* LanDiscovery::findByCode(const char* code) const
{
	if (!code || !code[0]) {
		return nullptr;
	}
	for (const Entry& e : entries_) {
		bool same = true;
		for (int i = 0; i < kRoomCodeLen - 1; ++i) {
			const char a = static_cast<char>(std::toupper(static_cast<unsigned char>(code[i])));
			const char b = static_cast<char>(std::toupper(static_cast<unsigned char>(e.info.code[i])));
			if (a != b) {
				same = false;
				break;
			}
			if (a == '\0') {
				break;
			}
		}
		if (same) {
			return &e.info;
		}
	}
	return nullptr;
}

// --- NetSession ---

NetSession::NetSession()
{
	net::netInit();
	startTime_ = Clock::now();
}

NetSession::~NetSession()
{
	shutdown(nullptr);
	net::netShutdown();
}

uint32_t NetSession::nowMs() const
{
	return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime_).count());
}

bool NetSession::isConnected() const
{
	if (mode_ == AppMode::Offline) {
		return true;
	}
	if (mode_ == AppMode::Host) {
		return listener_.valid();
	}
	return clientSock_.valid() && clientWelcomed_;
}

void NetSession::setStatus(const char* s)
{
	status_ = s ? s : "";
}

void NetSession::setConnectionLost(const char* reason)
{
	connectionLost_ = true;
	connectionLostReason_ = reason ? reason : "Connection lost.";
	setStatus(connectionLostReason_.c_str());
	setError(connectionLostReason_.c_str());
}

void NetSession::clearConnectionLost()
{
	connectionLost_ = false;
	connectionLostReason_.clear();
}

void NetSession::startOffline(Match& match, const MatchConfig& cfg)
{
	shutdown(&match);
	clearConnectionLost();
	mode_ = AppMode::Offline;
	localSeat_ = 0;
	startMatch(match, cfg);
	// Hotseat: all human-local.
	for (int i = 0; i < match.config.playerCount; ++i) {
		match.players[static_cast<size_t>(i)].control = SeatControl::HumanLocal;
	}
	bumpSync(match);
	sceneNeedsRebuild_ = true;
	lastSeenMatchId_ = match.matchId;
	lastSeenSync_ = match.syncGeneration;
	setStatus("Offline hotseat");
}

bool NetSession::hostLobby(Match& match, const MatchConfig& cfg, const char* hostName, uint16_t port)
{
	shutdown(&match);
	clearConnectionLost();
	if (!listener_.listen(port)) {
		setStatus("Host failed: port bind error");
		mode_ = AppMode::Offline;
		return false;
	}
	mode_ = AppMode::Host;
	listenPort_ = port;
	localSeat_ = 0;
	localName_ = (hostName && hostName[0]) ? hostName : "Host";
	initLobby(match, cfg);
	match.players[0].control = SeatControl::HumanLocal;
	match.players[0].setName(localName_.c_str());
	match.players[0].ready = true;
	bumpSync(match);
	sceneNeedsRebuild_ = true; // room/map preview
	lastSeenMatchId_ = match.matchId;
	lastSeenSync_ = match.syncGeneration;

	// v0.8 #85: room code + LAN beacon.
	uint32_t codeRng = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
												 Clock::now().time_since_epoch())
												 .count()) ^
					   (static_cast<uint32_t>(port) << 16) ^ cfg.seed;
	for (int i = 0; i < kRoomCodeLen - 1; ++i) {
		roomCode_[i] = static_cast<char>('A' + randRange(codeRng, 0, 25));
	}
	roomCode_[kRoomCodeLen - 1] = '\0';
	beaconSock_.openSender(); // best effort — discovery is optional
	lastBeacon_ = Clock::now() - std::chrono::seconds(5);
	lastHeartbeat_ = Clock::now();
	reclaimToken_ = {};
	rematchVoted_ = {};
	rematchAccept_ = {};

	char buf[128];
	std::snprintf(buf, sizeof(buf), "Hosting lobby on port %u — room code %s", static_cast<unsigned>(port), roomCode_);
	setStatus(buf);
	return true;
}

bool NetSession::joinLobby(Match& match, const char* host, uint16_t port, const char* playerName)
{
	// Note: clientToken_ survives shutdown so a quick rejoin can reclaim a seat (#99/#100).
	const uint32_t keepToken = clientToken_;
	shutdown(&match);
	clearConnectionLost();
	clientToken_ = keepToken;
	localName_ = (playerName && playerName[0]) ? playerName : "Client";
	if (!clientSock_.connect(host ? host : "127.0.0.1", port)) {
		setStatus("Join failed: connect error");
		mode_ = AppMode::Offline;
		return false;
	}
	mode_ = AppMode::Client;
	localSeat_ = -1;
	clientWelcomed_ = false;
	clientCodec_.reset();
	clientSendQ_.clear();
	clientSendQ_.enqueue(net::FrameCodec::pack(net::makeHello(localName_.c_str(), clientToken_)));
	clientLastRecv_ = Clock::now();
	initLobby(match, MatchConfig{});
	match.phase = Phase::Lobby;
	setStatus("Connecting…");
	return true;
}

void NetSession::shutdown(Match* /*match*/)
{
	for (auto& c : clients_) {
		c.sock.close();
		c.codec.reset();
		c.sendQ.clear();
		c.alive = false;
		c.seat = -1;
		c.name.clear();
		c.token = 0;
		c.lastIntentId = 0;
		c.pingMs = -1;
	}
	listener_.close();
	beaconSock_.close();
	roomCode_[0] = '\0';
	clientSock_.close();
	clientCodec_.reset();
	clientSendQ_.clear();
	clientWelcomed_ = false;
	listenPort_ = 0;
	reclaimToken_ = {};
	rematchVoted_ = {};
	rematchAccept_ = {};
	if (mode_ != AppMode::Offline) {
		mode_ = AppMode::Offline;
	}
}

int NetSession::allocateSeat(Match& match) const
{
	for (int i = 1; i < match.config.playerCount; ++i) {
		if (match.players[static_cast<size_t>(i)].control == SeatControl::Empty) {
			return i;
		}
	}
	return -1;
}

void NetSession::sendSnapshotTo(ClientSlot& c, const Match& match)
{
	if (!c.alive) {
		return;
	}
	const auto snap = serializeMatch(match);
	const auto msg = (match.phase == Phase::Lobby) ? net::makeLobbyState(snap) : net::makeMatchState(snap);
	c.sendQ.enqueue(net::FrameCodec::pack(msg));
}

void NetSession::broadcastSnapshot(const Match& match)
{
	for (auto& c : clients_) {
		if (c.alive) {
			sendSnapshotTo(c, match);
		}
	}
}

void NetSession::applyRemoteSnapshot(Match& match, const uint8_t* data, size_t size, bool /*isLobby*/)
{
	Match next;
	if (!deserializeMatch(next, data, size)) {
		// #92: broken/mismatched snapshot — ask the host for a fresh one.
		setStatus("Bad snapshot from host — requesting resync");
		clientSendQ_.enqueue(net::FrameCodec::pack(net::makeResyncRequest()));
		return;
	}

	// #91: ignore stale snapshots (older generation of the same match).
	if (next.matchId == lastSeenMatchId_ && next.syncGeneration < lastSeenSync_) {
		++staleSnapshotsIgnored_;
		return;
	}

	// Remap seat control labels for this client view.
	for (int i = 0; i < kMaxPlayers; ++i) {
		Player& p = next.players[static_cast<size_t>(i)];
		if (i == localSeat_) {
			p.control = SeatControl::HumanLocal;
		} else if (p.control == SeatControl::HumanLocal) {
			// Host's local becomes remote from our view.
			p.control = SeatControl::HumanRemote;
		}
	}

	const bool newMatch = (next.matchId != lastSeenMatchId_);
	const bool wasPlaying = (match.phase == Phase::Playing || match.phase == Phase::GameOver);
	const bool nowPlaying = (next.phase == Phase::Playing || next.phase == Phase::GameOver);
	bool cosmeticsChanged = false;
	if (match.phase == Phase::Lobby || next.phase == Phase::Lobby) {
		for (int i = 0; i < kMaxPlayers; ++i) {
			const Cosmetics& a = match.players[static_cast<size_t>(i)].cosmetics;
			const Cosmetics& b = next.players[static_cast<size_t>(i)].cosmetics;
			if (a.plastic != b.plastic || a.towerSkin != b.towerSkin || a.accessory != b.accessory) {
				cosmeticsChanged = true;
				break;
			}
		}
		if (match.config.mapId != next.config.mapId) {
			cosmeticsChanged = true; // rebuild room props
		}
	}
	if (newMatch || (!wasPlaying && nowPlaying) || cosmeticsChanged) {
		sceneNeedsRebuild_ = true;
	}

	// Preserve impulse if host just set one (client physics cue).
	match = std::move(next);
	lastSeenMatchId_ = match.matchId;
	lastSeenSync_ = match.syncGeneration;
}

void NetSession::setError(const char* msg)
{
	lastError_ = msg ? msg : "";
}

bool NetSession::canLocalAct(const Match& match) const
{
	if (match.phase != Phase::Playing) {
		return false;
	}
	if (mode_ == AppMode::Offline) {
		return true; // hotseat: any seat on this machine
	}
	return match.activePlayer == localSeat_ && localSeat_ >= 0;
}

bool NetSession::requestPlayCard(Match& match, int handIndex, int targetPlayer)
{
	clearError();
	if (mode_ == AppMode::Client) {
		if (!canLocalAct(match)) {
			setError("Not your turn.");
			return false;
		}
		if (!clientSock_.valid()) {
			setError("Disconnected from host.");
			return false;
		}
		const char* why = describeIllegalPlay(match, match.activePlayer, handIndex, targetPlayer);
		if (why && why[0]) {
			setError(why);
			return false;
		}
		clientSendQ_.enqueue(net::FrameCodec::pack(net::makePlayCard(nextIntentId_++, handIndex, targetPlayer)));
		return true;
	}

	if (match.phase != Phase::Playing) {
		setError(describeIllegalPlay(match, match.activePlayer, handIndex, targetPlayer));
		return false;
	}
	const Player& ap = match.players[static_cast<size_t>(match.activePlayer)];
	if (mode_ == AppMode::Host) {
		if (match.activePlayer != localSeat_ || ap.control != SeatControl::HumanLocal) {
			setError("Not your turn.");
			return false;
		}
	}
	const char* why = describeIllegalPlay(match, match.activePlayer, handIndex, targetPlayer);
	if (why && why[0]) {
		setError(why);
		return false;
	}
	if (!applyCardPlay(match, handIndex, targetPlayer)) {
		setError("Illegal play.");
		return false;
	}
	bumpSync(match);
	if (mode_ == AppMode::Host) {
		broadcastSnapshot(match);
	}
	return true;
}

bool NetSession::requestEndTurn(Match& match)
{
	if (mode_ == AppMode::Client) {
		if (!canLocalAct(match) || !clientSock_.valid()) {
			return false;
		}
		clientSendQ_.enqueue(net::FrameCodec::pack(net::makeEndTurn()));
		return true;
	}
	if (!canLocalAct(match) && mode_ != AppMode::Offline) {
		return false;
	}
	if (match.phase != Phase::Playing) {
		return false;
	}
	endTurn(match);
	bumpSync(match);
	if (mode_ == AppMode::Host) {
		broadcastSnapshot(match);
	}
	return true;
}

bool NetSession::requestReady(Match& match, bool ready)
{
	if (match.phase != Phase::Lobby) {
		return false;
	}
	if (mode_ == AppMode::Client) {
		clientSendQ_.enqueue(net::FrameCodec::pack(net::makeReady(ready)));
		return true;
	}
	if (localSeat_ >= 0) {
		match.players[static_cast<size_t>(localSeat_)].ready = ready;
		bumpSync(match);
		if (mode_ == AppMode::Host) {
			broadcastSnapshot(match);
		}
	}
	return true;
}

bool NetSession::requestSetTower(Match& match, TowerType t)
{
	if (match.phase != Phase::Lobby) {
		return false;
	}
	if (mode_ == AppMode::Client) {
		clientSendQ_.enqueue(net::FrameCodec::pack(net::makeSetTower(t)));
		return true;
	}
	if (localSeat_ >= 0) {
		match.players[static_cast<size_t>(localSeat_)].tower = t;
		bumpSync(match);
		if (mode_ == AppMode::Host) {
			broadcastSnapshot(match);
		}
	}
	return true;
}

bool NetSession::requestSetCosmetics(Match& match, const Cosmetics& cos)
{
	if (match.phase != Phase::Lobby && match.phase != Phase::Playing) {
		// Allow lobby primarily; mid-match cosmetic changes optional for jam fun in offline.
		return false;
	}
	if (mode_ == AppMode::Client) {
		if (match.phase != Phase::Lobby) {
			return false;
		}
		clientSendQ_.enqueue(net::FrameCodec::pack(net::makeSetCosmetics(cos)));
		return true;
	}
	// Offline hotseat: apply to active seat in lobby all human-local, or localSeat for host.
	int seat = localSeat_;
	if (mode_ == AppMode::Offline && match.phase == Phase::Lobby) {
		seat = 0;
	}
	if (mode_ == AppMode::Offline && match.phase == Phase::Playing) {
		seat = match.activePlayer;
	}
	if (seat < 0 || seat >= match.config.playerCount) {
		return false;
	}
	match.players[static_cast<size_t>(seat)].cosmetics = cos;
	bumpSync(match);
	sceneNeedsRebuild_ = true;
	if (mode_ == AppMode::Host) {
		broadcastSnapshot(match);
	}
	return true;
}

bool NetSession::requestSetDeckMods(Match& match, const int banned[2], const int extras[2])
{
	if (match.phase != Phase::Lobby) {
		return false;
	}
	if (mode_ == AppMode::Client) {
		clientSendQ_.enqueue(net::FrameCodec::pack(net::makeSetDeckMods(banned, extras)));
		return true;
	}
	if (localSeat_ >= 0) {
		Player& p = match.players[static_cast<size_t>(localSeat_)];
		p.bannedDefs[0] = banned[0];
		p.bannedDefs[1] = banned[1];
		p.extraDefs[0] = extras[0];
		p.extraDefs[1] = extras[1];
		bumpSync(match);
		if (mode_ == AppMode::Host) {
			broadcastSnapshot(match);
		}
	}
	return true;
}

bool NetSession::requestChat(Match& match, const char* text)
{
	if (!text || !text[0]) {
		return false;
	}
	if (mode_ == AppMode::Client) {
		// #81: client-side rate limit.
		const auto now = Clock::now();
		if (secondsBetween(clientLastChat_, now) < kChatMinIntervalSec) {
			setError("Chat rate limited — slow down.");
			return false;
		}
		clientLastChat_ = now;
		clientSendQ_.enqueue(net::FrameCodec::pack(net::makeChat(text)));
		return true;
	}
	// Host / offline: log directly.
	MatchEvent e;
	e.type = MatchEvent::Type::Info;
	e.actor = localSeat_;
	char buf[256];
	std::snprintf(buf, sizeof(buf), "%s: %s", localName_.c_str(), text);
	e.text = buf;
	match.log.push_back(std::move(e));
	bumpSync(match);
	if (mode_ == AppMode::Host) {
		broadcastSnapshot(match);
	}
	return true;
}

bool NetSession::requestRematchVote(Match& match, bool accept)
{
	if (match.phase != Phase::GameOver) {
		return false;
	}
	if (mode_ == AppMode::Client) {
		clientSendQ_.enqueue(net::FrameCodec::pack(net::makeRematchVote(accept)));
		return true;
	}
	if (mode_ == AppMode::Host && localSeat_ >= 0) {
		rematchVoted_[static_cast<size_t>(localSeat_)] = true;
		rematchAccept_[static_cast<size_t>(localSeat_)] = accept;
		maybeFinishRematchVote(match);
		return true;
	}
	return false;
}

void NetSession::rematchVoteCounts(const Match& match, int& votes, int& needed) const
{
	votes = 0;
	needed = 0;
	for (int i = 0; i < match.config.playerCount; ++i) {
		const SeatControl c = match.players[static_cast<size_t>(i)].control;
		if (c == SeatControl::HumanLocal || c == SeatControl::HumanRemote) {
			++needed;
			if (rematchVoted_[static_cast<size_t>(i)] && rematchAccept_[static_cast<size_t>(i)]) {
				++votes;
			}
		}
	}
}

void NetSession::maybeFinishRematchVote(Match& match)
{
	if (match.phase != Phase::GameOver) {
		return;
	}
	int votes = 0;
	int needed = 0;
	rematchVoteCounts(match, votes, needed);
	if (needed > 0 && votes >= needed) {
		hostRematch(match);
	}
}

bool NetSession::hostForceEndTurn(Match& match)
{
	if (mode_ == AppMode::Client || match.phase != Phase::Playing) {
		return false;
	}
	endTurn(match);
	bumpSync(match);
	if (mode_ == AppMode::Host) {
		broadcastSnapshot(match);
	}
	return true;
}

bool NetSession::hostKick(Match& match, int seat, const char* reason)
{
	if (mode_ != AppMode::Host || seat == localSeat_) {
		return false;
	}
	for (auto& c : clients_) {
		if (c.alive && c.seat == seat) {
			// Best-effort direct send — the socket closes right after (#79).
			const auto framed = net::FrameCodec::pack(net::makeKick(reason));
			c.sock.send(framed.data(), static_cast<int>(framed.size()));
			c.sock.close();
			c.alive = false;
			c.seat = -1;
			if (seat >= 0 && seat < kMaxPlayers) {
				match.players[static_cast<size_t>(seat)] = Player{};
				match.players[static_cast<size_t>(seat)].id = seat;
				match.players[static_cast<size_t>(seat)].control = SeatControl::Empty;
			}
			char buf[96];
			std::snprintf(buf, sizeof(buf), "Seat %d kicked.", seat);
			setStatus(buf);
			bumpSync(match);
			broadcastSnapshot(match);
			return true;
		}
	}
	return false;
}

bool NetSession::hostSetLobbyLocked(Match& match, bool locked)
{
	if (mode_ != AppMode::Host) {
		return false;
	}
	match.lobbyLocked = locked;
	bumpSync(match);
	broadcastSnapshot(match);
	setStatus(locked ? "Lobby closed to new players" : "Lobby open");
	return true;
}

int NetSession::seatPingMs(int seat) const
{
	for (const auto& c : clients_) {
		if (c.alive && c.seat == seat) {
			return c.pingMs;
		}
	}
	return -1;
}

float NetSession::seatPacketAge(int seat) const
{
	for (const auto& c : clients_) {
		if (c.alive && c.seat == seat) {
			return static_cast<float>(secondsBetween(c.lastRecv, Clock::now()));
		}
	}
	return -1.0f;
}

float NetSession::hostPacketAge() const
{
	if (mode_ != AppMode::Client || !clientSock_.valid()) {
		return -1.0f;
	}
	return static_cast<float>(secondsBetween(clientLastRecv_, Clock::now()));
}

bool NetSession::hostStartMatch(Match& match)
{
	if (mode_ != AppMode::Host || match.phase != Phase::Lobby) {
		return false;
	}
	// #80: every seated human must be ready.
	for (int i = 0; i < match.config.playerCount; ++i) {
		const Player& p = match.players[static_cast<size_t>(i)];
		if ((p.control == SeatControl::HumanLocal || p.control == SeatControl::HumanRemote) && !p.ready) {
			char buf[96];
			std::snprintf(buf, sizeof(buf), "Cannot start: %s is not ready.", p.name);
			setError(buf);
			setStatus(buf);
			return false;
		}
	}
	rematchVoted_ = {};
	rematchAccept_ = {};
	startMatchFromLobby(match);
	sceneNeedsRebuild_ = true;
	lastSeenMatchId_ = match.matchId;
	broadcastSnapshot(match);
	setStatus("Match in progress (host authority)");
	return true;
}

void NetSession::hostPushState(Match& match)
{
	if (mode_ != AppMode::Host) {
		return;
	}
	bumpSync(match);
	broadcastSnapshot(match);
}

bool NetSession::hostRematch(Match& match)
{
	if (mode_ != AppMode::Host) {
		return false;
	}
	// Keep seats + cosmetics + map; new seed (v0.6 P1 #23).
	const MapId map = match.config.mapId;
	const bool events = match.config.eventsEnabled;
	const bool fillAI = match.config.fillEmptyWithAI;
	Cosmetics kept[kMaxPlayers];
	SeatControl controls[kMaxPlayers];
	char names[kMaxPlayers][kPlayerNameLen];
	TowerType towers[kMaxPlayers];
	for (int i = 0; i < kMaxPlayers; ++i) {
		kept[i] = match.players[static_cast<size_t>(i)].cosmetics;
		controls[i] = match.players[static_cast<size_t>(i)].control;
		std::snprintf(names[i], kPlayerNameLen, "%s", match.players[static_cast<size_t>(i)].name);
		towers[i] = match.players[static_cast<size_t>(i)].tower;
	}

	match.config.seed = match.rng + 97u + match.matchId * 3u;
	match.config.mapId = map;
	match.config.eventsEnabled = events;
	match.config.fillEmptyWithAI = fillAI;
	match.phase = Phase::Lobby;
	for (int i = 0; i < match.config.playerCount; ++i) {
		Player& p = match.players[static_cast<size_t>(i)];
		p.control = controls[i];
		p.cosmetics = kept[i];
		p.setName(names[i]);
		p.tower = towers[i];
		p.ready = (p.control == SeatControl::HumanLocal || p.control == SeatControl::AI);
		p.eliminated = false;
	}
	rematchVoted_ = {};
	rematchAccept_ = {};
	startMatchFromLobby(match);
	// Restore cosmetics after startMatchFromLobby (AI re-rolls cosmetics)
	for (int i = 0; i < match.config.playerCount; ++i) {
		if (controls[i] != SeatControl::AI) {
			match.players[static_cast<size_t>(i)].cosmetics = kept[i];
		}
	}
	sceneNeedsRebuild_ = true;
	lastSeenMatchId_ = match.matchId;
	broadcastSnapshot(match);
	setStatus("Rematch started (same map/loadouts, new seed)");
	return true;
}

void NetSession::hostDropClient(Match& match, ClientSlot& c, const char* why)
{
	const int seat = c.seat;
	c.sock.close();
	c.alive = false;
	c.seat = -1;
	c.pingMs = -1;
	if (seat < 0) {
		return;
	}
	if (match.phase == Phase::Lobby) {
		// Lobby: free the seat entirely (existing M2 behavior, tested by #113).
		match.players[static_cast<size_t>(seat)] = Player{};
		match.players[static_cast<size_t>(seat)].id = seat;
		match.players[static_cast<size_t>(seat)].control = SeatControl::Empty;
		bumpSync(match);
		broadcastSnapshot(match);
	} else if (match.phase == Phase::Playing) {
		// #100-lite: AI takes over, seat reserved for the reconnect token for 60s.
		Player& p = match.players[static_cast<size_t>(seat)];
		reclaimToken_[static_cast<size_t>(seat)] = c.token;
		reclaimName_[static_cast<size_t>(seat)] = p.name;
		reclaimDeadline_[static_cast<size_t>(seat)] = Clock::now() + std::chrono::seconds(60);
		p.control = SeatControl::AI;
		MatchEvent e;
		e.type = MatchEvent::Type::Info;
		e.actor = seat;
		char buf[128];
		std::snprintf(buf, sizeof(buf), "%s lost connection (%s) — AI takes over. 60s to reconnect.", p.name,
					  why ? why : "dropped");
		e.text = buf;
		match.log.push_back(std::move(e));
		bumpSync(match);
		broadcastSnapshot(match);
	}
	char sbuf[96];
	std::snprintf(sbuf, sizeof(sbuf), "Client seat %d dropped (%s)", seat, why ? why : "?");
	setStatus(sbuf);
}

void NetSession::hostOnClientMessage(Match& match, ClientSlot& c, const std::vector<uint8_t>& frame)
{
	net::MsgHeader hdr{};
	const uint8_t* payload = nullptr;
	size_t payloadSize = 0;
	if (!net::parseHeader(frame.data(), frame.size(), hdr, payload, payloadSize)) {
		c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject("bad header")));
		return;
	}
	// #88: hard, readable version reject.
	if (hdr.version != kProtocolVersion) {
		char buf[120];
		std::snprintf(buf, sizeof(buf), "Protocol v%u required — please update the game (you sent v%u).",
					  static_cast<unsigned>(kProtocolVersion), static_cast<unsigned>(hdr.version));
		const auto framed = net::FrameCodec::pack(net::makeReject(buf));
		c.sock.send(framed.data(), static_cast<int>(framed.size()));
		c.sock.close();
		c.alive = false;
		c.seat = -1;
		return;
	}
	c.lastRecv = Clock::now();
	const auto type = static_cast<net::MsgType>(hdr.type);

	switch (type) {
	case net::MsgType::Hello: {
		char name[kPlayerNameLen] = {};
		uint16_t ver = 0;
		uint32_t token = 0;
		if (!net::readHello(payload, payloadSize, name, ver, token)) {
			c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject("bad hello")));
			return;
		}
		if (c.seat >= 0) {
			c.sendQ.enqueue(net::FrameCodec::pack(net::makeWelcome(c.seat, c.token)));
			return;
		}
		if (match.phase == Phase::Lobby) {
			// #79: closed lobby rejects fresh joins.
			if (match.lobbyLocked) {
				const auto framed = net::FrameCodec::pack(net::makeReject("lobby is closed"));
				c.sock.send(framed.data(), static_cast<int>(framed.size()));
				c.sock.close();
				c.alive = false;
				return;
			}
			const int seat = allocateSeat(match);
			if (seat < 0) {
				const auto framed = net::FrameCodec::pack(net::makeReject("lobby full"));
				c.sock.send(framed.data(), static_cast<int>(framed.size()));
				c.sock.close();
				c.alive = false;
				return;
			}
			c.seat = seat;
			c.name = name;
			// Fresh reconnect token for this client.
			uint32_t rng = nowMs() ^ (static_cast<uint32_t>(seat) << 24) ^ 0x9E3779B9u;
			c.token = xorshift32(rng);
			match.players[static_cast<size_t>(seat)].control = SeatControl::HumanRemote;
			match.players[static_cast<size_t>(seat)].setName(name);
			match.players[static_cast<size_t>(seat)].ready = false;
			bumpSync(match);
			c.sendQ.enqueue(net::FrameCodec::pack(net::makeWelcome(c.seat, c.token)));
			broadcastSnapshot(match);
			char buf[96];
			std::snprintf(buf, sizeof(buf), "%s joined seat %d", name, c.seat);
			setStatus(buf);
			return;
		}
		// Mid-match: only a token reclaim gets in (#99/#100).
		if (token != 0) {
			for (int seat = 0; seat < match.config.playerCount; ++seat) {
				if (reclaimToken_[static_cast<size_t>(seat)] == token &&
					Clock::now() < reclaimDeadline_[static_cast<size_t>(seat)]) {
					reclaimToken_[static_cast<size_t>(seat)] = 0;
					c.seat = seat;
					c.name = reclaimName_[static_cast<size_t>(seat)];
					c.token = token;
					Player& p = match.players[static_cast<size_t>(seat)];
					p.control = SeatControl::HumanRemote;
					p.setName(c.name.c_str());
					MatchEvent e;
					e.type = MatchEvent::Type::Info;
					e.actor = seat;
					e.text = std::string(p.name) + " reconnected!";
					match.log.push_back(std::move(e));
					bumpSync(match);
					c.sendQ.enqueue(net::FrameCodec::pack(net::makeWelcome(seat, token)));
					sendSnapshotTo(c, match);
					broadcastSnapshot(match);
					char buf[96];
					std::snprintf(buf, sizeof(buf), "%s reclaimed seat %d", p.name, seat);
					setStatus(buf);
					return;
				}
			}
		}
		{
			const auto framed = net::FrameCodec::pack(net::makeReject("match already started"));
			c.sock.send(framed.data(), static_cast<int>(framed.size()));
			c.sock.close();
			c.alive = false;
		}
		return;
	}
	case net::MsgType::Ready: {
		bool ready = false;
		if (c.seat < 0 || !net::readReady(payload, payloadSize, ready)) {
			return;
		}
		if (match.phase != Phase::Lobby) {
			return;
		}
		match.players[static_cast<size_t>(c.seat)].ready = ready;
		bumpSync(match);
		broadcastSnapshot(match);
		break;
	}
	case net::MsgType::SetTower: {
		TowerType t = TowerType::MachineGun;
		if (c.seat < 0 || !net::readSetTower(payload, payloadSize, t)) {
			return;
		}
		if (match.phase != Phase::Lobby) {
			return;
		}
		match.players[static_cast<size_t>(c.seat)].tower = t;
		bumpSync(match);
		broadcastSnapshot(match);
		break;
	}
	case net::MsgType::SetCosmetics: {
		Cosmetics cos{};
		if (c.seat < 0 || !net::readSetCosmetics(payload, payloadSize, cos)) {
			return;
		}
		if (match.phase != Phase::Lobby) {
			return;
		}
		match.players[static_cast<size_t>(c.seat)].cosmetics = cos;
		bumpSync(match);
		sceneNeedsRebuild_ = true;
		broadcastSnapshot(match);
		break;
	}
	case net::MsgType::SetDeckMods: {
		int banned[2] = { 0, 0 };
		int extras[2] = { 0, 0 };
		if (c.seat < 0 || !net::readSetDeckMods(payload, payloadSize, banned, extras)) {
			return;
		}
		if (match.phase != Phase::Lobby) {
			return;
		}
		Player& p = match.players[static_cast<size_t>(c.seat)];
		p.bannedDefs[0] = banned[0];
		p.bannedDefs[1] = banned[1];
		p.extraDefs[0] = extras[0];
		p.extraDefs[1] = extras[1];
		bumpSync(match);
		broadcastSnapshot(match);
		break;
	}
	case net::MsgType::PlayCard: {
		uint32_t intentId = 0;
		int hand = -1, target = -1;
		if (c.seat < 0 || !net::readPlayCard(payload, payloadSize, intentId, hand, target)) {
			return;
		}
		// #94: duplicate intent → ignore silently (already applied or already rejected).
		if (intentId != 0 && intentId == c.lastIntentId) {
			return;
		}
		// #106 lite: intent rate limit.
		const auto now = Clock::now();
		if (secondsBetween(c.lastIntent, now) < kIntentMinIntervalSec) {
			c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject("slow down")));
			return;
		}
		c.lastIntent = now;
		c.lastIntentId = intentId;
		if (match.phase != Phase::Playing || match.activePlayer != c.seat) {
			c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject("not your turn")));
			return;
		}
		// #103: readable rejection reasons, always.
		const char* why = describeIllegalPlay(match, match.activePlayer, hand, target);
		if (why && why[0]) {
			c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject(why)));
			return;
		}
		if (!applyCardPlay(match, hand, target)) {
			c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject("illegal play")));
			return;
		}
		bumpSync(match);
		broadcastSnapshot(match);
		break;
	}
	case net::MsgType::EndTurn: {
		if (c.seat < 0 || match.phase != Phase::Playing || match.activePlayer != c.seat) {
			c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject("not your turn")));
			return;
		}
		endTurn(match);
		bumpSync(match);
		broadcastSnapshot(match);
		break;
	}
	case net::MsgType::Chat: {
		std::string text;
		if (net::readString(payload, payloadSize, text)) {
			// #81: host-side rate limit.
			const auto now = Clock::now();
			if (secondsBetween(c.lastChat, now) < kChatMinIntervalSec) {
				return;
			}
			c.lastChat = now;
			if (text.size() > 200) {
				text.resize(200);
			}
			MatchEvent e;
			e.type = MatchEvent::Type::Info;
			e.actor = c.seat;
			e.text = std::string(c.name) + ": " + text;
			match.log.push_back(std::move(e));
			bumpSync(match);
			broadcastSnapshot(match);
		}
		break;
	}
	case net::MsgType::Pong: {
		uint32_t echoed = 0;
		if (net::readU32(payload, payloadSize, echoed)) {
			const uint32_t now = nowMs();
			c.pingMs = (now >= echoed) ? static_cast<int>(now - echoed) : 0; // #83
		}
		break;
	}
	case net::MsgType::ResyncRequest: {
		// #92: client wants a fresh full snapshot.
		sendSnapshotTo(c, match);
		break;
	}
	case net::MsgType::RematchVote: {
		bool accept = false;
		if (c.seat < 0 || !net::readRematchVote(payload, payloadSize, accept)) {
			return;
		}
		rematchVoted_[static_cast<size_t>(c.seat)] = true;
		rematchAccept_[static_cast<size_t>(c.seat)] = accept;
		maybeFinishRematchVote(match); // #109
		break;
	}
	default:
		break;
	}
}

void NetSession::clientOnMessage(Match& match, const std::vector<uint8_t>& frame)
{
	net::MsgHeader hdr{};
	const uint8_t* payload = nullptr;
	size_t payloadSize = 0;
	if (!net::parseHeader(frame.data(), frame.size(), hdr, payload, payloadSize)) {
		return;
	}
	// #88: mismatched host — bail with a readable reason.
	if (hdr.version != kProtocolVersion) {
		char buf[120];
		std::snprintf(buf, sizeof(buf), "Host runs protocol v%u, you have v%u — update to matching versions.",
					  static_cast<unsigned>(hdr.version), static_cast<unsigned>(kProtocolVersion));
		clientSock_.close();
		setConnectionLost(buf);
		return;
	}
	clientLastRecv_ = Clock::now();
	const auto type = static_cast<net::MsgType>(hdr.type);

	switch (type) {
	case net::MsgType::Welcome: {
		int seat = -1;
		uint32_t token = 0;
		if (net::readWelcome(payload, payloadSize, seat, token)) {
			localSeat_ = seat;
			clientToken_ = token;
			clientWelcomed_ = true;
			char buf[64];
			std::snprintf(buf, sizeof(buf), "Joined as seat %d", seat);
			setStatus(buf);
		}
		break;
	}
	case net::MsgType::LobbyState:
	case net::MsgType::MatchState: {
		const uint8_t* snap = nullptr;
		size_t snapSize = 0;
		if (net::snapshotPayload(payload, payloadSize, snap, snapSize)) {
			applyRemoteSnapshot(match, snap, snapSize, type == net::MsgType::LobbyState);
			if (match.phase == Phase::Playing) {
				setStatus("Match in progress (client)");
			} else if (match.phase == Phase::Lobby) {
				setStatus("In lobby (client)");
			} else if (match.phase == Phase::GameOver) {
				setStatus("Game over");
			}
		}
		break;
	}
	case net::MsgType::Heartbeat: {
		uint32_t millis = 0;
		if (net::readU32(payload, payloadSize, millis)) {
			clientSendQ_.enqueue(net::FrameCodec::pack(net::makePong(millis))); // #83/#93
		}
		break;
	}
	case net::MsgType::Kick: {
		std::string reason;
		net::readString(payload, payloadSize, reason);
		clientSock_.close();
		std::string msg = "Kicked by host";
		if (!reason.empty()) {
			msg += ": " + reason;
		}
		setConnectionLost(msg.c_str()); // #79/#98
		break;
	}
	case net::MsgType::Reject: {
		std::string reason;
		if (net::readString(payload, payloadSize, reason)) {
			const std::string msg = "Rejected: " + reason;
			setStatus(msg.c_str());
			setError(msg.c_str()); // UI toasts soft-fail on client
		}
		break;
	}
	default:
		break;
	}
}

void NetSession::hostAccept(Match& match)
{
	if (!listener_.valid()) {
		return;
	}
	for (;;) {
		net::TcpSocket s = listener_.accept();
		if (!s.valid()) {
			break;
		}
		// Find free client slot.
		ClientSlot* slot = nullptr;
		for (auto& c : clients_) {
			if (!c.alive) {
				slot = &c;
				break;
			}
		}
		if (!slot) {
			s.close();
			continue;
		}
		// v0.8: mid-match connections are accepted — Hello decides (reclaim vs reject).
		(void)match;
		slot->sock = std::move(s);
		slot->codec.reset();
		slot->sendQ.clear();
		slot->alive = true;
		slot->seat = -1;
		slot->name.clear();
		slot->token = 0;
		slot->lastIntentId = 0;
		slot->pingMs = -1;
		slot->lastRecv = Clock::now();
	}
}

void NetSession::hostTickBeaconAndHeartbeat(Match& match)
{
	const auto now = Clock::now();

	// #85: LAN beacon (broadcast + loopback for same-machine testing).
	if (beaconSock_.valid() && secondsBetween(lastBeacon_, now) >= kBeaconSec) {
		lastBeacon_ = now;
		net::BeaconInfo b;
		std::snprintf(b.code, sizeof(b.code), "%s", roomCode_);
		b.version = kProtocolVersion;
		b.tcpPort = listenPort_;
		std::snprintf(b.hostName, sizeof(b.hostName), "%s", localName_.c_str());
		uint8_t taken = 0;
		for (int i = 0; i < match.config.playerCount; ++i) {
			if (match.players[static_cast<size_t>(i)].control != SeatControl::Empty) {
				++taken;
			}
		}
		b.seatsTaken = taken;
		b.seatsTotal = static_cast<uint8_t>(match.config.playerCount);
		const auto dgram = net::packBeacon(b);
		beaconSock_.sendTo("255.255.255.255", kBeaconPort, dgram.data(), static_cast<int>(dgram.size()));
		beaconSock_.sendTo("127.0.0.1", kBeaconPort, dgram.data(), static_cast<int>(dgram.size()));
	}

	// #93: heartbeat to every client.
	if (secondsBetween(lastHeartbeat_, now) >= kHeartbeatSec) {
		lastHeartbeat_ = now;
		const auto hb = net::FrameCodec::pack(net::makeHeartbeat(nowMs()));
		for (auto& c : clients_) {
			if (c.alive) {
				c.sendQ.enqueue(hb);
			}
		}
	}

	// #93: drop silent clients.
	for (auto& c : clients_) {
		if (c.alive && secondsBetween(c.lastRecv, now) > kPeerTimeoutSec) {
			hostDropClient(match, c, "timeout");
		}
	}

	// #100: expire stale reclaim reservations.
	for (int i = 0; i < kMaxPlayers; ++i) {
		if (reclaimToken_[static_cast<size_t>(i)] != 0 && now > reclaimDeadline_[static_cast<size_t>(i)]) {
			reclaimToken_[static_cast<size_t>(i)] = 0;
		}
	}
}

void NetSession::hostPumpClients(Match& match)
{
	uint8_t tmp[4096];
	for (auto& c : clients_) {
		if (!c.alive) {
			continue;
		}
		if (!c.sendQ.flush(c.sock)) {
			hostDropClient(match, c, "send error");
			continue;
		}
		for (;;) {
			const int n = c.sock.recv(tmp, sizeof(tmp));
			if (n < 0) {
				hostDropClient(match, c, "recv error");
				break;
			}
			if (n == 0) {
				break;
			}
			std::vector<std::vector<uint8_t>> frames;
			if (!c.codec.feed(tmp, static_cast<size_t>(n), frames)) {
				hostDropClient(match, c, "bad frame");
				break;
			}
			for (const auto& f : frames) {
				hostOnClientMessage(match, c, f);
			}
		}
	}
}

void NetSession::clientPump(Match& match)
{
	if (!clientSock_.valid()) {
		if (mode_ == AppMode::Client && !connectionLost_) {
			setStatus("Disconnected from host");
		}
		return;
	}
	if (!clientSendQ_.flush(clientSock_)) {
		clientSock_.close();
		setConnectionLost("Host lost — send failed.");
		return;
	}
	uint8_t tmp[8192];
	for (;;) {
		const int n = clientSock_.recv(tmp, sizeof(tmp));
		if (n < 0) {
			clientSock_.close();
			if (!connectionLost_) {
				setConnectionLost("Host lost — connection closed.");
			}
			return;
		}
		if (n == 0) {
			break;
		}
		std::vector<std::vector<uint8_t>> frames;
		if (!clientCodec_.feed(tmp, static_cast<size_t>(n), frames)) {
			clientSock_.close();
			setConnectionLost("Protocol error — malformed data from host.");
			return;
		}
		for (const auto& f : frames) {
			clientOnMessage(match, f);
		}
	}
	// #93: 8s of silence from the host → give up with a reason (#98).
	if (clientWelcomed_ && secondsBetween(clientLastRecv_, Clock::now()) > kPeerTimeoutSec) {
		clientSock_.close();
		setConnectionLost("Host lost — no data for 8 seconds.");
	}
}

void NetSession::update(Match& match)
{
	if (mode_ == AppMode::Host) {
		hostAccept(match);
		hostPumpClients(match);
		hostTickBeaconAndHeartbeat(match);
		// AI turns (host authority).
		if (match.phase == Phase::Playing) {
			// Keep auto-playing AI until a human seat or game over (cap per frame).
			for (int i = 0; i < 4; ++i) {
				if (!autoPlayIfAITurn(match)) {
					break;
				}
				broadcastSnapshot(match);
			}
		}
	} else if (mode_ == AppMode::Client) {
		clientPump(match);
	} else if (mode_ == AppMode::Offline) {
		// Optional: auto AI seats in offline if any marked AI.
		if (match.phase == Phase::Playing) {
			for (int i = 0; i < 4; ++i) {
				if (!autoPlayIfAITurn(match)) {
					break;
				}
			}
		}
	}
}

} // namespace toy
