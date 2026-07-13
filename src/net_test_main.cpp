#include "game/cards.h"
#include "game/match.h"
#include "game/rules.h"
#include "game/snapshot.h"
#include "net/protocol.h"
#include "net/session.h"
#include "net/socket.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {

int g_fails = 0;

#define CHECK(cond, msg)                                                                                               \
	do {                                                                                                               \
		if (!(cond)) {                                                                                                 \
			std::printf("FAIL: %s\n", msg);                                                                            \
			++g_fails;                                                                                                 \
		}                                                                                                              \
	} while (0)

void pump(toy::NetSession& host, toy::Match& hostMatch, toy::NetSession& client, toy::Match& clientMatch, int times = 1)
{
	for (int i = 0; i < times; ++i) {
		host.update(hostMatch);
		client.update(clientMatch);
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
	}
}

void pumpHost(toy::NetSession& host, toy::Match& hostMatch, int times = 1)
{
	for (int i = 0; i < times; ++i) {
		host.update(hostMatch);
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
	}
}

bool waitForJoin(toy::NetSession& host, toy::Match& hostMatch, toy::NetSession& client, toy::Match& clientMatch)
{
	for (int i = 0; i < 300; ++i) {
		pump(host, hostMatch, client, clientMatch, 1);
		if (client.localSeat() >= 1) {
			const int s = client.localSeat();
			if (hostMatch.players[static_cast<size_t>(s)].control == toy::SeatControl::HumanRemote) {
				return true;
			}
		}
	}
	return false;
}

bool playActiveSeat(toy::NetSession& host, toy::Match& hostMatch, toy::NetSession& client, toy::Match& clientMatch)
{
	using namespace toy;
	if (hostMatch.phase != Phase::Playing) {
		return false;
	}

	const int ap = hostMatch.activePlayer;
	const SeatControl ctrl = hostMatch.players[static_cast<size_t>(ap)].control;

	if (ctrl == SeatControl::AI) {
		// Host.update will auto-play AI.
		pump(host, hostMatch, client, clientMatch, 4);
		return true;
	}

	if (ctrl == SeatControl::HumanRemote && ap == client.localSeat()) {
		// Choose from client view of own hand (should match host).
		Player& p = clientMatch.players[static_cast<size_t>(ap)];
		bool sent = false;
		for (int h = 0; h < static_cast<int>(p.hand.size()) && !sent; ++h) {
			for (int t = 0; t < hostMatch.config.playerCount; ++t) {
				if (canPlayCard(clientMatch, ap, h, t)) {
					client.requestPlayCard(clientMatch, h, t);
					sent = true;
					break;
				}
			}
		}
		if (!sent) {
			client.requestEndTurn(clientMatch);
		}
		pump(host, hostMatch, client, clientMatch, 8);
		return true;
	}

	if (ctrl == SeatControl::HumanLocal && ap == host.localSeat()) {
		Player& p = hostMatch.players[static_cast<size_t>(ap)];
		bool played = false;
		for (int h = 0; h < static_cast<int>(p.hand.size()) && !played; ++h) {
			for (int t = 0; t < hostMatch.config.playerCount; ++t) {
				if (canPlayCard(hostMatch, ap, h, t)) {
					played = host.requestPlayCard(hostMatch, h, t);
					if (played) {
						break;
					}
				}
			}
		}
		if (!played) {
			host.requestEndTurn(hostMatch);
		}
		pump(host, hostMatch, client, clientMatch, 8);
		return true;
	}

	// Unexpected control — advance with end turn if host seat somehow.
	pump(host, hostMatch, client, clientMatch, 4);
	return true;
}

// Read raw frames off a socket for a short while and look for a Reject containing `needle`.
bool rawExpectReject(toy::net::TcpSocket& sock, const char* needle)
{
	toy::net::FrameCodec codec;
	uint8_t tmp[2048];
	for (int i = 0; i < 200; ++i) {
		const int n = sock.recv(tmp, sizeof(tmp));
		if (n < 0) {
			return false;
		}
		if (n > 0) {
			std::vector<std::vector<uint8_t>> frames;
			if (!codec.feed(tmp, static_cast<size_t>(n), frames)) {
				return false;
			}
			for (const auto& f : frames) {
				toy::net::MsgHeader hdr{};
				const uint8_t* payload = nullptr;
				size_t psz = 0;
				if (!toy::net::parseHeader(f.data(), f.size(), hdr, payload, psz)) {
					continue;
				}
				if (static_cast<toy::net::MsgType>(hdr.type) == toy::net::MsgType::Reject) {
					std::string reason;
					toy::net::readString(payload, psz, reason);
					return reason.find(needle) != std::string::npos;
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
	}
	return false;
}

} // namespace

int main()
{
	using namespace toy;

	// --- Snapshot round-trip ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 99;
		startMatch(m, cfg);
		autoPlayBest(m);
		const auto bytes = serializeMatch(m);
		Match m2;
		CHECK(deserializeMatch(m2, bytes.data(), bytes.size()), "deserializeMatch");
		CHECK(m2.turnNumber == m.turnNumber && m2.players[0].towerHp == m.players[0].towerHp, "snapshot turn/hp");
		std::printf("OK snapshot (%zu bytes)\n", bytes.size());
	}

	// --- v0.8 #85: beacon pack/parse + live UDP round-trip via LanDiscovery ---
	{
		net::BeaconInfo b;
		std::snprintf(b.code, sizeof(b.code), "ABCD");
		b.version = kProtocolVersion;
		b.tcpPort = 31111;
		std::snprintf(b.hostName, sizeof(b.hostName), "BeaconBot");
		b.seatsTaken = 2;
		b.seatsTotal = 4;
		const auto dgram = net::packBeacon(b);
		net::BeaconInfo b2;
		CHECK(net::parseBeacon(dgram.data(), dgram.size(), b2), "beacon parse");
		CHECK(std::strcmp(b2.code, "ABCD") == 0 && b2.tcpPort == 31111, "beacon fields");
		CHECK(!net::parseBeacon(dgram.data(), 4, b2), "short beacon rejected");

		LanDiscovery disc;
		CHECK(disc.start(), "discovery starts");
		Match hm;
		NetSession host;
		MatchConfig cfg;
		cfg.seed = 5;
		CHECK(host.hostLobby(hm, cfg, "LanHost", 27130), "beacon host lobby");
		CHECK(host.roomCode()[0] != '\0', "room code generated");
		// Beacons fire ~1/s; the first one is immediate (lastBeacon_ backdated).
		for (int i = 0; i < 100 && disc.entries().empty(); ++i) {
			pumpHost(host, hm, 1);
			disc.update();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		CHECK(!disc.entries().empty(), "beacon received on loopback");
		CHECK(disc.findByCode(host.roomCode()) != nullptr, "find by room code");
		char lower[kRoomCodeLen];
		for (int i = 0; i < kRoomCodeLen; ++i) {
			lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(host.roomCode()[i])));
		}
		CHECK(disc.findByCode(lower) != nullptr, "room code case-insensitive");
		disc.stop();
		host.shutdown(&hm);
		std::printf("OK LAN beacon + room code\n");
	}

	// --- v0.8 #88: version mismatch → readable reject ---
	{
		Match hm;
		NetSession host;
		MatchConfig cfg;
		cfg.seed = 7;
		CHECK(host.hostLobby(hm, cfg, "VerHost", 27131), "verhost lobby");
		net::TcpSocket raw;
		CHECK(raw.connect("127.0.0.1", 27131), "raw connect");
		auto hello = net::makeHello("OldClient", 0);
		hello[7] = 0xEE; // corrupt version field (header: magic[0..3] type[4..5] version[6..7])
		auto framed = net::FrameCodec::pack(hello);
		raw.send(framed.data(), static_cast<int>(framed.size()));
		bool gotReject = false;
		for (int i = 0; i < 100 && !gotReject; ++i) {
			pumpHost(host, hm, 1);
			gotReject = rawExpectReject(raw, "Protocol");
		}
		CHECK(gotReject, "version mismatch rejected with message");
		raw.close();
		host.shutdown(&hm);
		std::printf("OK version mismatch reject\n");
	}

	// --- v0.8 #114: malformed frames must not kill the host ---
	{
		Match hm;
		NetSession host;
		MatchConfig cfg;
		cfg.seed = 9;
		CHECK(host.hostLobby(hm, cfg, "FuzzHost", 27132), "fuzzhost lobby");
		net::TcpSocket raw;
		CHECK(raw.connect("127.0.0.1", 27132), "fuzz connect");
		// Oversized frame length + garbage payload.
		uint8_t evil[64];
		std::memset(evil, 0xAB, sizeof(evil));
		evil[0] = 0xFF;
		evil[1] = 0xFF;
		evil[2] = 0xFF;
		evil[3] = 0x7F;
		raw.send(evil, sizeof(evil));
		pumpHost(host, hm, 20);

		// v1.0 #177: sustained fuzz — valid frames with random types/payloads must never
		// crash the host (each fresh socket, since bad frames get the client dropped).
		uint32_t fuzzRng = 0xFADEDBEEu;
		for (int round = 0; round < 40; ++round) {
			net::TcpSocket fz;
			if (!fz.connect("127.0.0.1", 27132)) {
				continue;
			}
			std::vector<uint8_t> payload;
			payload.push_back(0x54); // 'T' — sometimes forms the real magic below
			for (int b = 0; b < 48; ++b) {
				payload.push_back(static_cast<uint8_t>(xorshift32(fuzzRng) & 0xFF));
			}
			if (round % 3 == 0) {
				// Real header magic + random type/version + junk payload.
				payload[0] = 0x54;
				payload[1] = 0x59;
				payload[2] = 0x32;
				payload[3] = 0x4D;
			}
			const auto framed = net::FrameCodec::pack(payload);
			fz.send(framed.data(), static_cast<int>(framed.size()));
			pumpHost(host, hm, 2);
			fz.close();
		}
		pumpHost(host, hm, 10);
		// Host survives and still accepts a legitimate client.
		Match cm;
		NetSession client;
		CHECK(client.joinLobby(cm, "127.0.0.1", 27132, "PostFuzz"), "join after fuzz");
		CHECK(waitForJoin(host, hm, client, cm), "legit client joins after fuzz");
		raw.close();
		client.shutdown(&cm);
		host.shutdown(&hm);
		std::printf("OK malformed-frame fuzz\n");
	}

	// --- v0.8 #79/#80: lobby lock, kick, ready gate ---
	{
		Match hm, cm;
		NetSession host, client;
		MatchConfig cfg;
		cfg.seed = 11;
		cfg.fillEmptyWithAI = true;
		CHECK(host.hostLobby(hm, cfg, "LockHost", 27133), "lockhost lobby");
		CHECK(client.joinLobby(cm, "127.0.0.1", 27133, "Lockee"), "lockee join");
		CHECK(waitForJoin(host, hm, client, cm), "lockee seated");

		// #80: start blocked while the remote human is not ready.
		CHECK(!host.hostStartMatch(hm), "start blocked until ready");
		// #79: locked lobby rejects the next fresh join.
		host.hostSetLobbyLocked(hm, true);
		Match cm2;
		NetSession late;
		CHECK(late.joinLobby(cm2, "127.0.0.1", 27133, "TooLate"), "late connects");
		bool lateRejected = false;
		for (int i = 0; i < 200 && !lateRejected; ++i) {
			pump(host, hm, late, cm2, 1);
			lateRejected = late.connectionLost() ||
						   (late.lastError()[0] && std::strstr(late.lastError(), "closed") != nullptr) ||
						   (late.status() && std::strstr(late.status(), "closed") != nullptr);
		}
		CHECK(lateRejected, "locked lobby rejects join");
		late.shutdown(&cm2);
		host.hostSetLobbyLocked(hm, false);

		// #79: kick → client sees a reason.
		const int seat = client.localSeat();
		CHECK(host.hostKick(hm, seat), "kick executes");
		bool sawKick = false;
		for (int i = 0; i < 200 && !sawKick; ++i) {
			pump(host, hm, client, cm, 1);
			sawKick = client.connectionLost() &&
					  std::strstr(client.connectionLostReason(), "Kicked") != nullptr;
		}
		CHECK(sawKick, "client sees kick reason");
		CHECK(hm.players[static_cast<size_t>(seat)].control == SeatControl::Empty, "kicked seat freed");
		client.shutdown(&cm);
		host.shutdown(&hm);
		std::printf("OK lock + kick + ready gate\n");
	}

	// --- v0.8 #113: abrupt disconnect mid-lobby frees the seat ---
	{
		Match hm, cm;
		NetSession host, client;
		MatchConfig cfg;
		cfg.seed = 13;
		CHECK(host.hostLobby(hm, cfg, "DropHost", 27134), "drophost lobby");
		CHECK(client.joinLobby(cm, "127.0.0.1", 27134, "Dropper"), "dropper join");
		CHECK(waitForJoin(host, hm, client, cm), "dropper seated");
		const int seat = client.localSeat();
		client.shutdown(&cm); // abrupt: no goodbye
		bool freed = false;
		for (int i = 0; i < 300 && !freed; ++i) {
			pumpHost(host, hm, 1);
			freed = hm.players[static_cast<size_t>(seat)].control == SeatControl::Empty;
		}
		CHECK(freed, "mid-lobby drop frees seat");
		host.shutdown(&hm);
		std::printf("OK mid-lobby disconnect\n");
	}

	// --- v0.8 #100/#99: mid-match drop → AI takeover; token reclaim gets the seat back ---
	{
		Match hm, cm;
		NetSession host, client;
		MatchConfig cfg;
		cfg.seed = 17;
		cfg.fillEmptyWithAI = true;
		CHECK(host.hostLobby(hm, cfg, "AwayHost", 27135), "awayhost lobby");
		CHECK(client.joinLobby(cm, "127.0.0.1", 27135, "Wanderer"), "wanderer join");
		CHECK(waitForJoin(host, hm, client, cm), "wanderer seated");
		client.requestReady(cm, true);
		pump(host, hm, client, cm, 10);
		CHECK(host.hostStartMatch(hm), "away match starts");
		pump(host, hm, client, cm, 20);
		const int seat = client.localSeat();
		CHECK(cm.phase == Phase::Playing, "wanderer sees match");

		// Drop the client's socket without goodbye — same NetSession keeps its token.
		// (shutdown() preserves clientToken_ by design.)
		client.shutdown(&cm);
		bool aiTakeover = false;
		for (int i = 0; i < 300 && !aiTakeover; ++i) {
			pumpHost(host, hm, 1);
			aiTakeover = hm.phase != Phase::Playing ||
						 hm.players[static_cast<size_t>(seat)].control == SeatControl::AI;
		}
		CHECK(aiTakeover, "mid-match drop -> AI takeover");

		if (hm.phase == Phase::Playing) {
			// Reclaim with the preserved token.
			CHECK(client.joinLobby(cm, "127.0.0.1", 27135, "Wanderer"), "wanderer rejoins");
			bool reclaimed = false;
			for (int i = 0; i < 300 && !reclaimed; ++i) {
				pump(host, hm, client, cm, 1);
				reclaimed = client.localSeat() == seat &&
							hm.players[static_cast<size_t>(seat)].control == SeatControl::HumanRemote;
			}
			CHECK(reclaimed, "token reclaim restores seat");
		}

		// Match must still be able to finish regardless.
		int safety = 0;
		while (hm.phase == Phase::Playing && safety < 900) {
			playActiveSeat(host, hm, client, cm);
			++safety;
		}
		for (int i = 0; i < 200 && hm.phase == Phase::Playing; ++i) {
			pump(host, hm, client, cm, 2);
		}
		CHECK(hm.phase == Phase::GameOver, "match finishes after drop/reclaim");
		client.shutdown(&cm);
		host.shutdown(&hm);
		std::printf("OK mid-match drop + reclaim\n");
	}

	// --- Full host+client loopback match (original M2 flow, now with heartbeats/intents) ---
	{
		Match hostMatch, clientMatch;
		NetSession host, client;
		MatchConfig cfg;
		cfg.playerCount = 4;
		cfg.fillEmptyWithAI = true;
		cfg.seed = 2026;

		CHECK(host.hostLobby(hostMatch, cfg, "HostBot", kDefaultPort), "hostLobby");
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
		CHECK(client.joinLobby(clientMatch, "127.0.0.1", kDefaultPort, "ClientBot"), "joinLobby");
		CHECK(waitForJoin(host, hostMatch, client, clientMatch), "client joined");
		std::printf("OK client seat %d\n", client.localSeat());

		client.requestReady(clientMatch, true);
		pump(host, hostMatch, client, clientMatch, 10);

		// #81: chat lands in the host log with the sender name.
		client.requestChat(clientMatch, "gl hf");
		pump(host, hostMatch, client, clientMatch, 10);
		bool sawChat = false;
		for (const MatchEvent& e : hostMatch.log) {
			if (e.text.find("ClientBot: gl hf") != std::string::npos) {
				sawChat = true;
				break;
			}
		}
		CHECK(sawChat, "chat relayed with sender name");

		CHECK(host.hostStartMatch(hostMatch), "hostStartMatch");
		pump(host, hostMatch, client, clientMatch, 20);
		CHECK(clientMatch.phase == Phase::Playing, "client sees Playing");
		std::printf("OK match started id=%u\n", hostMatch.matchId);

		int safety = 0;
		while (hostMatch.phase == Phase::Playing && safety < 600) {
			const uint32_t syncBefore = hostMatch.syncGeneration;
			const int turnBefore = hostMatch.turnNumber;
			playActiveSeat(host, hostMatch, client, clientMatch);
			if (hostMatch.syncGeneration == syncBefore && hostMatch.turnNumber == turnBefore &&
				hostMatch.phase == Phase::Playing) {
				pump(host, hostMatch, client, clientMatch, 5);
			}
			++safety;
		}
		for (int i = 0; i < 200 && hostMatch.phase == Phase::Playing; ++i) {
			pump(host, hostMatch, client, clientMatch, 2);
		}
		pump(host, hostMatch, client, clientMatch, 30);

		CHECK(hostMatch.phase == Phase::GameOver, "host finished");
		CHECK(clientMatch.phase == Phase::GameOver, "client finished");
		CHECK(clientMatch.winner == hostMatch.winner, "winner in sync");

		// #83: heartbeats produced a ping measurement for the remote seat.
		CHECK(host.seatPingMs(client.localSeat()) >= 0, "ping measured");

		std::printf("OK loopback match winner=%s turns=%d\n",
					hostMatch.winner >= 0 ? hostMatch.players[static_cast<size_t>(hostMatch.winner)].name : "none",
					hostMatch.turnNumber);

		// #109: rematch by unanimous vote (host + client).
		const uint32_t prevMatchId = hostMatch.matchId;
		client.requestRematchVote(clientMatch, true);
		pump(host, hostMatch, client, clientMatch, 10);
		host.requestRematchVote(hostMatch, true);
		pump(host, hostMatch, client, clientMatch, 20);
		CHECK(hostMatch.matchId == prevMatchId + 1 && hostMatch.phase == Phase::Playing,
			  "unanimous votes start rematch");
		std::printf("OK rematch votes\n");
		host.shutdown(&hostMatch);
		client.shutdown(&clientMatch);
	}

	// --- v0.8 #115: 3 remote clients + host, full match loopback stress ---
	{
		Match hm;
		NetSession host;
		MatchConfig cfg;
		cfg.playerCount = 4;
		cfg.fillEmptyWithAI = true;
		cfg.seed = 31337;
		CHECK(host.hostLobby(hm, cfg, "StressHost", 27136), "stress host");

		Match cms[3];
		NetSession clients[3];
		const char* names[3] = { "S1", "S2", "S3" };
		for (int i = 0; i < 3; ++i) {
			CHECK(clients[i].joinLobby(cms[i], "127.0.0.1", 27136, names[i]), "stress join");
		}
		bool allSeated = false;
		for (int it = 0; it < 500 && !allSeated; ++it) {
			host.update(hm);
			for (int i = 0; i < 3; ++i) {
				clients[i].update(cms[i]);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			allSeated = clients[0].localSeat() >= 1 && clients[1].localSeat() >= 1 && clients[2].localSeat() >= 1;
		}
		CHECK(allSeated, "3 clients seated");
		for (int i = 0; i < 3; ++i) {
			clients[i].requestReady(cms[i], true);
		}
		for (int it = 0; it < 30; ++it) {
			host.update(hm);
			for (int i = 0; i < 3; ++i) {
				clients[i].update(cms[i]);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		CHECK(host.hostStartMatch(hm), "stress match starts");

		int safety = 0;
		while (hm.phase == Phase::Playing && safety < 4000) {
			host.update(hm);
			for (int i = 0; i < 3; ++i) {
				clients[i].update(cms[i]);
			}
			// Whoever's turn it is (remote human), that client plays first legal card.
			const int ap = hm.activePlayer;
			for (int i = 0; i < 3; ++i) {
				if (clients[i].localSeat() == ap && cms[i].phase == Phase::Playing) {
					Player& p = cms[i].players[static_cast<size_t>(ap)];
					bool sent = false;
					for (int h = 0; h < static_cast<int>(p.hand.size()) && !sent; ++h) {
						for (int t = 0; t < cms[i].config.playerCount; ++t) {
							if (canPlayCard(cms[i], ap, h, t)) {
								clients[i].requestPlayCard(cms[i], h, t);
								sent = true;
								break;
							}
						}
					}
					if (!sent) {
						clients[i].requestEndTurn(cms[i]);
					}
				}
			}
			if (hm.activePlayer == host.localSeat() && hm.phase == Phase::Playing) {
				host.requestEndTurn(hm); // host just cycles
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			++safety;
		}
		for (int i = 0; i < 300 && hm.phase == Phase::Playing; ++i) {
			host.update(hm);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		CHECK(hm.phase == Phase::GameOver, "4-seat stress match finishes");
		// All clients converge on the result.
		for (int it = 0; it < 100; ++it) {
			host.update(hm);
			for (int i = 0; i < 3; ++i) {
				clients[i].update(cms[i]);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		int converged = 0;
		for (int i = 0; i < 3; ++i) {
			if (cms[i].phase == Phase::GameOver && cms[i].winner == hm.winner) {
				++converged;
			}
		}
		CHECK(converged == 3, "3 clients converge on winner");
		for (int i = 0; i < 3; ++i) {
			clients[i].shutdown(&cms[i]);
		}
		host.shutdown(&hm);
		std::printf("OK 4-seat loopback stress (turns=%d)\n", hm.turnNumber);
	}

	// --- v1.2 #95/#96: compression round-trip + delta apply/desync recovery ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 424;
		startMatch(m, cfg);
		const auto full = serializeMatch(m);
		CHECK(full.size() > 2048, "snapshot big enough to hit the compression path");
		const auto comp = net::makeCompressedState(static_cast<uint8_t>(net::MsgType::MatchState), full.data(), full.size());
		CHECK(!comp.empty() && comp.size() < full.size(), "lz4 actually shrinks the snapshot");
		net::MsgHeader hdr{};
		const uint8_t* payload = nullptr;
		size_t psz = 0;
		CHECK(net::parseHeader(comp.data(), comp.size(), hdr, payload, psz), "compressed header parses");
		uint8_t innerType = 0;
		std::vector<uint8_t> raw;
		CHECK(net::readCompressedState(payload, psz, innerType, raw), "decompress ok");
		CHECK(raw == full, "lossless round-trip");

		// Delta: base state -> play a card -> delta applies cleanly on a copy of base.
		Match base = m;
		autoPlayBest(m);
		const auto delta = serializeMatchDelta(m, base);
		// A one-play delta re-sends the log tail + up to 2 dirty players, so the win vs a
		// full 4-player snapshot is meaningful but not dramatic at this game size.
		CHECK(delta.size() < full.size(), "delta smaller than full snapshot");
		Match noop = m;
		const auto idleDelta = serializeMatchDelta(m, noop);
		CHECK(idleDelta.size() < full.size() / 2, "no-change delta drops all player blobs");
		Match clientCopy = base;
		CHECK(applyMatchDelta(clientCopy, delta.data(), delta.size()) == DeltaResult::Ok, "delta applies");
		CHECK(clientCopy.syncGeneration == m.syncGeneration && clientCopy.turnNumber == m.turnNumber,
			  "delta reproduces head state");
		for (int i = 0; i < m.config.playerCount; ++i) {
			CHECK(clientCopy.players[static_cast<size_t>(i)].towerHp == m.players[static_cast<size_t>(i)].towerHp,
				  "delta HP matches");
		}
		// Desync: applying the same delta again must demand a resync, not corrupt state.
		CHECK(applyMatchDelta(clientCopy, delta.data(), delta.size()) == DeltaResult::NeedResync,
			  "stale delta demands resync");
		std::printf("OK compression + delta unit\n");
	}

	// --- v1.2 #111: spectator joins, watches, never takes a seat ---
	{
		Match hm, sm;
		NetSession host, spec;
		MatchConfig cfg;
		cfg.seed = 505;
		cfg.fillEmptyWithAI = true;
		CHECK(host.hostLobby(hm, cfg, "SpecHost", 27140), "spec host lobby");
		CHECK(spec.joinLobby(sm, "127.0.0.1", 27140, "Watcher", true), "spectator connects");
		bool welcomed = false;
		for (int i = 0; i < 300 && !welcomed; ++i) {
			pump(host, hm, spec, sm, 1);
			welcomed = spec.localSeat() == kSpectatorSeat;
		}
		CHECK(welcomed, "spectator welcomed with spectator seat");
		CHECK(spec.isSpectator(), "isSpectator flag");
		int taken = 0;
		for (int i = 0; i < hm.config.playerCount; ++i) {
			if (hm.players[static_cast<size_t>(i)].control == SeatControl::HumanRemote) {
				++taken;
			}
		}
		CHECK(taken == 0, "spectator takes no seat");
		// v1.2 #84: banner color propagates.
		host.hostSetBannerColor(hm, 5);
		CHECK(host.hostStartMatch(hm), "spec match starts");
		pump(host, hm, spec, sm, 30);
		CHECK(sm.phase == Phase::Playing, "spectator sees the match");
		CHECK(sm.bannerColor == 5, "banner color reaches spectator");
		host.shutdown(&hm);
		spec.shutdown(&sm);
		std::printf("OK spectator + banner\n");
	}

	// --- v1.2 #101/#102: host migration — election, adoption, name-reattach ---
	{
		Match hm, c1m, c2m;
		NetSession host, c1, c2;
		MatchConfig cfg;
		cfg.seed = 606;
		cfg.fillEmptyWithAI = true;
		CHECK(host.hostLobby(hm, cfg, "DoomedHost", 27141), "doomed host lobby");
		CHECK(c1.joinLobby(c1m, "127.0.0.1", 27141, "Alpha"), "alpha joins");
		CHECK(waitForJoin(host, hm, c1, c1m), "alpha seated");
		CHECK(c2.joinLobby(c2m, "127.0.0.1", 27141, "Bravo"), "bravo joins");
		bool bravoIn = false;
		for (int i = 0; i < 400 && !bravoIn; ++i) {
			host.update(hm);
			c1.update(c1m);
			c2.update(c2m);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			bravoIn = c2.localSeat() >= 1;
		}
		CHECK(bravoIn, "bravo seated");
		const int alphaSeat = c1.localSeat();
		const int bravoSeat = c2.localSeat();
		// let peer IPs propagate to everyone
		for (int i = 0; i < 40; ++i) {
			host.update(hm);
			c1.update(c1m);
			c2.update(c2m);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		CHECK(c2m.peerIp[alphaSeat][0] != '\0', "bravo knows the alpha peer ip");

		// Host vanishes without a goodbye.
		host.shutdown(&hm);
		bool c1Lost = false, c2Lost = false;
		for (int i = 0; i < 400 && !(c1Lost && c2Lost); ++i) {
			c1.update(c1m);
			c2.update(c2m);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			c1Lost = c1.connectionLost();
			c2Lost = c2.connectionLost();
		}
		CHECK(c1Lost && c2Lost, "both survivors notice the host loss");

		// Election: lowest surviving seat (alpha) becomes the new authority.
		const auto p1 = c1.planMigration(c1m, 27141);
		const auto p2 = c2.planMigration(c2m, 27141);
		CHECK(p1.role == NetSession::MigrationPlan::Role::BecomeHost, "alpha elected host");
		CHECK(p2.role == NetSession::MigrationPlan::Role::JoinNewHost, "bravo told to rejoin");
		CHECK(p2.newHostSeat == alphaSeat, "bravo agrees on the new host seat");

		c1.clearConnectionLost();
		c2.clearConnectionLost();
		CHECK(c1.hostAdoptMatch(c1m, 27141, "Alpha"), "alpha adopts the match");
		CHECK(c1.mode() == AppMode::Host && c1.localSeat() == alphaSeat, "alpha hosts from the same seat");
		CHECK(c1m.players[0].control == SeatControl::AI, "vanished host seat handed to AI");

		CHECK(c2.joinLobby(c2m, p2.ip, p2.port, "Bravo"), "bravo rejoins the new host");
		bool reattached = false;
		for (int i = 0; i < 500 && !reattached; ++i) {
			c1.update(c1m);
			c2.update(c2m);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			reattached = c2.localSeat() == bravoSeat &&
						 c1m.players[static_cast<size_t>(bravoSeat)].control == SeatControl::HumanRemote;
		}
		CHECK(reattached, "bravo reattached to the old seat by name");
		c1.shutdown(&c1m);
		c2.shutdown(&c2m);
		std::printf("OK host migration\n");
	}

	if (g_fails) {
		std::printf("%d failure(s)\n", g_fails);
		return 1;
	}
	std::printf("OK v1.2 net tests\n");
	return 0;
}
