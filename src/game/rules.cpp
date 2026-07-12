#include "game/rules.h"

#include "game/cards.h"
#include "game/events.h"
#include "game/match.h"

#include <algorithm>
#include <cstdio>

namespace toy {

namespace {

void pushEvent(Match& m, MatchEvent::Type type, int actor, int target, int value, const char* text)
{
	MatchEvent e;
	e.type = type;
	e.actor = actor;
	e.target = target;
	e.value = value;
	e.text = text ? text : "";
	m.log.push_back(std::move(e));
	if (m.log.size() > 80) {
		m.log.erase(m.log.begin(), m.log.begin() + static_cast<std::ptrdiff_t>(m.log.size() - 80));
	}
}

int nextLivingPlayer(const Match& m, int from)
{
	const int n = m.config.playerCount;
	for (int step = 1; step <= n; ++step) {
		const int i = (from + step) % n;
		if (!m.players[static_cast<size_t>(i)].eliminated) {
			return i;
		}
	}
	return from;
}

int livingCount(const Match& m)
{
	int c = 0;
	for (int i = 0; i < m.config.playerCount; ++i) {
		if (!m.players[static_cast<size_t>(i)].eliminated) {
			++c;
		}
	}
	return c;
}

bool isAdjacentSeat(const Match& m, int a, int b)
{
	const int diff = std::abs(a - b);
	return diff == 1 || diff == m.config.playerCount - 1;
}

// Apply one resolved attack hit: damage, log, crown transfer, physics cue.
void dealAttackDamage(Match& match, int actor, int target, const CardDef& card)
{
	const int dmg = computeAttackDamage(match, actor, target, card);
	Player& p = match.players[static_cast<size_t>(actor)];
	Player& t = match.players[static_cast<size_t>(target)];
	t.towerHp = std::max(0, t.towerHp - dmg);
	char buf[160];
	std::snprintf(buf, sizeof(buf), "%s takes %d damage (HP %d)", t.name, dmg, t.towerHp);
	pushEvent(match, MatchEvent::Type::Damage, actor, target, dmg, buf);

	// Hot Potato (#55): hitting the crown holder steals the crown.
	if (match.config.mode == GameMode::HotPotato && match.crownSeat == target && dmg > 0 && actor != target) {
		match.crownSeat = actor;
		std::snprintf(buf, sizeof(buf), "%s snatches the crown from %s!", p.name, t.name);
		pushEvent(match, MatchEvent::Type::Info, actor, target, 1, buf);
	}

	match.pendingPhysicsImpulse.targetPlayer = target;
	match.pendingPhysicsImpulse.strength = 1.5f + 0.25f * static_cast<float>(dmg);
	match.pendingPhysicsImpulse.frames = 1;
}

} // namespace

bool sameTeam(const Match& match, int a, int b)
{
	if (match.config.mode != GameMode::Teams2v2) {
		return false;
	}
	if (a < 0 || b < 0 || a >= match.config.playerCount || b >= match.config.playerCount) {
		return false;
	}
	const int ta = match.players[static_cast<size_t>(a)].team;
	const int tb = match.players[static_cast<size_t>(b)].team;
	return ta >= 0 && ta == tb;
}

bool isValidTarget(const Match& match, int actor, int target, const CardDef& card)
{
	if (card.klass == CardClass::Defense || card.klass == CardClass::Tactic || card.klass == CardClass::Event) {
		return target == actor || target < 0;
	}
	if (card.klass != CardClass::Attack) {
		return false;
	}
	// AOE attacks (#44) resolve against both neighbors — no single target pick.
	if (card.keywords & KwAOE) {
		return target == actor || target < 0;
	}
	if (target < 0 || target >= match.config.playerCount) {
		return false;
	}
	if (match.players[static_cast<size_t>(target)].eliminated) {
		return false;
	}
	if (target == actor) {
		return false;
	}
	// #72: no friendly fire in 2v2.
	if (sameTeam(match, actor, target)) {
		return false;
	}
	const bool adjacentOnly =
		!match.config.freeTargeting || eventForcesAdjacent(match) || (card.keywords & KwAdjacentOnly);
	if (adjacentOnly) {
		// Adjacent on a 4-seat table: (0-1), (1-2), (2-3), (3-0)
		if (!isAdjacentSeat(match, actor, target)) {
			return false;
		}
	}
	return true;
}

bool canPlayCard(const Match& match, int playerIndex, int handIndex, int targetPlayer)
{
	if (match.phase != Phase::Playing) {
		return false;
	}
	if (playerIndex != match.activePlayer) {
		return false;
	}
	const Player& p = match.players[static_cast<size_t>(playerIndex)];
	if (p.eliminated) {
		return false;
	}
	if (handIndex < 0 || handIndex >= static_cast<int>(p.hand.size())) {
		return false;
	}
	const CardInstance& inst = p.hand[static_cast<size_t>(handIndex)];
	const CardDef* def = findCard(inst.defId);
	if (!def) {
		return false;
	}
	const bool needsTarget = (def->klass == CardClass::Attack) && !(def->keywords & KwAOE);
	const int tgt = needsTarget ? targetPlayer : playerIndex;
	return isValidTarget(match, playerIndex, tgt, *def);
}

int computeAttackDamage(const Match& match, int actor, int target, const CardDef& card)
{
	// Magnet Hand (#45) is a utility attack — never deals damage.
	if (card.id == 26) {
		return 0;
	}
	int dmg = card.power;
	// March Fire: two pellets (2+2) before other modifiers.
	if (card.id == 4) {
		dmg = card.power * 2;
	}
	const Player& ap = match.players[static_cast<size_t>(actor)];
	if (ap.tower == TowerType::Sniper) {
		dmg += 2; // M5: glass cannon payoff
	}
	if (ap.tower == TowerType::ShieldBearer && dmg > 1) {
		dmg -= 1; // #37: sturdy but soft-swinging (attacks still chip at least 1)
	}
	if (ap.tower == TowerType::Sapper && !ap.firstAttackDone) {
		dmg += 1; // #39: opener spike
	}
	// Ambush Order: +2 if target unshielded.
	if (card.id == 24 && match.players[static_cast<size_t>(target)].shieldTurns <= 0) {
		dmg += 2;
	}
	if (ap.attackBonusNext > 0) {
		dmg += ap.attackBonusNext;
	}
	// Shield after bonuses (halve, round up) — Pierce ignores it (#42).
	if (match.players[static_cast<size_t>(target)].shieldTurns > 0 && !(card.keywords & KwPierce)) {
		dmg = (dmg + 1) / 2;
	}
	// Hot Potato (#55): the crown holder takes +1 after mitigation.
	if (match.config.mode == GameMode::HotPotato && match.crownSeat == target) {
		dmg += 1;
	}
	dmg = eventModifyAttackDamage(match, dmg);
	return std::max(0, dmg);
}

void drawCards(Match& match, int playerIndex, int count)
{
	Player& p = match.players[static_cast<size_t>(playerIndex)];
	for (int i = 0; i < count; ++i) {
		if (static_cast<int>(p.hand.size()) >= kMaxHandSize) {
			// #48: surface the wasted draw instead of failing silently.
			char buf[96];
			std::snprintf(buf, sizeof(buf), "%s's hand is full (%d) — draw wasted.", p.name, kMaxHandSize);
			pushEvent(match, MatchEvent::Type::Info, playerIndex, -1, 0, buf);
			break;
		}
		if (p.deck.empty()) {
			if (p.discard.empty()) {
				break;
			}
			p.deck.swap(p.discard);
			shuffleInPlace(p.deck, match.rng);
			pushEvent(match, MatchEvent::Type::Info, playerIndex, -1, 0, "Reshuffled discard into deck.");
		}
		p.hand.push_back(p.deck.back());
		p.deck.pop_back();
		pushEvent(match, MatchEvent::Type::Draw, playerIndex, -1, 1, "Drew a card.");
	}
}

bool applyCardPlay(Match& match, int handIndex, int targetPlayer)
{
	const int actor = match.activePlayer;
	if (!canPlayCard(match, actor, handIndex, targetPlayer)) {
		return false;
	}

	Player& p = match.players[static_cast<size_t>(actor)];
	const CardInstance inst = p.hand[static_cast<size_t>(handIndex)];
	const CardDef* def = findCard(inst.defId);
	if (!def) {
		return false;
	}

	p.hand.erase(p.hand.begin() + handIndex);
	p.discard.push_back(inst);

	char buf[160];
	std::snprintf(buf, sizeof(buf), "%s plays %s", p.name, def->name);
	pushEvent(match, MatchEvent::Type::CardPlayed, actor, targetPlayer, def->id, buf);

	switch (def->klass) {
	case CardClass::Attack: {
		if (def->id == 26) {
			// Magnet Hand (#45): steal 1 shield turn, no damage.
			Player& t = match.players[static_cast<size_t>(targetPlayer)];
			if (t.shieldTurns > 0) {
				--t.shieldTurns;
				++p.shieldTurns;
				std::snprintf(buf, sizeof(buf), "%s steals a shield turn from %s!", p.name, t.name);
				pushEvent(match, MatchEvent::Type::Shield, actor, targetPlayer, p.shieldTurns, buf);
			} else {
				std::snprintf(buf, sizeof(buf), "%s finds nothing to steal from %s.", p.name, t.name);
				pushEvent(match, MatchEvent::Type::Info, actor, targetPlayer, 0, buf);
			}
			break;
		}
		if (def->keywords & KwAOE) {
			// Tin Rain (#44): hit both neighbors (dedup for 2-player rings), skip teammates.
			const int n = match.config.playerCount;
			const int neighbors[2] = { (actor + 1) % n, (actor + n - 1) % n };
			for (int k = 0; k < 2; ++k) {
				const int t = neighbors[k];
				if (k == 1 && t == neighbors[0]) {
					break;
				}
				if (t == actor || match.players[static_cast<size_t>(t)].eliminated ||
					match.players[static_cast<size_t>(t)].control == SeatControl::Empty ||
					sameTeam(match, actor, t)) {
					continue;
				}
				dealAttackDamage(match, actor, t, *def);
			}
		} else {
			dealAttackDamage(match, actor, targetPlayer, *def);
		}
		if (def->keywords & KwDraw) {
			drawCards(match, actor, 1); // Slingshot
		}
		// Spend scout / opener buffs after one attack.
		p.attackBonusNext = 0;
		if (p.tower == TowerType::Sapper) {
			p.firstAttackDone = true;
		}
		break;
	}
	case CardClass::Defense: {
		p.shieldTurns += def->power;
		std::snprintf(buf, sizeof(buf), "%s gains shield (%d turns)", p.name, p.shieldTurns);
		pushEvent(match, MatchEvent::Type::Shield, actor, actor, p.shieldTurns, buf);
		if (def->keywords & KwDraw) {
			drawCards(match, actor, 1); // Glue Trap
		}
		if (def->id == 14) {
			p.towerHp = std::min(p.towerMaxHp, p.towerHp + 2); // Cardboard Bunker
		}
		break;
	}
	case CardClass::Tactic: {
		if (def->id == 20) {
			drawCards(match, actor, def->power);
		} else if (def->id == 21) {
			drawCards(match, actor, 1);
			p.towerHp = std::min(p.towerMaxHp, p.towerHp + def->power); // Field Rations heal 3
		} else if (def->id == 22) {
			drawCards(match, actor, 1);
			p.attackBonusNext += 1; // persists until next attack
			pushEvent(match, MatchEvent::Type::Info, actor, -1, 1, "Scout marks the next attack (+1).");
		} else if (def->id == 23) {
			drawCards(match, actor, 1);
		} else if (def->id == 25) {
			p.towerHp = std::min(p.towerMaxHp, p.towerHp + def->power); // Medkit Foam heal 4
			std::snprintf(buf, sizeof(buf), "%s heals to %d HP", p.name, p.towerHp);
			pushEvent(match, MatchEvent::Type::Info, actor, -1, def->power, buf);
		} else if (def->id == 27) {
			// Line Cutter (#46): skip the next player once — capped, no stacking.
			const int victim = nextLivingPlayer(match, actor);
			Player& v = match.players[static_cast<size_t>(victim)];
			if (victim != actor && !v.skipNextTurn) {
				v.skipNextTurn = true;
				std::snprintf(buf, sizeof(buf), "%s cuts the line — %s will skip a turn!", p.name, v.name);
				pushEvent(match, MatchEvent::Type::Info, actor, victim, 1, buf);
			} else {
				std::snprintf(buf, sizeof(buf), "Line Cutter fizzles — %s is already cut.", v.name);
				pushEvent(match, MatchEvent::Type::Info, actor, victim, 0, buf);
			}
		}
		break;
	}
	case CardClass::Event: {
		// #63/#64/#65: player-cast ambient effects through the world-event system.
		if (def->id == 30) {
			std::snprintf(buf, sizeof(buf), "%s calls the cat!", p.name);
			pushEvent(match, MatchEvent::Type::Info, actor, -1, 0, buf);
			forceWorldEvent(match, EventKind::Cat, -1);
		} else if (def->id == 31) {
			std::snprintf(buf, sizeof(buf), "%s throws pocket sand!", p.name);
			pushEvent(match, MatchEvent::Type::Info, actor, -1, 0, buf);
			forceWorldEvent(match, EventKind::Sandstorm, -1);
		}
		break;
	}
	}

	evaluateEndState(match);
	if (match.phase == Phase::Playing) {
		// One card per turn in M1 for readability.
		endTurn(match);
	}
	return true;
}

void endTurn(Match& match)
{
	if (match.phase != Phase::Playing) {
		return;
	}

	Player& cur = match.players[static_cast<size_t>(match.activePlayer)];
	if (cur.shieldTurns > 0) {
		--cur.shieldTurns;
	}
	// attackBonusNext intentionally persists across turns (M5 Scout Peek fix).

	int next = nextLivingPlayer(match, match.activePlayer);
	// Line Cutter (#46): consume skip flags (bounded — flags never stack per player).
	int guard = 0;
	while (match.players[static_cast<size_t>(next)].skipNextTurn && guard < kMaxPlayers) {
		Player& skipped = match.players[static_cast<size_t>(next)];
		skipped.skipNextTurn = false;
		char sbuf[96];
		std::snprintf(sbuf, sizeof(sbuf), "%s's turn is cut! (draws 1 as consolation)", skipped.name);
		pushEvent(match, MatchEvent::Type::Info, next, -1, 0, sbuf);
		drawCards(match, next, 1);
		next = nextLivingPlayer(match, next);
		++guard;
	}
	match.activePlayer = next;
	++match.turnNumber;

	Player& n = match.players[static_cast<size_t>(next)];
	char buf[96];
	std::snprintf(buf, sizeof(buf), "Turn %d — %s", match.turnNumber, n.name);
	pushEvent(match, MatchEvent::Type::TurnStart, next, -1, match.turnNumber, buf);

	drawCards(match, next, 1);

	// M3: map/room events tick after the new turn begins (flood leak, cat, spawns).
	tickWorldEvents(match);
	evaluateEndState(match);
}

void evaluateEndState(Match& match)
{
	// Sandbox (#57): towers pop back up instead of dying; the match never ends.
	if (match.config.mode == GameMode::Sandbox) {
		for (int i = 0; i < match.config.playerCount; ++i) {
			Player& p = match.players[static_cast<size_t>(i)];
			if (!p.eliminated && p.towerHp <= 0) {
				p.towerHp = p.towerMaxHp;
				char buf[96];
				std::snprintf(buf, sizeof(buf), "Sandbox: %s's tower pops back up.", p.name);
				pushEvent(match, MatchEvent::Type::Info, i, -1, 0, buf);
			}
		}
		return;
	}

	for (int i = 0; i < match.config.playerCount; ++i) {
		Player& p = match.players[static_cast<size_t>(i)];
		if (!p.eliminated && p.towerHp <= 0) {
			p.eliminated = true;
			p.towerHp = 0;
			char buf[96];
			std::snprintf(buf, sizeof(buf), "%s tower destroyed!", p.name);
			pushEvent(match, MatchEvent::Type::PlayerEliminated, i, -1, 0, buf);
		}
	}

	// Teams2v2 (#54): the match ends when only one team has towers left.
	if (match.config.mode == GameMode::Teams2v2) {
		bool teamAlive[2] = { false, false };
		int firstLiving[2] = { -1, -1 };
		for (int i = 0; i < match.config.playerCount; ++i) {
			const Player& p = match.players[static_cast<size_t>(i)];
			if (!p.eliminated && p.team >= 0 && p.team < 2) {
				teamAlive[p.team] = true;
				if (firstLiving[p.team] < 0) {
					firstLiving[p.team] = i;
				}
			}
		}
		if (!teamAlive[0] || !teamAlive[1]) {
			match.phase = Phase::GameOver;
			const int winTeam = teamAlive[0] ? 0 : (teamAlive[1] ? 1 : -1);
			match.winner = winTeam >= 0 ? firstLiving[winTeam] : -1;
			char buf[96];
			if (winTeam >= 0) {
				std::snprintf(buf, sizeof(buf), "Team %s wins!", winTeam == 0 ? "Alpha (seats 0&2)" : "Bravo (seats 1&3)");
				pushEvent(match, MatchEvent::Type::Winner, match.winner, -1, winTeam, buf);
			} else {
				pushEvent(match, MatchEvent::Type::Winner, -1, -1, 0, "Draw — both teams down.");
			}
			// v1.1 #105: seed reveal (see the FFA branch below for the commit line).
			char rbuf[96];
			std::snprintf(rbuf, sizeof(rbuf), "Trust: seed was %u (commit %08x).", match.config.seed,
						  seedCommitHash(match.config.seed));
			pushEvent(match, MatchEvent::Type::Info, -1, -1, 0, rbuf);
		}
		return;
	}

	if (livingCount(match) <= 1) {
		match.phase = Phase::GameOver;
		match.winner = -1;
		for (int i = 0; i < match.config.playerCount; ++i) {
			if (!match.players[static_cast<size_t>(i)].eliminated) {
				match.winner = i;
				break;
			}
		}
		if (match.winner >= 0) {
			char buf[96];
			std::snprintf(buf, sizeof(buf), "%s wins!", match.players[static_cast<size_t>(match.winner)].name);
			pushEvent(match, MatchEvent::Type::Winner, match.winner, -1, 0, buf);
		} else {
			pushEvent(match, MatchEvent::Type::Winner, -1, -1, 0, "Draw — all towers down.");
		}
		// v1.1 #105: reveal the seed so clients can verify the match-start commitment.
		char rbuf[96];
		std::snprintf(rbuf, sizeof(rbuf), "Trust: seed was %u (commit %08x).", match.config.seed,
					  seedCommitHash(match.config.seed));
		pushEvent(match, MatchEvent::Type::Info, -1, -1, 0, rbuf);
	}
}

} // namespace toy
