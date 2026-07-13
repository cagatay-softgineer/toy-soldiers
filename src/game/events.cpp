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

// Weights per map for random rolls (relative ints). v0.9 adds ants + 4 new rooms (#136-#139).
struct EventWeights {
	int sand = 0, rain = 0, flood = 0, cat = 0, dog = 0, blackout = 0, ants = 0, titan = 0;
};

EventWeights eventWeightsFor(MapId map)
{
	EventWeights w;
	switch (map) {
	case MapId::LivingRoom:
		w.titan = 4; // v1.2 #69: rare boss
		w.cat = 40;
		w.rain = 20;
		w.dog = 15;
		w.blackout = 10;
		w.flood = 5;
		w.sand = 5;
		break;
	case MapId::Desert:
		w.sand = 50;
		w.flood = 15;
		w.blackout = 10;
		w.rain = 5;
		w.cat = 5;
		w.dog = 5;
		break;
	case MapId::Backyard:
		w.rain = 35;
		w.flood = 25;
		w.dog = 15;
		w.cat = 10;
		w.ants = 5;
		w.sand = 5;
		w.blackout = 5;
		break;
	case MapId::KidsBedroom:
		w.titan = 5;
		w.cat = 35;
		w.blackout = 30;
		w.dog = 10;
		w.flood = 5;
		w.rain = 5;
		w.ants = 5;
		break;
	case MapId::Garage:
		w.titan = 4;
		w.flood = 35; // oil spills
		w.blackout = 25;
		w.dog = 10;
		w.cat = 5;
		w.sand = 5;
		w.rain = 5;
		w.ants = 5;
		break;
	case MapId::PicnicBlanket:
		w.ants = 45;
		w.rain = 15;
		w.dog = 15;
		w.cat = 10;
		w.flood = 5;
		w.sand = 5;
		break;
	case MapId::ChristmasTable:
		w.rain = 30; // gentle snow
		w.cat = 30;
		w.blackout = 10;
		w.dog = 10;
		w.flood = 5;
		break;
	default:
		break;
	}
	return w;
}

EventKind rollEventKind(Match& m)
{
	const EventWeights w = eventWeightsFor(m.config.mapId);
	const int total = w.sand + w.rain + w.flood + w.cat + w.dog + w.blackout + w.ants + w.titan;
	if (total <= 0) {
		return EventKind::None;
	}
	int r = randRange(m.rng, 0, total - 1);
	if ((r -= w.sand) < 0) {
		return EventKind::Sandstorm;
	}
	if ((r -= w.rain) < 0) {
		return EventKind::Rain;
	}
	if ((r -= w.flood) < 0) {
		return EventKind::Flood;
	}
	if ((r -= w.cat) < 0) {
		return EventKind::Cat;
	}
	if ((r -= w.dog) < 0) {
		return EventKind::Dog;
	}
	if ((r -= w.blackout) < 0) {
		return EventKind::Blackout;
	}
	if ((r -= w.ants) < 0) {
		return EventKind::Ants;
	}
	return EventKind::Titan;
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
	case EventKind::Dog:
		// #66: one big table shake — every seat's soldiers get pushed inward. No HP.
		m.world.remainingTurns = 1;
		std::snprintf(buf, sizeof(buf), "WORLD: A dog bumps the table! Soldiers everywhere scatter.");
		pushWorld(m, -1, static_cast<int>(kind), buf);
		m.pendingPhysicsImpulse.targetPlayer = kImpulseAllSeats;
		m.pendingPhysicsImpulse.strength = 2.8f;
		m.pendingPhysicsImpulse.frames = 1;
		break;
	case EventKind::Blackout:
		// #67: UI fog — enemy HP hidden for 1 turn. No gameplay change.
		m.world.remainingTurns = 1;
		std::snprintf(buf, sizeof(buf), "WORLD: Blackout! Enemy tower HP is hidden for 1 turn.");
		pushWorld(m, -1, static_cast<int>(kind), buf);
		break;
	case EventKind::Titan:
		// v1.2 #69: rare map boss — two full turns of telegraph, then a table-wide stomp. No HP.
		m.world.remainingTurns = 2;
		m.world.warning = true;
		std::snprintf(buf, sizeof(buf), "WORLD: Distant stomping… something HUGE is coming (2 turns).");
		pushWorld(m, -1, static_cast<int>(kind), buf);
		break;
	case EventKind::Ants:
		// v0.9 #138: ants swarm one seat — soldiers jitter on that seat's turns. No HP.
		m.world.remainingTurns = 2;
		if (focus < 0) {
			focus = pickLivingSeat(m);
			m.world.focusSeat = focus;
		}
		if (focus >= 0) {
			std::snprintf(buf, sizeof(buf), "WORLD: Ants! A column marches through %s's soldiers.",
						  m.players[static_cast<size_t>(focus)].name);
			m.pendingPhysicsImpulse.targetPlayer = focus;
			m.pendingPhysicsImpulse.strength = 1.2f;
			m.pendingPhysicsImpulse.frames = 1;
		} else {
			std::snprintf(buf, sizeof(buf), "WORLD: Ants wander off (no seats).");
			m.world.kind = EventKind::None;
			m.world.remainingTurns = 0;
		}
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

bool eventHidesHp(const Match& match)
{
	return match.world.kind == EventKind::Blackout && match.world.remainingTurns > 0;
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
	case EventKind::Dog:
		std::snprintf(out, outCap, "Dog bump (%d) — soldiers scattered, no HP damage", match.world.remainingTurns);
		break;
	case EventKind::Blackout:
		std::snprintf(out, outCap, "Blackout (%d) — enemy tower HP hidden", match.world.remainingTurns);
		break;
	case EventKind::Ants:
		std::snprintf(out, outCap, "Ants on %s (%d) — soldiers jitter, no HP damage",
					  match.world.focusSeat >= 0 ? match.players[static_cast<size_t>(match.world.focusSeat)].name : "?",
					  match.world.remainingTurns);
		break;
	case EventKind::Titan:
		if (match.world.warning) {
			std::snprintf(out, outCap, "TOY TITAN incoming — stomp in %d turn%s!", match.world.remainingTurns,
						  match.world.remainingTurns == 1 ? "" : "s");
		} else {
			std::snprintf(out, outCap, "Toy Titan dust settling (%d)", match.world.remainingTurns);
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

	// v0.9 Ants: jitter the swarmed seat's soldiers when their turn starts (physics only).
	if (match.world.kind == EventKind::Ants && match.world.remainingTurns > 0 &&
		match.world.focusSeat == match.activePlayer && match.world.focusSeat >= 0) {
		match.pendingPhysicsImpulse.targetPlayer = match.world.focusSeat;
		match.pendingPhysicsImpulse.strength = 1.0f;
		match.pendingPhysicsImpulse.frames = 1;
	}

	// Cat warning resolves into pounce.
	if (match.world.kind == EventKind::Cat && match.world.warning) {
		resolveCatPounce(match);
		// Don't also decrement this same tick for warning→pounce clarity.
		return;
	}

	// v1.2 #69: Titan telegraph counts down, then the stomp hits every seat at once.
	if (match.world.kind == EventKind::Titan && match.world.warning) {
		--match.world.remainingTurns;
		if (match.world.remainingTurns > 0) {
			char tbuf[120];
			std::snprintf(tbuf, sizeof(tbuf), "WORLD: The stomping grows LOUDER… (%d turn%s)",
						  match.world.remainingTurns, match.world.remainingTurns == 1 ? "" : "s");
			pushWorld(match, -1, static_cast<int>(EventKind::Titan), tbuf);
		} else {
			match.world.warning = false;
			match.world.remainingTurns = 1; // dust settles for one turn
			pushWorld(match, -1, static_cast<int>(EventKind::Titan),
					  "WORLD: THE TOY TITAN STOMPS PAST! Every soldier on the table goes flying (no tower damage).");
			match.pendingPhysicsImpulse.targetPlayer = kImpulseAllSeats;
			match.pendingPhysicsImpulse.strength = 5.5f;
			match.pendingPhysicsImpulse.frames = 1;
		}
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
	case MapId::KidsBedroom: // soft blue play rug
		r = 0.26f;
		g = 0.34f;
		b = 0.62f;
		break;
	case MapId::Garage: // scuffed workbench top
		r = 0.42f;
		g = 0.38f;
		b = 0.32f;
		break;
	case MapId::PicnicBlanket: // red-checkered blanket
		r = 0.72f;
		g = 0.28f;
		b = 0.26f;
		break;
	case MapId::ChristmasTable: // deep green runner
		r = 0.14f;
		g = 0.38f;
		b = 0.22f;
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
	case MapId::KidsBedroom: // dark bedroom carpet
		r = 0.16f;
		g = 0.16f;
		b = 0.26f;
		break;
	case MapId::Garage: // concrete
		r = 0.30f;
		g = 0.30f;
		b = 0.32f;
		break;
	case MapId::PicnicBlanket: // park grass
		r = 0.22f;
		g = 0.42f;
		b = 0.20f;
		break;
	case MapId::ChristmasTable: // warm wood floor
		r = 0.34f;
		g = 0.22f;
		b = 0.14f;
		break;
	default:
		r = 0.35f;
		g = 0.30f;
		b = 0.25f;
		break;
	}
}

} // namespace toy
