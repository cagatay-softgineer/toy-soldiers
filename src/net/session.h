#pragma once

#include "game/match.h"
#include "game/replay.h"
#include "net/protocol.h"
#include "net/socket.h"
#include "net/transport.h"

#include <array>
#include <chrono>
#include <string>
#include <vector>

namespace toy {

enum class AppMode : uint8_t {
	Offline, // hotseat
	Host,
	Client,
};

// v0.8 #85/#86: LAN lobby browser — listens for host UDP beacons.
class LanDiscovery {
public:
	struct Entry {
		net::BeaconInfo info;
		double lastSeen = 0.0; // seconds (steady clock)
	};

	bool start();
	void stop();
	bool active() const { return sock_.valid(); }
	// Drain pending datagrams; entries older than 5s expire.
	void update();
	const std::vector<Entry>& entries() const { return entries_; }
	// nullptr if no live beacon carries this room code (case-insensitive).
	const net::BeaconInfo* findByCode(const char* code) const;

private:
	net::UdpSocket sock_;
	std::vector<Entry> entries_;
};

// Host-authoritative lobby + match session (concept board M2, hardened in v0.8).
class NetSession {
public:
	NetSession();
	~NetSession();

	AppMode mode() const { return mode_; }
	int localSeat() const { return localSeat_; }
	bool isConnected() const;
	const char* status() const { return status_.c_str(); }
	uint16_t listenPort() const { return listenPort_; }
	bool sceneNeedsRebuild() const { return sceneNeedsRebuild_; }
	void clearSceneFlag() { sceneNeedsRebuild_ = false; }

	// Offline hotseat start.
	void startOffline(Match& match, const MatchConfig& cfg);

	// Host: open TCP listen, claim seat 0, start UDP beacon (#85).
	bool hostLobby(Match& match, const MatchConfig& cfg, const char* hostName, uint16_t port = kDefaultPort);

	// Client: connect to host. spectate = seatless read-only client (v1.2 #111).
	bool joinLobby(Match& match, const char* host, uint16_t port, const char* playerName, bool spectate = false);
	bool isSpectator() const { return mode_ == AppMode::Client && localSeat_ == kSpectatorSeat; }

	// v1.2 #89: pick how joinLobby establishes its connection. DirectTcp (default)
	// dials the peer's IP:port directly; see net/transport.h for what Relay needs.
	void setTransport(const net::TransportConfig& cfg) { transportConfig_ = cfg; }
	const net::TransportConfig& transport() const { return transportConfig_; }

	void shutdown(Match* match = nullptr);

	// Poll sockets + AI turns + heartbeats/timeouts. Call every frame.
	void update(Match& match);

	// Local player actions. Returns false if rejected / not your turn / not connected.
	bool requestPlayCard(Match& match, int handIndex, int targetPlayer);
	bool requestEndTurn(Match& match);
	bool requestReady(Match& match, bool ready);
	bool requestSetTower(Match& match, TowerType t);
	bool requestSetCosmetics(Match& match, const Cosmetics& cos);
	// v0.7 #47: deck builder lite (lobby only).
	bool requestSetDeckMods(Match& match, const int banned[2], const int extras[2]);
	// v0.8 #81: lobby chat (rate-limited both sides).
	bool requestChat(Match& match, const char* text);
	// v0.8 #109: rematch vote (host and client paths).
	bool requestRematchVote(Match& match, bool accept);

	// Host only.
	bool hostStartMatch(Match& match);
	bool hostRematch(Match& match);
	// v0.7 #56: authority-side auto-skip when the turn timer expires (any seat).
	bool hostForceEndTurn(Match& match);
	// Push current match/lobby snapshot to all clients (map pick, etc.).
	void hostPushState(Match& match);
	// v0.8 #79: kick a remote seat / open-close the lobby.
	bool hostKick(Match& match, int seat, const char* reason = "kicked by host");
	bool hostSetLobbyLocked(Match& match, bool locked);
	// v1.2 #84: host-picked lobby banner color.
	bool hostSetBannerColor(Match& match, uint8_t color);

	// --- v1.2 #101/#102: host migration ---
	struct MigrationPlan {
		enum class Role : uint8_t { None, BecomeHost, JoinNewHost };
		Role role = Role::None;
		int newHostSeat = -1;
		char ip[40] = {};
		uint16_t port = 0;
	};
	// Pure decision from the last known match state: the lowest surviving human seat
	// (old host seat 0 excluded) becomes the new host; everyone else rejoins its peer IP.
	MigrationPlan planMigration(const Match& lastState, uint16_t knownHostPort) const;
	// Become the new authority over the CURRENT match state (lobby or mid-match).
	// Opens listener+beacon, converts the vanished host seat to AI, and holds a 30s
	// window where survivors reattach to their old seats by display name.
	bool hostAdoptMatch(Match& match, uint16_t port, const char* myName);

	// v1.2 #107: host-authority action recording — every applyCardPlay/endTurn actually
	// resolved on this side (local human, AI, or an accepted remote client intent) is
	// logged. No-op for Client mode (its requests are not yet authoritative). Pass
	// nullptr to stop recording; caller owns the ReplayRecorder's lifetime.
	void setReplayRecorder(ReplayRecorder* recorder) { replayRecorder_ = recorder; }

	// Offline "Auto Play"/Space hotkey path — routed here (instead of the free function)
	// so the active replay recorder, if any, sees these plays too (#107).
	bool autoPlay(Match& match)
	{
		const bool ok = autoPlayBest(match, replayRecorder_);
		if (ok) {
			bumpSync(match);
		}
		return ok;
	}

	bool canLocalAct(const Match& match) const;
	bool isLocalSeat(int seat) const { return seat == localSeat_; }

	// Last human-readable failure from request* (v0.6).
	const char* lastError() const { return lastError_.c_str(); }
	void clearError() { lastError_.clear(); }

	// --- v0.8 observability & resilience ---
	const char* roomCode() const { return roomCode_; }
	// Host view: last measured RTT for a remote seat (-1 unknown / not remote).
	int seatPingMs(int seat) const;
	// Host view: seconds since we heard from a remote seat (-1 not remote).
	float seatPacketAge(int seat) const;
	// Client view: seconds since the host last sent anything (-1 when not client).
	float hostPacketAge() const;
	// #98: set when the connection died (timeout, kick, protocol mismatch).
	bool connectionLost() const { return connectionLost_; }
	const char* connectionLostReason() const { return connectionLostReason_.c_str(); }
	void clearConnectionLost();
	// #109 UI: votes collected / votes required (humans only).
	void rematchVoteCounts(const Match& match, int& votes, int& needed) const;

private:
	using Clock = std::chrono::steady_clock;

	struct ClientSlot {
		net::TcpSocket sock;
		net::FrameCodec codec;
		net::SendQueue sendQ;
		int seat = -1;
		bool alive = false;
		std::string name;
		// v0.8
		uint32_t token = 0;
		uint32_t lastIntentId = 0;
		int pingMs = -1;
		Clock::time_point lastRecv{};
		Clock::time_point lastChat{};
		Clock::time_point lastIntent{};
		char ip[64] = {}; // v1.2 #101: peer address from accept()
	};

	void setError(const char* msg);
	void setStatus(const char* s);
	void setConnectionLost(const char* reason);
	void broadcastSnapshot(const Match& match);
	void sendSnapshotTo(ClientSlot& c, const Match& match);
	void hostOnClientMessage(Match& match, ClientSlot& c, const std::vector<uint8_t>& frame);
	void clientOnMessage(Match& match, const std::vector<uint8_t>& frame);
	void hostAccept(Match& match);
	void hostPumpClients(Match& match);
	void hostDropClient(Match& match, ClientSlot& c, const char* why);
	void hostTickBeaconAndHeartbeat(Match& match);
	void clientPump(Match& match);
	void applyRemoteSnapshot(Match& match, const uint8_t* data, size_t size, bool isLobby);
	void applyRemoteDelta(Match& match, const uint8_t* data, size_t size); // v1.2 #96
	void adoptRemoteState(Match& match, Match&& next);
	int allocateSeat(Match& match) const;
	void maybeFinishRematchVote(Match& match);
	uint32_t nowMs() const;

	AppMode mode_ = AppMode::Offline;
	int localSeat_ = 0;
	uint16_t listenPort_ = 0;
	std::string status_ = "Offline hotseat";
	std::string localName_ = "Host";
	bool sceneNeedsRebuild_ = false;
	uint32_t lastSeenMatchId_ = 0;
	uint32_t lastSeenSync_ = 0;

	net::TcpListener listener_;
	std::array<ClientSlot, kMaxPlayers - 1> clients_{}; // host uses seat 0; up to 3 remote

	net::TcpSocket clientSock_;
	net::FrameCodec clientCodec_;
	net::SendQueue clientSendQ_;
	bool clientWelcomed_ = false;
	std::string lastError_;
	net::TransportConfig transportConfig_; // v1.2 #89: DirectTcp until a relay backend exists

	// --- v0.8 state ---
	Clock::time_point startTime_{};
	// Host: LAN beacon + heartbeat cadence.
	net::UdpSocket beaconSock_;
	char roomCode_[kRoomCodeLen] = {};
	Clock::time_point lastBeacon_{};
	Clock::time_point lastHeartbeat_{};
	// Host: mid-match reconnect slots (#99/#100): seat → token/name until deadline.
	std::array<uint32_t, kMaxPlayers> reclaimToken_{};
	std::array<std::string, kMaxPlayers> reclaimName_{};
	std::array<Clock::time_point, kMaxPlayers> reclaimDeadline_{};
	// Host: rematch votes (#109).
	std::array<bool, kMaxPlayers> rematchVoted_{};
	std::array<bool, kMaxPlayers> rematchAccept_{};
	// Client:
	uint32_t clientToken_ = 0;   // reconnect token from Welcome
	uint32_t nextIntentId_ = 1;  // #94
	Clock::time_point clientLastRecv_{};
	bool spectateRequested_ = false; // v1.2 #111
	// v1.2 #96: last broadcast state for delta diffing.
	Match lastBroadcast_{};
	bool lastBroadcastValid_ = false;
	// v1.2 #101/#102: post-migration name-reclaim window.
	Clock::time_point migrationDeadline_{};
	bool migrationWindow_ = false;
	// v1.2 #107: not owned — caller keeps the recorder alive for the match's duration.
	ReplayRecorder* replayRecorder_ = nullptr;
	Clock::time_point clientLastChat_{};
	bool connectionLost_ = false;
	std::string connectionLostReason_;
	uint32_t staleSnapshotsIgnored_ = 0; // #91 (exposed via status/log only)
};

} // namespace toy
