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

} // namespace

bool isValidTarget(const Match& match, int actor, int target, const CardDef& card)
{
	if (card.klass == CardClass::Defense || card.klass == CardClass::Tactic) {
		return target == actor || target < 0;
	}
	if (card.klass != CardClass::Attack) {
		return false;
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
	const bool adjacentOnly = !match.config.freeTargeting || eventForcesAdjacent(match);
	if (adjacentOnly) {
		// Adjacent on a 4-seat table: (0-1), (1-2), (2-3), (3-0)
		const int diff = std::abs(actor - target);
		if (!(diff == 1 || diff == match.config.playerCount - 1)) {
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
	const int tgt = (def->klass == CardClass::Attack) ? targetPlayer : playerIndex;
	return isValidTarget(match, playerIndex, tgt, *def);
}

int computeAttackDamage(const Match& match, int actor, int target, const CardDef& card)
{
	int dmg = card.power;
	// March Fire: two pellets (2+2) before other modifiers.
	if (card.id == 4) {
		dmg = card.power * 2;
	}
	if (match.players[static_cast<size_t>(actor)].tower == TowerType::Sniper) {
		dmg += 2; // M5: glass cannon payoff
	}
	// Ambush Order: +2 if target unshielded.
	if (card.id == 24 && match.players[static_cast<size_t>(target)].shieldTurns <= 0) {
		dmg += 2;
	}
	if (match.players[static_cast<size_t>(actor)].attackBonusNext > 0) {
		dmg += match.players[static_cast<size_t>(actor)].attackBonusNext;
	}
	// Shield after bonuses (halve, round up).
	if (match.players[static_cast<size_t>(target)].shieldTurns > 0) {
		dmg = (dmg + 1) / 2;
	}
	dmg = eventModifyAttackDamage(match, dmg);
	return std::max(0, dmg);
}

void drawCards(Match& match, int playerIndex, int count)
{
	Player& p = match.players[static_cast<size_t>(playerIndex)];
	for (int i = 0; i < count; ++i) {
		if (static_cast<int>(p.hand.size()) >= kMaxHandSize) {
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
		const int dmg = computeAttackDamage(match, actor, targetPlayer, *def);
		Player& t = match.players[static_cast<size_t>(targetPlayer)];
		t.towerHp = std::max(0, t.towerHp - dmg);
		std::snprintf(buf, sizeof(buf), "%s takes %d damage (HP %d)", t.name, dmg, t.towerHp);
		pushEvent(match, MatchEvent::Type::Damage, actor, targetPlayer, dmg, buf);
		// Spend scout / buff after one attack.
		p.attackBonusNext = 0;
		match.pendingPhysicsImpulse.targetPlayer = targetPlayer;
		match.pendingPhysicsImpulse.strength = 1.5f + 0.25f * static_cast<float>(dmg);
		match.pendingPhysicsImpulse.frames = 1;
		break;
	}
	case CardClass::Defense: {
		p.shieldTurns += def->power;
		std::snprintf(buf, sizeof(buf), "%s gains shield (%d turns)", p.name, p.shieldTurns);
		pushEvent(match, MatchEvent::Type::Shield, actor, actor, p.shieldTurns, buf);
		if (def->id == 12) {
			drawCards(match, actor, 1);
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

	const int next = nextLivingPlayer(match, match.activePlayer);
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
	}
}

} // namespace toy
