#include "game/match.h"
#include "game/rules.h"
#include "game/snapshot.h"
#include "net/session.h"

#include <chrono>
#include <cstdio>
#include <thread>

namespace {

void pump(toy::NetSession& host, toy::Match& hostMatch, toy::NetSession& client, toy::Match& clientMatch, int times = 1)
{
	for (int i = 0; i < times; ++i) {
		host.update(hostMatch);
		client.update(clientMatch);
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
	}
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
		if (!deserializeMatch(m2, bytes.data(), bytes.size())) {
			std::printf("FAIL: deserializeMatch\n");
			return 1;
		}
		if (m2.turnNumber != m.turnNumber || m2.players[0].towerHp != m.players[0].towerHp) {
			std::printf("FAIL: snapshot mismatch turn/hp\n");
			return 2;
		}
		std::printf("OK snapshot (%zu bytes)\n", bytes.size());
	}

	// --- Host + client loopback ---
	Match hostMatch;
	Match clientMatch;
	NetSession host;
	NetSession client;

	MatchConfig cfg;
	cfg.playerCount = 4;
	cfg.fillEmptyWithAI = true;
	cfg.seed = 2026;

	if (!host.hostLobby(hostMatch, cfg, "HostBot", kDefaultPort)) {
		std::printf("FAIL: hostLobby: %s\n", host.status());
		return 3;
	}
	std::printf("Host listening on %u\n", static_cast<unsigned>(host.listenPort()));
	std::this_thread::sleep_for(std::chrono::milliseconds(30));

	if (!client.joinLobby(clientMatch, "127.0.0.1", kDefaultPort, "ClientBot")) {
		std::printf("FAIL: joinLobby: %s\n", client.status());
		return 4;
	}

	bool joined = false;
	for (int i = 0; i < 300; ++i) {
		pump(host, hostMatch, client, clientMatch, 1);
		if (client.localSeat() >= 1) {
			const int s = client.localSeat();
			if (hostMatch.players[static_cast<size_t>(s)].control == SeatControl::HumanRemote) {
				joined = true;
				break;
			}
		}
	}
	if (!joined) {
		std::printf("FAIL: client never joined (host=%s client=%s seat=%d)\n", host.status(), client.status(),
					client.localSeat());
		return 5;
	}
	std::printf("OK client seat %d\n", client.localSeat());

	client.requestReady(clientMatch, true);
	pump(host, hostMatch, client, clientMatch, 10);

	if (!host.hostStartMatch(hostMatch)) {
		std::printf("FAIL: hostStartMatch\n");
		return 6;
	}
	pump(host, hostMatch, client, clientMatch, 20);

	if (clientMatch.phase != Phase::Playing) {
		std::printf("FAIL: client phase=%s (expected Playing)\n", phaseName(clientMatch.phase));
		return 7;
	}
	std::printf("OK match started id=%u\n", hostMatch.matchId);

	int safety = 0;
	while (hostMatch.phase == Phase::Playing && safety < 600) {
		const uint32_t turnBefore = static_cast<uint32_t>(hostMatch.turnNumber);
		const uint32_t syncBefore = hostMatch.syncGeneration;
		playActiveSeat(host, hostMatch, client, clientMatch);
		// Ensure progress
		if (hostMatch.syncGeneration == syncBefore && hostMatch.turnNumber == static_cast<int>(turnBefore) &&
			hostMatch.phase == Phase::Playing) {
			// Force AI / host tick
			pump(host, hostMatch, client, clientMatch, 5);
		}
		++safety;
	}

	// Drain remaining AI
	for (int i = 0; i < 200 && hostMatch.phase == Phase::Playing; ++i) {
		pump(host, hostMatch, client, clientMatch, 2);
	}

	pump(host, hostMatch, client, clientMatch, 30);

	if (hostMatch.phase != Phase::GameOver) {
		std::printf("FAIL: host did not finish (phase=%s turn=%d)\n", phaseName(hostMatch.phase), hostMatch.turnNumber);
		return 8;
	}
	if (clientMatch.phase != Phase::GameOver) {
		std::printf("FAIL: client phase=%s (expected GameOver)\n", phaseName(clientMatch.phase));
		return 9;
	}
	if (clientMatch.winner != hostMatch.winner) {
		std::printf("FAIL: winner desync host=%d client=%d\n", hostMatch.winner, clientMatch.winner);
		return 10;
	}

	std::printf("OK game over winner=%s turns=%d pumps=%d\n",
				hostMatch.winner >= 0 ? hostMatch.players[static_cast<size_t>(hostMatch.winner)].name : "none",
				hostMatch.turnNumber, safety);

	host.shutdown(&hostMatch);
	client.shutdown(&clientMatch);
	std::printf("OK M2 net test\n");
	return 0;
}
