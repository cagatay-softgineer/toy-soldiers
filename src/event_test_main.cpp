#include "game/cards.h"
#include "game/events.h"
#include "game/match.h"
#include "game/rules.h"
#include "game/snapshot.h"

#include <cstdio>

static int g_fails = 0;

#define CHECK(cond, msg)                                                                                               \
	do {                                                                                                               \
		if (!(cond)) {                                                                                                 \
			std::printf("FAIL: %s\n", msg);                                                                            \
			++g_fails;                                                                                                 \
		}                                                                                                              \
	} while (0)

int main()
{
	using namespace toy;

	// --- Sandstorm forces adjacent ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 7;
		cfg.mapId = MapId::Desert;
		cfg.freeTargeting = true;
		startMatch(m, cfg);
		CHECK(canPlayCard(m, 0, 0, 2) || !m.players[0].hand.empty(), "baseline hand");
		// Find an attack card
		int handAtk = -1;
		for (int h = 0; h < static_cast<int>(m.players[0].hand.size()); ++h) {
			const CardDef* d = findCard(m.players[0].hand[static_cast<size_t>(h)].defId);
			if (d && d->klass == CardClass::Attack) {
				handAtk = h;
				break;
			}
		}
		CHECK(handAtk >= 0, "has attack card");
		// Free targeting allows opposite seat (0→2)
		CHECK(canPlayCard(m, 0, handAtk, 2), "free target opposite before sandstorm");

		forceWorldEvent(m, EventKind::Sandstorm);
		CHECK(eventForcesAdjacent(m), "sandstorm active");
		CHECK(!canPlayCard(m, 0, handAtk, 2), "sandstorm blocks opposite");
		CHECK(canPlayCard(m, 0, handAtk, 1), "sandstorm allows adjacent");
		std::printf("OK sandstorm targeting\n");
	}

	// --- Scout Peek bonus persists across endTurn ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 31;
		cfg.eventsEnabled = false;
		startMatch(m, cfg);
		m.players[0].attackBonusNext = 1;
		endTurn(m);
		// After endTurn active player advanced; seat 0 should still hold bonus if not cleared.
		// endTurn no longer clears attackBonusNext.
		CHECK(m.players[0].attackBonusNext == 1, "scout bonus persists");
		std::printf("OK scout bonus persists\n");
	}

	// --- Rain reduces damage ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 11;
		startMatch(m, cfg);
		CardDef fake = *findCard(1); // Plastic Volley power 3
		const int dry = computeAttackDamage(m, 0, 1, fake);
		forceWorldEvent(m, EventKind::Rain);
		const int wet = computeAttackDamage(m, 0, 1, fake);
		CHECK(wet == dry - 1 || (dry <= 1 && wet == dry), "rain reduces damage");
		std::printf("OK rain damage dry=%d wet=%d\n", dry, wet);
	}

	// --- Flood leak + shield block ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 13;
		startMatch(m, cfg);
		forceWorldEvent(m, EventKind::Flood, 1);
		CHECK(seatIsFlooded(m, 1), "seat 1 flooded");
		const int hpBefore = m.players[1].towerHp;
		// Simulate turn advance onto seat 1
		m.activePlayer = 1;
		tickWorldEvents(m);
		CHECK(m.players[1].towerHp == hpBefore - 1, "flood leaks 1 HP");

		// Shield blocks
		forceWorldEvent(m, EventKind::Flood, 1);
		m.players[1].shieldTurns = 2;
		const int hp2 = m.players[1].towerHp;
		m.activePlayer = 1;
		tickWorldEvents(m);
		CHECK(m.players[1].towerHp == hp2, "shield blocks flood");
		std::printf("OK flood + shield\n");
	}

	// --- Cat warning → pounce impulse ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 17;
		cfg.mapId = MapId::LivingRoom;
		startMatch(m, cfg);
		forceWorldEvent(m, EventKind::Cat, 0);
		CHECK(m.world.warning, "cat warning");
		tickWorldEvents(m);
		CHECK(!m.world.warning, "cat pounced");
		CHECK(m.pendingPhysicsImpulse.frames > 0, "cat sets physics impulse");
		std::printf("OK cat pounce\n");
	}

	// --- Snapshot round-trip preserves world event ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 19;
		cfg.mapId = MapId::Backyard;
		startMatch(m, cfg);
		forceWorldEvent(m, EventKind::Rain);
		const auto bytes = serializeMatch(m);
		Match m2;
		CHECK(deserializeMatch(m2, bytes.data(), bytes.size()), "deserialize v2");
		CHECK(m2.config.mapId == MapId::Backyard, "map id");
		CHECK(m2.world.kind == EventKind::Rain, "event kind");
		CHECK(m2.world.remainingTurns == m.world.remainingTurns, "event turns");
		std::printf("OK snapshot v2 (%zu bytes)\n", bytes.size());
	}

	// --- Full match still completes with events ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 202603;
		cfg.mapId = MapId::Desert;
		cfg.eventsEnabled = true;
		startMatch(m, cfg);
		int safety = 0;
		while (m.phase == Phase::Playing && safety < 500) {
			autoPlayBest(m);
			++safety;
		}
		CHECK(m.phase == Phase::GameOver, "match finishes with events");
		std::printf("OK full match with events in %d plays winner=%s\n", safety,
					m.winner >= 0 ? m.players[static_cast<size_t>(m.winner)].name : "?");
	}

	if (g_fails) {
		std::printf("%d failure(s)\n", g_fails);
		return 1;
	}
	std::printf("OK M3 event tests\n");
	return 0;
}
