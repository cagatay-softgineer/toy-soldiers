#pragma once

#include "game/match.h"
#include "net/socket.h"

#include <array>
#include <string>

namespace toy {

enum class AppMode : uint8_t {
	Offline, // hotseat
	Host,
	Client,
};

// Host-authoritative lobby + match session (concept board M2).
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

	// Host: open TCP listen, claim seat 0.
	bool hostLobby(Match& match, const MatchConfig& cfg, const char* hostName, uint16_t port = kDefaultPort);

	// Client: connect to host.
	bool joinLobby(Match& match, const char* host, uint16_t port, const char* playerName);

	void shutdown(Match* match = nullptr);

	// Poll sockets + AI turns. Call every frame.
	void update(Match& match);

	// Local player actions. Returns false if rejected / not your turn / not connected.
	bool requestPlayCard(Match& match, int handIndex, int targetPlayer);
	bool requestEndTurn(Match& match);
	bool requestReady(Match& match, bool ready);
	bool requestSetTower(Match& match, TowerType t);
	bool requestSetCosmetics(Match& match, const Cosmetics& cos);
	// v0.7 #47: deck builder lite (lobby only).
	bool requestSetDeckMods(Match& match, const int banned[2], const int extras[2]);

	// Host only.
	bool hostStartMatch(Match& match);
	bool hostRematch(Match& match);
	// v0.7 #56: authority-side auto-skip when the turn timer expires (any seat).
	bool hostForceEndTurn(Match& match);
	// Push current match/lobby snapshot to all clients (map pick, etc.).
	void hostPushState(Match& match);

	bool canLocalAct(const Match& match) const;
	bool isLocalSeat(int seat) const { return seat == localSeat_; }

	// Last human-readable failure from request* (v0.6).
	const char* lastError() const { return lastError_.c_str(); }
	void clearError() { lastError_.clear(); }

private:
	void setError(const char* msg);
	struct ClientSlot {
		net::TcpSocket sock;
		net::FrameCodec codec;
		net::SendQueue sendQ;
		int seat = -1;
		bool alive = false;
		std::string name;
	};

	void setStatus(const char* s);
	void broadcastSnapshot(const Match& match);
	void sendSnapshotTo(ClientSlot& c, const Match& match);
	void hostOnClientMessage(Match& match, ClientSlot& c, const std::vector<uint8_t>& frame);
	void clientOnMessage(Match& match, const std::vector<uint8_t>& frame);
	void hostAccept(Match& match);
	void hostPumpClients(Match& match);
	void clientPump(Match& match);
	void applyRemoteSnapshot(Match& match, const uint8_t* data, size_t size, bool isLobby);
	int allocateSeat(Match& match) const;
	void hostAssignAIFills(Match& match);

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
};

} // namespace toy
