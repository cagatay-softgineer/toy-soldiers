#include "net/session.h"

#include "game/play_reason.h"
#include "game/rules.h"
#include "game/snapshot.h"
#include "net/protocol.h"

#include <cstdio>

namespace toy {

NetSession::NetSession()
{
	net::netInit();
}

NetSession::~NetSession()
{
	shutdown(nullptr);
	net::netShutdown();
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

void NetSession::startOffline(Match& match, const MatchConfig& cfg)
{
	shutdown(&match);
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

	char buf[96];
	std::snprintf(buf, sizeof(buf), "Hosting lobby on port %u — waiting for clients", static_cast<unsigned>(port));
	setStatus(buf);
	return true;
}

bool NetSession::joinLobby(Match& match, const char* host, uint16_t port, const char* playerName)
{
	shutdown(&match);
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
	clientSendQ_.enqueue(net::FrameCodec::pack(net::makeHello(localName_.c_str())));
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
	}
	listener_.close();
	clientSock_.close();
	clientCodec_.reset();
	clientSendQ_.clear();
	clientWelcomed_ = false;
	listenPort_ = 0;
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
		setStatus("Bad snapshot from host");
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
		clientSendQ_.enqueue(net::FrameCodec::pack(net::makePlayCard(handIndex, targetPlayer)));
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

bool NetSession::hostStartMatch(Match& match)
{
	if (mode_ != AppMode::Host || match.phase != Phase::Lobby) {
		return false;
	}
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

void NetSession::hostOnClientMessage(Match& match, ClientSlot& c, const std::vector<uint8_t>& frame)
{
	net::MsgHeader hdr{};
	const uint8_t* payload = nullptr;
	size_t payloadSize = 0;
	if (!net::parseHeader(frame.data(), frame.size(), hdr, payload, payloadSize)) {
		c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject("bad header")));
		return;
	}
	const auto type = static_cast<net::MsgType>(hdr.type);

	switch (type) {
	case net::MsgType::Hello: {
		char name[kPlayerNameLen] = {};
		uint16_t ver = 0;
		if (!net::readHello(payload, payloadSize, name, ver)) {
			c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject("bad hello")));
			return;
		}
		if (c.seat < 0) {
			const int seat = allocateSeat(match);
			if (seat < 0) {
				c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject("lobby full")));
				c.alive = false;
				c.sock.close();
				return;
			}
			c.seat = seat;
			c.name = name;
			match.players[static_cast<size_t>(seat)].control = SeatControl::HumanRemote;
			match.players[static_cast<size_t>(seat)].setName(name);
			match.players[static_cast<size_t>(seat)].ready = false;
			bumpSync(match);
		}
		c.sendQ.enqueue(net::FrameCodec::pack(net::makeWelcome(c.seat, listenPort_)));
		broadcastSnapshot(match);
		char buf[96];
		std::snprintf(buf, sizeof(buf), "%s joined seat %d", name, c.seat);
		setStatus(buf);
		break;
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
	case net::MsgType::PlayCard: {
		int hand = -1, target = -1;
		if (c.seat < 0 || !net::readPlayCard(payload, payloadSize, hand, target)) {
			return;
		}
		if (match.phase != Phase::Playing || match.activePlayer != c.seat) {
			c.sendQ.enqueue(net::FrameCodec::pack(net::makeReject("not your turn")));
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
	const auto type = static_cast<net::MsgType>(hdr.type);

	switch (type) {
	case net::MsgType::Welcome: {
		int seat = -1;
		if (net::readWelcome(payload, payloadSize, seat)) {
			localSeat_ = seat;
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
		if (match.phase != Phase::Lobby) {
			// Mid-match joins not supported in M2.
			auto q = net::FrameCodec::pack(net::makeReject("match already started"));
			// best-effort blocking-ish send
			s.send(q.data(), static_cast<int>(q.size()));
			s.close();
			continue;
		}
		slot->sock = std::move(s);
		slot->codec.reset();
		slot->sendQ.clear();
		slot->alive = true;
		slot->seat = -1;
		slot->name.clear();
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
			// Drop client.
			if (c.seat >= 0 && match.phase == Phase::Lobby) {
				match.players[static_cast<size_t>(c.seat)] = Player{};
				match.players[static_cast<size_t>(c.seat)].id = c.seat;
				match.players[static_cast<size_t>(c.seat)].control = SeatControl::Empty;
				bumpSync(match);
				broadcastSnapshot(match);
			}
			c.sock.close();
			c.alive = false;
			c.seat = -1;
			continue;
		}
		for (;;) {
			const int n = c.sock.recv(tmp, sizeof(tmp));
			if (n < 0) {
				if (c.seat >= 0 && match.phase == Phase::Lobby) {
					match.players[static_cast<size_t>(c.seat)] = Player{};
					match.players[static_cast<size_t>(c.seat)].id = c.seat;
					match.players[static_cast<size_t>(c.seat)].control = SeatControl::Empty;
					bumpSync(match);
					broadcastSnapshot(match);
				}
				c.sock.close();
				c.alive = false;
				c.seat = -1;
				break;
			}
			if (n == 0) {
				break;
			}
			std::vector<std::vector<uint8_t>> frames;
			if (!c.codec.feed(tmp, static_cast<size_t>(n), frames)) {
				c.sock.close();
				c.alive = false;
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
		if (mode_ == AppMode::Client) {
			setStatus("Disconnected from host");
		}
		return;
	}
	if (!clientSendQ_.flush(clientSock_)) {
		clientSock_.close();
		setStatus("Disconnected from host");
		return;
	}
	uint8_t tmp[8192];
	for (;;) {
		const int n = clientSock_.recv(tmp, sizeof(tmp));
		if (n < 0) {
			clientSock_.close();
			setStatus("Disconnected from host");
			return;
		}
		if (n == 0) {
			break;
		}
		std::vector<std::vector<uint8_t>> frames;
		if (!clientCodec_.feed(tmp, static_cast<size_t>(n), frames)) {
			clientSock_.close();
			setStatus("Protocol error");
			return;
		}
		for (const auto& f : frames) {
			clientOnMessage(match, f);
		}
	}
}

void NetSession::update(Match& match)
{
	if (mode_ == AppMode::Host) {
		hostAccept(match);
		hostPumpClients(match);
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
