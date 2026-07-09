#include "game/match.h"

#include "game/cards.h"
#include "game/cosmetics.h"
#include "game/events.h"
#include "game/rules.h"

#include <cstdio>

namespace toy {

namespace {

const char* kDefaultSeatNames[kMaxPlayers] = { "Green", "Blue", "Red", "Yellow" };

void pushInfo(Match& match, const char* text)
{
	MatchEvent e;
	e.type = MatchEvent::Type::Info;
	e.text = text ? text : "";
	match.log.push_back(std::move(e));
	if (match.log.size() > 80) {
		match.log.erase(match.log.begin(), match.log.begin() + static_cast<std::ptrdiff_t>(match.log.size() - 80));
	}
}

void dealSoldiers(Match& match, Player& p)
{
	p.soldiers.clear();
	for (int s = 0; s < kSoldiersPerPlayer; ++s) {
		Soldier sol;
		sol.id = match.nextSoldierId++;
		sol.owner = p.id;
		sol.alive = true;
		sol.bodyIndex = -1;
		p.soldiers.push_back(sol);
	}
}

void applyTowerStats(Player& p)
{
	// M5: MG tank vs Sniper (+2 dmg) — close HP so snipers aren't free focus.
	p.towerMaxHp = (p.tower == TowerType::MachineGun) ? 36 : 34;
	p.towerHp = p.towerMaxHp;
	p.shieldTurns = 0;
	p.attackBonusNext = 0;
	p.eliminated = false;
	p.hand.clear();
	p.discard.clear();
}

} // namespace

void bumpSync(Match& match)
{
	++match.syncGeneration;
}

void initLobby(Match& match, const MatchConfig& config)
{
	match = Match{};
	match.config = config;
	if (match.config.playerCount < 2) {
		match.config.playerCount = 2;
	}
	if (match.config.playerCount > kMaxPlayers) {
		match.config.playerCount = kMaxPlayers;
	}
	match.phase = Phase::Lobby;
	match.rng = config.seed ? config.seed : 1u;
	resetWorldEvents(match);

	for (int i = 0; i < kMaxPlayers; ++i) {
		Player& p = match.players[static_cast<size_t>(i)];
		p = Player{};
		p.id = i;
		p.setName(kDefaultSeatNames[i]);
		p.control = SeatControl::Empty;
		p.ready = false;
		p.tower = (i % 2 == 0) ? TowerType::MachineGun : TowerType::Sniper;
		p.cosmetics = defaultCosmeticsForSeat(i);
		applyTowerStats(p);
	}
	char buf[128];
	std::snprintf(buf, sizeof(buf), "Lobby open — map: %s. Host seat 0; clients join.",
				  mapName(match.config.mapId));
	pushInfo(match, buf);
	bumpSync(match);
}

void startMatchFromLobby(Match& match)
{
	// Fill empties with AI if requested.
	int humans = 0;
	for (int i = 0; i < match.config.playerCount; ++i) {
		Player& p = match.players[static_cast<size_t>(i)];
		if (p.control == SeatControl::Empty) {
			if (match.config.fillEmptyWithAI) {
				p.control = SeatControl::AI;
				char n[kPlayerNameLen];
				std::snprintf(n, sizeof(n), "AI-%s", kDefaultSeatNames[i]);
				p.setName(n);
				p.ready = true;
				applyAiCosmetics(p, match.config.seed);
			}
		}
		if (p.control == SeatControl::HumanLocal || p.control == SeatControl::HumanRemote) {
			++humans;
		}
	}
	if (humans < 1) {
		pushInfo(match, "Cannot start: no human players.");
		bumpSync(match);
		return;
	}

	match.rng = match.config.seed ? match.config.seed : 1u;
	match.nextInstanceId = 1;
	match.nextSoldierId = 1;
	match.activePlayer = 0;
	match.turnNumber = 1;
	match.winner = -1;
	match.phase = Phase::Playing;
	++match.matchId;
	match.pendingPhysicsImpulse = {};
	resetWorldEvents(match);

	for (int i = 0; i < match.config.playerCount; ++i) {
		Player& p = match.players[static_cast<size_t>(i)];
		if (p.control == SeatControl::Empty) {
			// Seat unused beyond playerCount — leave empty.
			continue;
		}
		applyTowerStats(p);
		p.deck = makeStarterDeck(i, match.rng, match.nextInstanceId);
		dealSoldiers(match, p);
		// drawCards needs Playing phase + valid player — call after phase set
	}

	// Draw opening hands
	for (int i = 0; i < match.config.playerCount; ++i) {
		Player& p = match.players[static_cast<size_t>(i)];
		if (p.control == SeatControl::Empty) {
			continue;
		}
		drawCards(match, i, kStartingHandSize);
	}

	char buf[128];
	std::snprintf(buf, sizeof(buf), "Match #%u on %s — %s to play.", match.matchId,
				  mapName(match.config.mapId), match.players[0].name);
	MatchEvent e;
	e.type = MatchEvent::Type::TurnStart;
	e.actor = 0;
	e.value = 1;
	e.text = buf;
	match.log.push_back(std::move(e));
	bumpSync(match);
}

void startMatch(Match& match, const MatchConfig& config)
{
	initLobby(match, config);
	// Offline: all seats local hotseat humans for first playerCount, rest AI if fill enabled.
	for (int i = 0; i < match.config.playerCount; ++i) {
		match.players[static_cast<size_t>(i)].control = SeatControl::HumanLocal;
		match.players[static_cast<size_t>(i)].ready = true;
		match.players[static_cast<size_t>(i)].setName(kDefaultSeatNames[i]);
	}
	startMatchFromLobby(match);
}

void resetMatch(Match& match)
{
	const MatchConfig cfg = match.config;
	// Preserve seat controls if we were in multiplayer mid-game — offline path restarts hotseat.
	const bool wasNet = (match.players[0].control == SeatControl::HumanLocal &&
						 (match.players[1].control == SeatControl::HumanRemote ||
						  match.players[1].control == SeatControl::AI));
	if (wasNet && match.phase != Phase::Lobby) {
		// Host rematch: keep seats, re-deal.
		startMatchFromLobby(match);
		return;
	}
	startMatch(match, cfg);
}

bool autoPlayBest(Match& match)
{
	if (match.phase != Phase::Playing) {
		return false;
	}
	const int actor = match.activePlayer;
	Player& p = match.players[static_cast<size_t>(actor)];

	int bestHand = -1;
	int bestTarget = -1;
	int bestScore = -9999;

	for (int h = 0; h < static_cast<int>(p.hand.size()); ++h) {
		const CardDef* def = findCard(p.hand[static_cast<size_t>(h)].defId);
		if (!def) {
			continue;
		}
		if (def->klass == CardClass::Attack) {
			for (int t = 0; t < match.config.playerCount; ++t) {
				if (!canPlayCard(match, actor, h, t)) {
					continue;
				}
				const int dmg = computeAttackDamage(match, actor, t, *def);
				const Player& tp = match.players[static_cast<size_t>(t)];
				// M5: diversify focus — soft HP weight + noise (no tower-type bias).
				int score = dmg * 6 - tp.towerHp / 3 + randRange(match.rng, 0, 12);
				if (tp.shieldTurns > 0) {
					score -= 5; // prefer unshielded
				}
				// Finish low targets without hard-locking one seat forever.
				if (tp.towerHp <= 8) {
					score += 6;
				}
				if (score > bestScore) {
					bestScore = score;
					bestHand = h;
					bestTarget = t;
				}
			}
		} else if (canPlayCard(match, actor, h, actor)) {
			// Prefer defense when low, tactics when hand small.
			int score = (def->klass == CardClass::Defense) ? 6 : 4;
			if (p.towerHp * 2 < p.towerMaxHp) {
				score += (def->klass == CardClass::Defense) ? 8 : 0;
			}
			if (static_cast<int>(p.hand.size()) <= 2 && def->klass == CardClass::Tactic) {
				score += 5;
			}
			score += randRange(match.rng, 0, 4);
			if (score > bestScore) {
				bestScore = score;
				bestHand = h;
				bestTarget = actor;
			}
		}
	}

	if (bestHand < 0) {
		endTurn(match);
		bumpSync(match);
		return true;
	}
	const bool ok = applyCardPlay(match, bestHand, bestTarget);
	if (ok) {
		bumpSync(match);
	}
	return ok;
}

bool autoPlayIfAITurn(Match& match)
{
	if (match.phase != Phase::Playing) {
		return false;
	}
	const Player& p = match.players[static_cast<size_t>(match.activePlayer)];
	if (p.control != SeatControl::AI || p.eliminated) {
		return false;
	}
	return autoPlayBest(match);
}

} // namespace toy
