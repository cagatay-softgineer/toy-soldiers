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

void applyTowerStats(Player& p, GameMode mode)
{
	// v0.7: per-tower base stats; Quick Duel flattens HP to 24 (#53).
	p.towerMaxHp = (mode == GameMode::QuickDuel) ? 24 : towerBaseHp(p.tower);
	p.towerHp = p.towerMaxHp;
	// Shield Bearer (#37) opens with one shield turn.
	p.shieldTurns = (p.tower == TowerType::ShieldBearer) ? 1 : 0;
	p.attackBonusNext = 0;
	p.firstAttackDone = false;
	p.skipNextTurn = false;
	p.eliminated = false;
	p.hand.clear();
	p.discard.clear();
}

int pickOccupiedSeat(Match& match)
{
	int seats[kMaxPlayers];
	int n = 0;
	for (int i = 0; i < match.config.playerCount; ++i) {
		if (match.players[static_cast<size_t>(i)].control != SeatControl::Empty) {
			seats[n++] = i;
		}
	}
	if (n <= 0) {
		return -1;
	}
	return seats[randRange(match.rng, 0, n - 1)];
}

} // namespace

void bumpSync(Match& match)
{
	++match.syncGeneration;
}

void applyModeToConfig(MatchConfig& config)
{
	switch (config.mode) {
	case GameMode::QuickDuel:
		config.playerCount = 2;
		break;
	case GameMode::Teams2v2:
	case GameMode::HotPotato:
		config.playerCount = 4;
		break;
	default:
		if (config.playerCount < 2 || config.playerCount > kMaxPlayers) {
			config.playerCount = 4;
		}
		break;
	}
}

void initLobby(Match& match, const MatchConfig& config)
{
	match = Match{};
	match.config = config;
	applyModeToConfig(match.config);
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
		applyTowerStats(p, match.config.mode);
	}
	char buf[128];
	std::snprintf(buf, sizeof(buf), "Lobby open — %s on %s. Host seat 0; clients join.",
				  gameModeName(match.config.mode), mapName(match.config.mapId));
	pushInfo(match, buf);
	bumpSync(match);
}

void startMatchFromLobby(Match& match)
{
	applyModeToConfig(match.config);

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
	match.crownSeat = -1;
	resetWorldEvents(match);

	for (int i = 0; i < match.config.playerCount; ++i) {
		Player& p = match.players[static_cast<size_t>(i)];
		if (p.control == SeatControl::Empty) {
			// Seat unused beyond playerCount — leave empty.
			continue;
		}
		applyTowerStats(p, match.config.mode);
		// Teams2v2 (#54): 0&2 vs 1&3.
		p.team = (match.config.mode == GameMode::Teams2v2) ? (i % 2) : -1;
		// AI personalities (#59) — deterministic from match rng.
		if (p.control == SeatControl::AI) {
			p.persona = static_cast<AiPersona>(randRange(match.rng, 1, 3)); // Aggro/Turtle/Chaos
		} else {
			p.persona = AiPersona::Balanced;
		}
		p.deck = makeStarterDeck(i, match.rng, match.nextInstanceId, match.config.mode, p);
		dealSoldiers(match, p);
		// drawCards needs Playing phase + valid player — call after phase set
	}

	// v1.1 trust lines (#104/#105): deck fingerprints + seed commitment, visible to every
	// client via the snapshot log. The seed itself is revealed at game over.
	{
		char tbuf[160];
		int off = std::snprintf(tbuf, sizeof(tbuf), "Trust: seed commit %08x — deck check", seedCommitHash(match.config.seed));
		for (int i = 0; i < match.config.playerCount && off > 0 && off < static_cast<int>(sizeof(tbuf)) - 12; ++i) {
			const Player& p = match.players[static_cast<size_t>(i)];
			if (p.control == SeatControl::Empty) {
				continue;
			}
			off += std::snprintf(tbuf + off, sizeof(tbuf) - static_cast<size_t>(off), " %d:%04x", i,
								 deckCheckHash(p.deck) & 0xFFFF);
		}
		pushInfo(match, tbuf);
	}

	// Hot Potato (#55): crown lands on a random occupied seat.
	if (match.config.mode == GameMode::HotPotato) {
		match.crownSeat = pickOccupiedSeat(match);
		if (match.crownSeat >= 0) {
			char cbuf[96];
			std::snprintf(cbuf, sizeof(cbuf), "The crown lands on %s — they take +1 damage!",
						  match.players[static_cast<size_t>(match.crownSeat)].name);
			pushInfo(match, cbuf);
		}
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
	std::snprintf(buf, sizeof(buf), "Match #%u — %s on %s — %s to play.", match.matchId,
				  gameModeName(match.config.mode), mapName(match.config.mapId), match.players[0].name);
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

namespace {

struct AiCandidate {
	int hand = -1;
	int target = -1;
	int score = -9999;
	CardClass klass = CardClass::Tactic;
	int estDamage = 0;
};

// Persona weight shifts (#59).
int personaAdjust(AiPersona persona, CardClass klass, uint32_t& rng)
{
	int adj = 0;
	switch (persona) {
	case AiPersona::Aggro:
		adj += (klass == CardClass::Attack) ? 4 : -2;
		break;
	case AiPersona::Turtle:
		adj += (klass == CardClass::Defense) ? 6 : (klass == CardClass::Attack ? -2 : 0);
		break;
	case AiPersona::Chaos:
		adj += randRange(rng, 0, 14);
		break;
	default:
		break;
	}
	return adj;
}

// Collect every legal play for the active seat.
void collectCandidates(Match& match, std::vector<AiCandidate>& out)
{
	const int actor = match.activePlayer;
	const Player& p = match.players[static_cast<size_t>(actor)];
	for (int h = 0; h < static_cast<int>(p.hand.size()); ++h) {
		const CardDef* def = findCard(p.hand[static_cast<size_t>(h)].defId);
		if (!def) {
			continue;
		}
		if (def->klass == CardClass::Attack && !(def->keywords & KwAOE)) {
			for (int t = 0; t < match.config.playerCount; ++t) {
				if (canPlayCard(match, actor, h, t)) {
					AiCandidate c;
					c.hand = h;
					c.target = t;
					c.klass = def->klass;
					c.estDamage = computeAttackDamage(match, actor, t, *def);
					out.push_back(c);
				}
			}
		} else if (canPlayCard(match, actor, h, actor)) {
			AiCandidate c;
			c.hand = h;
			c.target = actor;
			c.klass = def->klass;
			if (def->keywords & KwAOE) {
				const int n = match.config.playerCount;
				const int nb[2] = { (actor + 1) % n, (actor + n - 1) % n };
				for (int k = 0; k < 2; ++k) {
					const int t = nb[k];
					if ((k == 1 && t == nb[0]) || t == actor) {
						continue;
					}
					const Player& tp = match.players[static_cast<size_t>(t)];
					if (tp.eliminated || tp.control == SeatControl::Empty || sameTeam(match, actor, t)) {
						continue;
					}
					c.estDamage += computeAttackDamage(match, actor, t, *def);
				}
			}
			out.push_back(c);
		}
	}
}

// AI flavor lines (#60) — only for AI seats, kept sparse.
void aiFlavorLine(Match& match, int actor, CardClass klass)
{
	if (match.players[static_cast<size_t>(actor)].control != SeatControl::AI) {
		return;
	}
	const char* name = match.players[static_cast<size_t>(actor)].name;
	char buf[96];
	if (klass == CardClass::Defense) {
		std::snprintf(buf, sizeof(buf), "%s digs in.", name);
	} else if (klass == CardClass::Event) {
		std::snprintf(buf, sizeof(buf), "%s stirs up the table!", name);
	} else {
		return;
	}
	MatchEvent e;
	e.type = MatchEvent::Type::Info;
	e.actor = actor;
	e.text = buf;
	match.log.push_back(std::move(e));
}

int scoreCandidate(Match& match, const AiCandidate& c, AiLevel level, AiPersona persona)
{
	const int actor = match.activePlayer;
	const Player& p = match.players[static_cast<size_t>(actor)];
	const CardDef* def = findCard(p.hand[static_cast<size_t>(c.hand)].defId);
	if (!def) {
		return -9999;
	}
	int score = 0;
	if (def->klass == CardClass::Attack) {
		if (def->id == 26) {
			// Magnet Hand: only worth it against a shield.
			const Player& tp = match.players[static_cast<size_t>(c.target)];
			score = (tp.shieldTurns > 0) ? 12 : -20;
		} else if (def->keywords & KwAOE) {
			score = c.estDamage * 6 + randRange(match.rng, 0, 8);
		} else {
			const Player& tp = match.players[static_cast<size_t>(c.target)];
			score = c.estDamage * 6 - tp.towerHp / 3 + randRange(match.rng, 0, level == AiLevel::Hard ? 4 : 12);
			if (tp.shieldTurns > 0 && !(def->keywords & KwPierce)) {
				score -= 5; // prefer unshielded
			}
			if (tp.towerHp <= 8) {
				score += 6;
			}
			// Hard (#58): lookahead-1 — a lethal hit dominates everything.
			if (level == AiLevel::Hard && c.estDamage >= tp.towerHp && match.config.mode != GameMode::Sandbox) {
				score += 1000;
			}
		}
	} else if (def->klass == CardClass::Event) {
		score = 3 + randRange(match.rng, 0, 6);
	} else if (def->id == 27) {
		// Line Cutter: worth more the healthier the next player is.
		score = 5 + randRange(match.rng, 0, 4);
	} else {
		// Defense / tactics.
		score = (def->klass == CardClass::Defense) ? 6 : 4;
		if (p.towerHp * 2 < p.towerMaxHp) {
			score += (def->klass == CardClass::Defense) ? 8 : 0;
		}
		if (static_cast<int>(p.hand.size()) <= 2 && def->klass == CardClass::Tactic) {
			score += 5;
		}
		if (level == AiLevel::Hard && (def->keywords & KwHeal) && p.towerHp >= p.towerMaxHp - 1) {
			score -= 10; // don't heal at full
		}
		score += randRange(match.rng, 0, 4);
	}
	score += personaAdjust(persona, def->klass, match.rng);
	return score;
}

} // namespace

bool autoPlayBest(Match& match)
{
	if (match.phase != Phase::Playing) {
		return false;
	}
	const int actor = match.activePlayer;
	const Player& p = match.players[static_cast<size_t>(actor)];
	const AiLevel level = match.config.aiLevel;
	const AiPersona persona = p.persona;

	std::vector<AiCandidate> candidates;
	candidates.reserve(24);
	collectCandidates(match, candidates);

	if (candidates.empty()) {
		// #61: never softlock — always end the turn.
		endTurn(match);
		bumpSync(match);
		return true;
	}

	int pick = -1;
	if (level == AiLevel::Easy) {
		// #58 Easy: any random legal play.
		pick = randRange(match.rng, 0, static_cast<int>(candidates.size()) - 1);
	} else {
		int bestScore = -9999;
		for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
			const int s = scoreCandidate(match, candidates[static_cast<size_t>(i)], level, persona);
			if (s > bestScore) {
				bestScore = s;
				pick = i;
			}
		}
	}
	if (pick < 0) {
		endTurn(match);
		bumpSync(match);
		return true;
	}

	const AiCandidate chosen = candidates[static_cast<size_t>(pick)];
	const CardClass klass = chosen.klass;
	const bool ok = applyCardPlay(match, chosen.hand, chosen.target);
	if (ok) {
		aiFlavorLine(match, actor, klass);
		bumpSync(match);
		return true;
	}
	// Defensive fallback (#61): a rejected AI play must not stall the match.
	endTurn(match);
	bumpSync(match);
	return true;
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
