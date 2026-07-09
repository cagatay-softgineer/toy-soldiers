#include "game/events.h"

#include "game/cards.h"
#include "game/match.h"

#include <algorithm>
#include <cstdio>

namespace toy {

namespace {

void pushWorld(Match& m, int focus, int value, const char* text)
{
	MatchEvent e;
	e.type = MatchEvent::Type::WorldEvent;
	e.actor = -1;
	e.target = focus;
	e.value = value;
	e.text = text ? text : "";
	m.log.push_back(std::move(e));
	if (m.log.size() > 80) {
		m.log.erase(m.log.begin(), m.log.begin() + static_cast<std::ptrdiff_t>(m.log.size() - 80));
	}
}

int livingSeats(const Match& m, int* outSeats, int cap)
{
	int n = 0;
	for (int i = 0; i < m.config.playerCount && n < cap; ++i) {
		if (!m.players[static_cast<size_t>(i)].eliminated &&
			m.players[static_cast<size_t>(i)].control != SeatControl::Empty) {
			outSeats[n++] = i;
		}
	}
	return n;
}

int pickLivingSeat(Match& m)
{
	int seats[kMaxPlayers];
	const int n = livingSeats(m, seats, kMaxPlayers);
	if (n <= 0) {
		return -1;
	}
	return seats[randRange(m.rng, 0, n - 1)];
}

// Weights per map for random rolls (relative ints).
void eventWeights(MapId map, int& sand, int& rain, int& flood, int& cat)
{
	sand = rain = flood = cat = 0;
	switch (map) {
	case MapId::LivingRoom:
		cat = 50;
		rain = 25;
		flood = 5;
		sand = 5;
		break;
	case MapId::Desert:
		sand = 55;
		flood = 20;
		rain = 5;
		cat = 5;
		break;
	case MapId::Backyard:
		rain = 45;
		flood = 30;
		cat = 15;
		sand = 5;
		break;
	default:
		break;
	}
}

EventKind rollEventKind(Match& m)
{
	int sand = 0, rain = 0, flood = 0, cat = 0;
	eventWeights(m.config.mapId, sand, rain, flood, cat);
	const int total = sand + rain + flood + cat;
	if (total <= 0) {
		return EventKind::None;
	}
	int r = randRange(m.rng, 0, total - 1);
	if ((r -= sand) < 0) {
		return EventKind::Sandstorm;
	}
	if ((r -= rain) < 0) {
		return EventKind::Rain;
	}
	if ((r -= flood) < 0) {
		return EventKind::Flood;
	}
	return EventKind::Cat;
}

void beginEvent(Match& m, EventKind kind, int focus)
{
	m.world.kind = kind;
	m.world.focusSeat = focus;
	m.world.warning = false;
	m.eventCooldown = 4; // M5: slightly longer cooldown between events

	char buf[160];
	switch (kind) {
	case EventKind::Sandstorm:
		m.world.remainingTurns = 1;
		std::snprintf(buf, sizeof(buf),
					  "WORLD: Sandstorm! Attacks are adjacent-only for %d turn(s).", m.world.remainingTurns);
		pushWorld(m, -1, static_cast<int>(kind), buf);
		break;
	case EventKind::Rain:
		m.world.remainingTurns = 2;
		std::snprintf(buf, sizeof(buf), "WORLD: Rain soaks the table — attack damage -1 for %d turns.",
					  m.world.remainingTurns);
		pushWorld(m, -1, static_cast<int>(kind), buf);
		break;
	case EventKind::Flood:
		m.world.remainingTurns = 2;
		if (focus < 0) {
			focus = pickLivingSeat(m);
			m.world.focusSeat = focus;
		}
		if (focus >= 0) {
			std::snprintf(buf, sizeof(buf),
						  "WORLD: Flood around %s! They leak 1 HP at the start of their turn (%d turns).",
						  m.players[static_cast<size_t>(focus)].name, m.world.remainingTurns);
		} else {
			std::snprintf(buf, sizeof(buf), "WORLD: Flood fizzles (no seats).");
			m.world.kind = EventKind::None;
			m.world.remainingTurns = 0;
		}
		pushWorld(m, focus, static_cast<int>(kind), buf);
		// Physics splash cue
		if (focus >= 0) {
			m.pendingPhysicsImpulse.targetPlayer = focus;
			m.pendingPhysicsImpulse.strength = 2.2f;
			m.pendingPhysicsImpulse.frames = 1;
		}
		break;
	case EventKind::Cat:
		// Warning first — resolve on next tick.
		m.world.remainingTurns = 2;
		m.world.warning = true;
		if (focus < 0) {
			focus = pickLivingSeat(m);
			m.world.focusSeat = focus;
		}
		std::snprintf(buf, sizeof(buf), "WORLD: A cat approaches the table… (watch %s)",
					  focus >= 0 ? m.players[static_cast<size_t>(focus)].name : "someone");
		pushWorld(m, focus, static_cast<int>(kind), buf);
		break;
	default:
		m.world = {};
		break;
	}
}

void clearEvent(Match& m, const char* reason)
{
	if (m.world.kind != EventKind::None) {
		char buf[120];
		std::snprintf(buf, sizeof(buf), "WORLD: %s ends.%s", eventKindName(m.world.kind), reason ? reason : "");
		pushWorld(m, m.world.focusSeat, 0, buf);
	}
	m.world = {};
}

void resolveCatPounce(Match& m)
{
	const int focus = m.world.focusSeat >= 0 ? m.world.focusSeat : pickLivingSeat(m);
	m.world.focusSeat = focus;
	m.world.warning = false;
	m.world.remainingTurns = 1; // linger one turn as "cat on table" flavor after pounce
	char buf[140];
	if (focus >= 0) {
		std::snprintf(buf, sizeof(buf), "WORLD: Cat pounces near %s! Soldiers scatter (no tower damage).",
					  m.players[static_cast<size_t>(focus)].name);
		m.pendingPhysicsImpulse.targetPlayer = focus;
		m.pendingPhysicsImpulse.strength = 3.5f;
		m.pendingPhysicsImpulse.frames = 1;
	} else {
		std::snprintf(buf, sizeof(buf), "WORLD: Cat loses interest.");
		m.world.kind = EventKind::None;
		m.world.remainingTurns = 0;
	}
	pushWorld(m, focus, static_cast<int>(EventKind::Cat), buf);
}

void applyFloodLeak(Match& m, int seat)
{
	if (seat < 0 || seat >= m.config.playerCount) {
		return;
	}
	Player& p = m.players[static_cast<size_t>(seat)];
	if (p.eliminated) {
		return;
	}
	// Counter-play: shield halves flood chip (round up).
	int dmg = 1;
	if (p.shieldTurns > 0) {
		dmg = 0; // full block — rewards defense cards
	}
	if (dmg > 0) {
		p.towerHp = std::max(0, p.towerHp - dmg);
		char buf[120];
		std::snprintf(buf, sizeof(buf), "WORLD: Flood soaks %s for %d HP (now %d).", p.name, dmg, p.towerHp);
		pushWorld(m, seat, dmg, buf);
	} else {
		char buf[120];
		std::snprintf(buf, sizeof(buf), "WORLD: %s's shield holds back the flood.", p.name);
		pushWorld(m, seat, 0, buf);
	}
}

bool trySpawnEvent(Match& m)
{
	if (!m.config.eventsEnabled) {
		return false;
	}
	if (m.world.kind != EventKind::None) {
		return false;
	}
	if (m.eventCooldown > 0) {
		return false;
	}
	// Don't spam early game (M5: wait a bit longer).
	if (m.turnNumber < 5) {
		return false;
	}
	// ~20% chance per turn after cooldown — readable, not chaotic.
	if (randRange(m.rng, 0, 99) >= 20) {
		return false;
	}
	const EventKind kind = rollEventKind(m);
	if (kind == EventKind::None) {
		return false;
	}
	beginEvent(m, kind, -1);
	return true;
}

} // namespace

bool eventForcesAdjacent(const Match& match)
{
	return match.world.kind == EventKind::Sandstorm && match.world.remainingTurns > 0 && !match.world.warning;
}

int eventModifyAttackDamage(const Match& match, int baseDamage)
{
	int dmg = baseDamage;
	if (match.world.kind == EventKind::Rain && match.world.remainingTurns > 0) {
		if (dmg >= 2) {
			dmg -= 1;
		}
		// 1-damage chips stay 1 so the game doesn't freeze.
	}
	return std::max(0, dmg);
}

bool seatIsFlooded(const Match& match, int seat)
{
	return match.world.kind == EventKind::Flood && match.world.remainingTurns > 0 && match.world.focusSeat == seat;
}

void formatWorldEventStatus(const Match& match, char* out, int outCap)
{
	if (!out || outCap <= 0) {
		return;
	}
	if (match.world.kind == EventKind::None || match.world.remainingTurns <= 0) {
		std::snprintf(out, outCap, "Map: %s — calm", mapName(match.config.mapId));
		return;
	}
	switch (match.world.kind) {
	case EventKind::Sandstorm:
		std::snprintf(out, outCap, "Sandstorm (%d) — adjacent attacks only", match.world.remainingTurns);
		break;
	case EventKind::Rain:
		std::snprintf(out, outCap, "Rain (%d) — attack damage -1", match.world.remainingTurns);
		break;
	case EventKind::Flood:
		std::snprintf(out, outCap, "Flood on %s (%d) — 1 HP leak / their turn (shield blocks)",
					  match.world.focusSeat >= 0 ? match.players[static_cast<size_t>(match.world.focusSeat)].name : "?",
					  match.world.remainingTurns);
		break;
	case EventKind::Cat:
		if (match.world.warning) {
			std::snprintf(out, outCap, "Cat approaching %s…",
						  match.world.focusSeat >= 0
							  ? match.players[static_cast<size_t>(match.world.focusSeat)].name
							  : "?");
		} else {
			std::snprintf(out, outCap, "Cat on the table near %s (%d)",
						  match.world.focusSeat >= 0
							  ? match.players[static_cast<size_t>(match.world.focusSeat)].name
							  : "?",
						  match.world.remainingTurns);
		}
		break;
	default:
		std::snprintf(out, outCap, "Map: %s", mapName(match.config.mapId));
		break;
	}
}

void resetWorldEvents(Match& match)
{
	match.world = {};
	match.eventCooldown = 3; // M5: short grace after match start
}

void tickWorldEvents(Match& match)
{
	if (match.phase != Phase::Playing || !match.config.eventsEnabled) {
		return;
	}

	// Flood leak applies when the active player begins their turn under flood.
	if (seatIsFlooded(match, match.activePlayer)) {
		applyFloodLeak(match, match.activePlayer);
	}

	// Cat warning resolves into pounce.
	if (match.world.kind == EventKind::Cat && match.world.warning) {
		resolveCatPounce(match);
		// Don't also decrement this same tick for warning→pounce clarity.
		return;
	}

	// Countdown active event.
	if (match.world.kind != EventKind::None && match.world.remainingTurns > 0) {
		--match.world.remainingTurns;
		if (match.world.remainingTurns <= 0) {
			clearEvent(match, "");
		}
	}

	if (match.eventCooldown > 0) {
		--match.eventCooldown;
	}

	// Maybe spawn a new one if table is calm.
	trySpawnEvent(match);
}

bool forceWorldEvent(Match& match, EventKind kind, int focusSeat)
{
	if (kind == EventKind::None) {
		clearEvent(match, " (cleared)");
		return true;
	}
	if (match.phase != Phase::Playing) {
		return false;
	}
	beginEvent(match, kind, focusSeat);
	return match.world.kind == kind;
}

void mapTableColor(MapId map, float& r, float& g, float& b)
{
	switch (map) {
	case MapId::Desert:
		r = 0.72f;
		g = 0.58f;
		b = 0.32f;
		break;
	case MapId::Backyard:
		r = 0.22f;
		g = 0.52f;
		b = 0.30f;
		break;
	case MapId::LivingRoom:
	default:
		r = 0.28f;
		g = 0.55f;
		b = 0.35f;
		break;
	}
}

void mapFloorColor(MapId map, float& r, float& g, float& b)
{
	switch (map) {
	case MapId::Desert:
		r = 0.55f;
		g = 0.45f;
		b = 0.28f;
		break;
	case MapId::Backyard:
		r = 0.25f;
		g = 0.38f;
		b = 0.22f;
		break;
	default:
		r = 0.35f;
		g = 0.30f;
		b = 0.25f;
		break;
	}
}

} // namespace toy
