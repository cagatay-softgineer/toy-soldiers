#include "game/cards.h"
#include "game/events.h"
#include "game/match.h"
#include "game/rules.h"
#include "game/snapshot.h"

#include <cstdio>

// v0.7 Deep Toybox gate: towers, keywords, modes, AI tiers, event cards.

static int g_fails = 0;

#define CHECK(cond, msg)                                                                                               \
	do {                                                                                                               \
		if (!(cond)) {                                                                                                 \
			std::printf("FAIL: %s\n", msg);                                                                            \
			++g_fails;                                                                                                 \
		}                                                                                                              \
	} while (0)

using namespace toy;

namespace {

Match makeMatch(GameMode mode, uint32_t seed, bool events = false)
{
	MatchConfig cfg;
	cfg.playerCount = 4;
	cfg.seed = seed;
	cfg.eventsEnabled = events;
	cfg.fillEmptyWithAI = true;
	cfg.mode = mode;
	Match m;
	startMatch(m, cfg);
	return m;
}

// Force a known card into hand slot 0 of the given seat.
void forceHandCard(Match& m, int seat, int defId)
{
	Player& p = m.players[static_cast<size_t>(seat)];
	if (p.hand.empty()) {
		p.hand.push_back(CardInstance{ defId, 9000 + seat });
	} else {
		p.hand[0].defId = defId;
	}
}

bool autoFinish(Match& m, int cap = 600)
{
	int safety = 0;
	while (m.phase == Phase::Playing && safety < cap) {
		if (!autoPlayBest(m)) {
			return false;
		}
		++safety;
	}
	return m.phase == Phase::GameOver;
}

} // namespace

int main()
{
	// --- Catalog size + event-card cap in decks (#43, #63) ---
	{
		CHECK(static_cast<int>(cardCatalog().size()) >= 22, "catalog has >= 22 defs");
		Match m = makeMatch(GameMode::ClassicFFA, 5);
		for (int i = 0; i < m.config.playerCount; ++i) {
			const Player& p = m.players[static_cast<size_t>(i)];
			int events = 0;
			int total = 0;
			auto countIn = [&](const std::vector<CardInstance>& v) {
				for (const CardInstance& c : v) {
					const CardDef* d = findCard(c.defId);
					if (d && d->klass == CardClass::Event) {
						++events;
					}
					++total;
				}
			};
			countIn(p.deck);
			countIn(p.hand);
			countIn(p.discard);
			CHECK(events <= 2, "deck holds at most 2 event cards");
			CHECK(total >= 25, "FFA deck size (30 recipe + tower card - draws)");
		}
		std::printf("OK catalog + event cap\n");
	}

	// --- Shield Bearer (#37): starts with 1 shield, attacks -1 (min 1) ---
	{
		Match m = makeMatch(GameMode::ClassicFFA, 7);
		m.players[0].tower = TowerType::ShieldBearer;
		const CardDef* volley = findCard(1); // power 3
		m.players[1].shieldTurns = 0;
		const int dmg = computeAttackDamage(m, 0, 1, *volley);
		CHECK(dmg == 2, "shield bearer -1 attack (3 -> 2)");
		const CardDef* sling = findCard(8); // power 1
		const int dmg1 = computeAttackDamage(m, 0, 1, *sling);
		CHECK(dmg1 == 1, "shield bearer never drops chip below 1");
		// Starting shield comes from applyTowerStats path:
		MatchConfig cfg;
		cfg.mode = GameMode::ClassicFFA;
		cfg.seed = 7;
		cfg.eventsEnabled = false;
		Match ms;
		initLobby(ms, cfg);
		ms.players[0].control = SeatControl::HumanLocal;
		ms.players[0].ready = true;
		ms.players[0].tower = TowerType::ShieldBearer;
		startMatchFromLobby(ms);
		CHECK(ms.players[0].shieldTurns == 1, "shield bearer starts with 1 shield turn");
		CHECK(ms.players[0].towerMaxHp == 35, "shield bearer 35 HP");
		std::printf("OK shield bearer\n");
	}

	// --- Sapper (#39): first attack +1, once ---
	{
		Match m = makeMatch(GameMode::ClassicFFA, 11);
		m.players[0].tower = TowerType::Sapper;
		m.players[0].firstAttackDone = false;
		const CardDef* volley = findCard(1);
		CHECK(computeAttackDamage(m, 0, 1, *volley) == 4, "sapper first attack 3+1");
		m.players[0].firstAttackDone = true;
		CHECK(computeAttackDamage(m, 0, 1, *volley) == 3, "sapper bonus spent");
		// Spending happens through applyCardPlay:
		m.players[0].firstAttackDone = false;
		forceHandCard(m, 0, 1);
		m.activePlayer = 0;
		CHECK(applyCardPlay(m, 0, 1), "sapper attack applies");
		CHECK(m.players[0].firstAttackDone, "sapper flag set after attack");
		std::printf("OK sapper\n");
	}

	// --- Pierce (#42): Marble Strike ignores shield ---
	{
		Match m = makeMatch(GameMode::ClassicFFA, 13);
		m.players[1].shieldTurns = 2;
		const CardDef* marble = findCard(7); // pierce 3
		const CardDef* volley = findCard(1); // normal 3
		CHECK(computeAttackDamage(m, 0, 1, *marble) == 3, "pierce ignores shield");
		CHECK(computeAttackDamage(m, 0, 1, *volley) == 2, "normal attack halved by shield");
		std::printf("OK pierce\n");
	}

	// --- AOE Tin Rain (#44): hits both neighbors, not opposite ---
	{
		Match m = makeMatch(GameMode::ClassicFFA, 17);
		m.activePlayer = 0;
		forceHandCard(m, 0, 6);
		const int hp1 = m.players[1].towerHp;
		const int hp2 = m.players[2].towerHp;
		const int hp3 = m.players[3].towerHp;
		CHECK(canPlayCard(m, 0, 0, 0), "AOE playable without enemy target");
		CHECK(applyCardPlay(m, 0, 0), "AOE applies");
		CHECK(m.players[1].towerHp == hp1 - 1, "AOE hit right neighbor");
		CHECK(m.players[3].towerHp == hp3 - 1, "AOE hit left neighbor");
		CHECK(m.players[2].towerHp == hp2, "AOE spares opposite seat");
		std::printf("OK tin rain AOE\n");
	}

	// --- Magnet Hand (#45): steals a shield turn ---
	{
		Match m = makeMatch(GameMode::ClassicFFA, 19);
		m.activePlayer = 0;
		m.players[1].shieldTurns = 2;
		m.players[0].shieldTurns = 0;
		forceHandCard(m, 0, 26);
		const int hp1 = m.players[1].towerHp;
		CHECK(applyCardPlay(m, 0, 1), "magnet applies");
		CHECK(m.players[1].shieldTurns == 1, "magnet removed a shield turn");
		CHECK(m.players[1].towerHp == hp1, "magnet deals no damage");
		std::printf("OK magnet hand\n");
	}

	// --- Line Cutter (#46): skip next player, capped ---
	{
		Match m = makeMatch(GameMode::ClassicFFA, 23);
		m.activePlayer = 0;
		forceHandCard(m, 0, 27);
		CHECK(applyCardPlay(m, 0, 0), "line cutter applies");
		// applyCardPlay ends turn 0; seat 1 was flagged and skipped -> active should be 2.
		CHECK(m.activePlayer == 2, "flagged player skipped");
		CHECK(!m.players[1].skipNextTurn, "skip flag consumed");
		std::printf("OK line cutter\n");
	}

	// --- Event cards (#63/#64/#65) cast world events ---
	{
		Match m = makeMatch(GameMode::ClassicFFA, 29);
		m.activePlayer = 0;
		forceHandCard(m, 0, 31); // pocket sand
		CHECK(applyCardPlay(m, 0, 0), "pocket sand applies");
		CHECK(m.world.kind == EventKind::Sandstorm, "pocket sand casts sandstorm");
		CHECK(eventForcesAdjacent(m), "sandstorm active after cast");

		Match m2 = makeMatch(GameMode::ClassicFFA, 31);
		m2.activePlayer = 0;
		forceHandCard(m2, 0, 30); // call the cat
		CHECK(applyCardPlay(m2, 0, 0), "call the cat applies");
		CHECK(m2.world.kind == EventKind::Cat, "cat event cast");
		std::printf("OK event cards\n");
	}

	// --- Dog + Blackout ambient events (#66/#67) ---
	{
		Match m = makeMatch(GameMode::ClassicFFA, 37);
		CHECK(forceWorldEvent(m, EventKind::Dog), "dog spawns");
		CHECK(m.pendingPhysicsImpulse.targetPlayer == kImpulseAllSeats, "dog shoves all seats");
		CHECK(forceWorldEvent(m, EventKind::Blackout), "blackout spawns");
		CHECK(eventHidesHp(m), "blackout hides HP");
		std::printf("OK dog + blackout\n");
	}

	// --- Quick Duel (#53): 2 seats, 24 HP, 15-card decks (+1 tower card) ---
	{
		MatchConfig cfg;
		cfg.mode = GameMode::QuickDuel;
		cfg.seed = 41;
		cfg.eventsEnabled = false;
		cfg.fillEmptyWithAI = true;
		Match m;
		startMatch(m, cfg);
		CHECK(m.config.playerCount == 2, "duel clamps to 2 players");
		CHECK(m.players[0].towerMaxHp == 24, "duel 24 HP towers");
		const int total = static_cast<int>(m.players[0].deck.size() + m.players[0].hand.size() +
										   m.players[0].discard.size());
		CHECK(total == 16, "duel deck = 15 recipe + 1 tower card");
		CHECK(autoFinish(m), "duel finishes");
		std::printf("OK quick duel\n");
	}

	// --- Teams 2v2 (#54/#72): no friendly fire, team win ---
	{
		Match m = makeMatch(GameMode::Teams2v2, 43);
		CHECK(m.players[0].team == 0 && m.players[2].team == 0, "seats 0&2 team A");
		CHECK(m.players[1].team == 1 && m.players[3].team == 1, "seats 1&3 team B");
		m.activePlayer = 0;
		forceHandCard(m, 0, 1);
		CHECK(!canPlayCard(m, 0, 0, 2), "cannot target teammate");
		CHECK(canPlayCard(m, 0, 0, 1), "can target enemy");
		// Team win: knock out both enemies.
		m.players[1].towerHp = 0;
		m.players[3].towerHp = 0;
		evaluateEndState(m);
		CHECK(m.phase == Phase::GameOver, "team match over");
		CHECK(m.winner == 0 || m.winner == 2, "team A representative wins");
		std::printf("OK 2v2 teams\n");
	}

	// --- Hot Potato (#55): crown +1 dmg and transfers ---
	{
		Match m = makeMatch(GameMode::HotPotato, 47);
		CHECK(m.crownSeat >= 0 && m.crownSeat < m.config.playerCount, "crown assigned");
		m.crownSeat = 1;
		const CardDef* volley = findCard(1);
		m.players[1].shieldTurns = 0;
		CHECK(computeAttackDamage(m, 0, 1, *volley) == 4, "crown holder takes +1 (3+1)");
		m.activePlayer = 0;
		forceHandCard(m, 0, 1);
		CHECK(applyCardPlay(m, 0, 1), "attack crown holder");
		CHECK(m.crownSeat == 0, "crown transferred to attacker");
		CHECK(autoFinish(m), "hot potato finishes");
		std::printf("OK hot potato\n");
	}

	// --- Sandbox (#57): towers never die ---
	{
		Match m = makeMatch(GameMode::Sandbox, 53);
		m.players[1].towerHp = 0;
		evaluateEndState(m);
		CHECK(m.phase == Phase::Playing, "sandbox never ends");
		CHECK(!m.players[1].eliminated, "sandbox no elimination");
		CHECK(m.players[1].towerHp == m.players[1].towerMaxHp, "sandbox tower reset");
		std::printf("OK sandbox\n");
	}

	// --- AI tiers (#58): all levels finish matches; hard finds lethal ---
	{
		for (int lvl = 0; lvl <= 2; ++lvl) {
			MatchConfig cfg;
			cfg.playerCount = 4;
			cfg.seed = 6100u + static_cast<uint32_t>(lvl) * 13u;
			cfg.eventsEnabled = true;
			cfg.aiLevel = static_cast<AiLevel>(lvl);
			Match m;
			startMatch(m, cfg);
			char msg[64];
			std::snprintf(msg, sizeof(msg), "AI level %d finishes", lvl);
			CHECK(autoFinish(m, 900), msg);
		}
		// Hard lethal: seat 1 at 2 HP, seat 0 holds a volley -> must kill.
		Match m = makeMatch(GameMode::ClassicFFA, 61);
		m.config.aiLevel = AiLevel::Hard;
		m.activePlayer = 0;
		forceHandCard(m, 0, 1); // 3 dmg volley
		m.players[1].towerHp = 2;
		m.players[1].shieldTurns = 0;
		CHECK(autoPlayBest(m), "hard AI plays");
		CHECK(m.players[1].eliminated, "hard AI takes the lethal");
		std::printf("OK AI tiers\n");
	}

	// --- Deck mods (#47): bans remove, extras add ---
	{
		MatchConfig cfg;
		cfg.mode = GameMode::ClassicFFA;
		cfg.seed = 67;
		cfg.eventsEnabled = false;
		Match m;
		initLobby(m, cfg);
		m.players[0].control = SeatControl::HumanLocal;
		m.players[0].ready = true;
		m.players[0].bannedDefs[0] = 1;  // ban Plastic Volley (3 copies)
		m.players[0].extraDefs[0] = 25;  // add a Medkit Foam
		startMatchFromLobby(m);
		int volleys = 0;
		int medkits = 0;
		auto countIn = [&](const std::vector<CardInstance>& v) {
			for (const CardInstance& c : v) {
				if (c.defId == 1) {
					++volleys;
				}
				if (c.defId == 25) {
					++medkits;
				}
			}
		};
		countIn(m.players[0].deck);
		countIn(m.players[0].hand);
		countIn(m.players[0].discard);
		CHECK(volleys == 0, "banned def removed entirely");
		CHECK(medkits == 2, "sideboard extra added (1 recipe + 1 extra)");
		std::printf("OK deck mods\n");
	}

	// --- Snapshot v5 round-trip preserves v0.7 state ---
	{
		Match m = makeMatch(GameMode::HotPotato, 71);
		m.config.turnTimerSeconds = 45;
		m.config.paradeRest = true;
		m.players[0].firstAttackDone = true;
		m.players[2].skipNextTurn = true;
		const auto bytes = serializeMatch(m);
		Match m2;
		CHECK(deserializeMatch(m2, bytes.data(), bytes.size()), "deserialize v5");
		CHECK(m2.config.mode == GameMode::HotPotato, "mode survives");
		CHECK(m2.config.turnTimerSeconds == 45, "turn timer survives");
		CHECK(m2.config.paradeRest, "parade rest survives");
		CHECK(m2.crownSeat == m.crownSeat, "crown survives");
		CHECK(m2.players[0].firstAttackDone, "sapper flag survives");
		CHECK(m2.players[2].skipNextTurn, "skip flag survives");
		std::printf("OK snapshot v5 (%zu bytes)\n", bytes.size());
	}

	// --- v1.1 trust lines (#104/#105): commit at start, reveal at game over ---
	{
		Match m = makeMatch(GameMode::ClassicFFA, 313);
		bool sawCommit = false;
		for (const MatchEvent& e : m.log) {
			if (e.text.find("Trust: seed commit") != std::string::npos) {
				sawCommit = true;
				char expect[16];
				std::snprintf(expect, sizeof(expect), "%08x", seedCommitHash(m.config.seed));
				CHECK(e.text.find(expect) != std::string::npos, "commit line carries hash(seed)");
			}
		}
		CHECK(sawCommit, "seed commit logged at match start");
		CHECK(autoFinish(m), "trust match finishes");
		bool sawReveal = false;
		for (const MatchEvent& e : m.log) {
			if (e.text.find("Trust: seed was") != std::string::npos) {
				sawReveal = true;
			}
		}
		CHECK(sawReveal, "seed revealed at game over");
		// Deck fingerprint is order-independent (decks are shuffled).
		std::vector<CardInstance> a = { { 1, 1 }, { 5, 2 }, { 20, 3 } };
		std::vector<CardInstance> b = { { 20, 9 }, { 1, 8 }, { 5, 7 } };
		CHECK(deckCheckHash(a) == deckCheckHash(b), "deck hash ignores order/instance ids");
		std::vector<CardInstance> c = { { 1, 1 }, { 5, 2 }, { 21, 3 } };
		CHECK(deckCheckHash(a) != deckCheckHash(c), "deck hash detects content change");
		std::printf("OK trust lines\n");
	}

	// --- Lobby targeting toggle (#70) ---
	{
		Match m = makeMatch(GameMode::ClassicFFA, 73);
		m.activePlayer = 0;
		forceHandCard(m, 0, 1);
		CHECK(canPlayCard(m, 0, 0, 2), "free targeting reaches opposite");
		m.config.freeTargeting = false;
		CHECK(!canPlayCard(m, 0, 0, 2), "adjacent-only blocks opposite");
		CHECK(canPlayCard(m, 0, 0, 1), "adjacent-only allows neighbor");
		std::printf("OK targeting toggle\n");
	}

	// --- All modes finish under autoplay (stability sweep) ---
	{
		const GameMode modes[] = { GameMode::ClassicFFA, GameMode::QuickDuel, GameMode::Teams2v2,
								   GameMode::HotPotato };
		for (GameMode mode : modes) {
			MatchConfig cfg;
			cfg.mode = mode;
			cfg.seed = 8000u + static_cast<uint32_t>(mode) * 31u;
			cfg.eventsEnabled = true;
			Match m;
			startMatch(m, cfg);
			char msg[64];
			std::snprintf(msg, sizeof(msg), "%s finishes", gameModeName(mode));
			CHECK(autoFinish(m, 900), msg);
		}
		std::printf("OK mode sweep\n");
	}

	if (g_fails) {
		std::printf("%d failure(s)\n", g_fails);
		return 1;
	}
	std::printf("OK v0.7 mode tests\n");
	return 0;
}
