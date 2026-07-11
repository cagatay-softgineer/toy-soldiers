#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Oyuncak Asker Masa Savasi — shared game types.
// Layers: Input → Rule Engine → Game State → Event System → Network Sync.

namespace toy {

constexpr int kMaxPlayers = 4;
constexpr int kSoldiersPerPlayer = 4;
constexpr int kStartingHandSize = 5;
constexpr int kDefaultTowerHp = 30;
constexpr int kMaxHandSize = 8;
constexpr int kPlayerNameLen = 24;
constexpr uint16_t kDefaultPort = 27123;
constexpr uint16_t kProtocolVersion = 5; // v0.7: modes, towers, keywords, crown, deck mods

enum class Phase : uint8_t {
	Lobby,
	Playing,
	GameOver,
};

enum class CardClass : uint8_t {
	Attack,
	Defense,
	Tactic,
	Event, // v0.7 #63: player-cast ambient world effect
};

enum class Rarity : uint8_t {
	Common,
	Rare,
	Epic,
	Legendary,
};

// v0.7 #42 keyword system — bitmask on CardDef.
enum CardKeyword : uint16_t {
	KwNone = 0,
	KwShield = 1 << 0,
	KwPierce = 1 << 1, // attack ignores target shield
	KwDraw = 1 << 2,
	KwHeal = 1 << 3,
	KwAdjacentOnly = 1 << 4,
	KwAOE = 1 << 5,   // hits both adjacent enemies; no single target pick
	KwEvent = 1 << 6, // casts a world event
};

enum class TowerType : uint8_t {
	MachineGun,
	Sniper,
	ShieldBearer, // v0.7 #37: starts with 1 shield, attacks -1
	Sapper,       // v0.7 #39: first attack each match +1
	Count,
};

// v0.7 #52-#57 session modes.
enum class GameMode : uint8_t {
	ClassicFFA = 0,
	QuickDuel = 1, // 1v1, 24 HP towers, 15-card decks
	Teams2v2 = 2,  // seats 0&2 vs 1&3, shared win
	HotPotato = 3, // crown holder takes +1 dmg; crown moves to attacker
	Sandbox = 4,   // practice: towers never die, free event spawns
	Count,
};

// v0.7 #58 AI difficulty.
enum class AiLevel : uint8_t {
	Easy = 0,   // random legal play
	Normal = 1, // heuristic
	Hard = 2,   // lethal-aware lookahead
};

// v0.7 #59 AI personalities (flavor + weight shifts).
enum class AiPersona : uint8_t {
	Balanced = 0,
	Aggro = 1,
	Turtle = 2,
	Chaos = 3,
};

// Table / room theme — drives visuals + event weights (M3).
enum class MapId : uint8_t {
	LivingRoom = 0, // cat + light rain
	Desert = 1,     // sandstorm + rare flood
	Backyard = 2,   // rain + flood
	Count = 3,
};

// Controlled world events (concept board M3).
enum class EventKind : uint8_t {
	None = 0,
	Sandstorm = 1, // adjacent-only targeting for N turns
	Rain = 2,      // attack damage -1
	Flood = 3,     // focus seat leaks 1 HP on their turn start
	Cat = 4,       // warn then physics knock (no HP by default)
	Dog = 5,       // v0.7 #66: pushes ALL soldiers inward (physics only)
	Blackout = 6,  // v0.7 #67: enemy HP hidden for 1 turn (UI fog)
};

enum class SeatControl : uint8_t {
	Empty,
	HumanLocal,
	HumanRemote,
	AI,
};

// --- M4 cosmetics (presentation only — no combat power) ---

enum class PlasticColor : uint8_t {
	ClassicGreen = 0,
	ArmyOlive,
	Blue,
	Red,
	Yellow,
	Purple,
	Orange,
	White,
	Pink,
	Camo,
	Count
};

enum class TowerSkin : uint8_t {
	Block = 0,      // classic cube tower
	Sandcastle,     // wider, sandy
	Rocket,         // tall thin
	Fort,           // cardboard fort
	Count
};

enum class Accessory : uint8_t {
	None = 0,
	PartyHat,
	SantaHat,
	Bandana,
	StarMedal,
	Count
};

struct Cosmetics {
	PlasticColor plastic = PlasticColor::ClassicGreen;
	TowerSkin towerSkin = TowerSkin::Block;
	Accessory accessory = Accessory::None;
};

struct CardDef {
	int id = 0;
	const char* name = "";
	const char* description = "";
	CardClass klass = CardClass::Attack;
	Rarity rarity = Rarity::Common;
	int cost = 1;
	int power = 0;
	int range = 1;
	bool freeTarget = true;
	uint16_t keywords = KwNone; // v0.7 #42
};

struct CardInstance {
	int defId = 0;
	int instanceId = 0;
};

struct Soldier {
	int id = 0;
	int owner = 0;
	bool alive = true;
	int bodyIndex = -1;
};

struct Player {
	int id = 0;
	char name[kPlayerNameLen] = {};
	TowerType tower = TowerType::MachineGun;
	int towerHp = kDefaultTowerHp;
	int towerMaxHp = kDefaultTowerHp;
	int shieldTurns = 0;
	// Persists across turns until the next attack spends it (Scout Peek etc.).
	int attackBonusNext = 0;
	bool eliminated = false;
	SeatControl control = SeatControl::Empty;
	bool ready = false;
	Cosmetics cosmetics{};
	// --- v0.7 Deep Toybox ---
	int team = -1;               // Teams2v2: 0 or 1, else -1
	bool firstAttackDone = false; // Sapper passive spent
	bool skipNextTurn = false;    // Line Cutter (no stacking)
	AiPersona persona = AiPersona::Balanced;
	// Deck builder lite (#47): ban up to 2 defs, add up to 2 sideboard copies (0 = unused).
	int bannedDefs[2] = { 0, 0 };
	int extraDefs[2] = { 0, 0 };
	std::vector<CardInstance> hand;
	std::vector<CardInstance> deck;
	std::vector<CardInstance> discard;
	std::vector<Soldier> soldiers;

	void setName(const char* n)
	{
		if (!n || !n[0]) {
			std::snprintf(name, sizeof(name), "Player");
			return;
		}
		std::snprintf(name, sizeof(name), "%s", n);
	}
};

// Active table-wide / room event (host authority, snapshotted).
struct WorldEventState {
	EventKind kind = EventKind::None;
	int remainingTurns = 0; // decremented at end of each turn while active
	int focusSeat = -1;     // flood / cat target
	bool warning = false;   // cat wind-up: announce, resolve next tick
};

struct MatchConfig {
	int playerCount = 4;
	bool freeTargeting = true;
	uint32_t seed = 42;
	bool fillEmptyWithAI = true;
	MapId mapId = MapId::LivingRoom;
	bool eventsEnabled = true;
	// --- v0.7 ---
	GameMode mode = GameMode::ClassicFFA;
	AiLevel aiLevel = AiLevel::Normal;
	int turnTimerSeconds = 0; // 0 off; 30/45/60 auto-skip (#56)
	bool paradeRest = false;  // reset fallen soldiers each round (#76)
};

struct MatchEvent {
	enum class Type : uint8_t {
		TurnStart,
		CardPlayed,
		Damage,
		Shield,
		Draw,
		PlayerEliminated,
		Winner,
		Info,
		WorldEvent,
	};
	Type type = Type::Info;
	int actor = -1;
	int target = -1;
	int value = 0;
	std::string text;
};

inline const char* cardClassName(CardClass c)
{
	switch (c) {
	case CardClass::Attack: return "Attack";
	case CardClass::Defense: return "Defense";
	case CardClass::Tactic: return "Tactic";
	case CardClass::Event: return "Event";
	}
	return "?";
}

inline const char* towerTypeName(TowerType t)
{
	switch (t) {
	case TowerType::MachineGun: return "Machine Gun";
	case TowerType::Sniper: return "Sniper";
	case TowerType::ShieldBearer: return "Shield Bearer";
	case TowerType::Sapper: return "Sapper";
	default: return "?";
	}
}

// v0.7 #37/#39 tower base stats (Quick Duel overrides HP to 24).
inline int towerBaseHp(TowerType t)
{
	switch (t) {
	case TowerType::MachineGun: return 36;
	case TowerType::Sniper: return 34;
	case TowerType::ShieldBearer: return 35;
	case TowerType::Sapper: return 34;
	default: return 30;
	}
}

// One-line passive description for pick UI / tooltips (#38, #40).
inline const char* towerPassive(TowerType t)
{
	switch (t) {
	case TowerType::MachineGun: return "Tank: no passive, most HP.";
	case TowerType::Sniper: return "Glass cannon: attacks +2 damage.";
	case TowerType::ShieldBearer: return "Starts with 1 shield turn; attacks -1 (min 1).";
	case TowerType::Sapper: return "First attack each match +1 damage.";
	default: return "";
	}
}

inline const char* gameModeName(GameMode m)
{
	switch (m) {
	case GameMode::ClassicFFA: return "Classic FFA";
	case GameMode::QuickDuel: return "Quick Duel";
	case GameMode::Teams2v2: return "2v2 Teams";
	case GameMode::HotPotato: return "Hot Potato King";
	case GameMode::Sandbox: return "Sandbox";
	default: return "?";
	}
}

inline const char* gameModeBlurb(GameMode m)
{
	switch (m) {
	case GameMode::ClassicFFA: return "4 seats, last tower standing.";
	case GameMode::QuickDuel: return "1v1, 24 HP towers, 15-card decks.";
	case GameMode::Teams2v2: return "Seats 0&2 vs 1&3 — shared win, no friendly fire.";
	case GameMode::HotPotato: return "Crown holder takes +1 damage; hitting them steals the crown.";
	case GameMode::Sandbox: return "Practice: towers reset instead of dying; spawn events freely.";
	default: return "";
	}
}

inline const char* aiLevelName(AiLevel l)
{
	switch (l) {
	case AiLevel::Easy: return "Easy";
	case AiLevel::Normal: return "Normal";
	case AiLevel::Hard: return "Hard";
	}
	return "?";
}

inline const char* aiPersonaName(AiPersona p)
{
	switch (p) {
	case AiPersona::Balanced: return "Balanced";
	case AiPersona::Aggro: return "Aggro";
	case AiPersona::Turtle: return "Turtle";
	case AiPersona::Chaos: return "Chaos";
	}
	return "?";
}

inline const char* phaseName(Phase p)
{
	switch (p) {
	case Phase::Lobby: return "Lobby";
	case Phase::Playing: return "Playing";
	case Phase::GameOver: return "Game Over";
	}
	return "?";
}

inline const char* seatControlName(SeatControl c)
{
	switch (c) {
	case SeatControl::Empty: return "Empty";
	case SeatControl::HumanLocal: return "You";
	case SeatControl::HumanRemote: return "Remote";
	case SeatControl::AI: return "AI";
	}
	return "?";
}

inline const char* mapName(MapId m)
{
	switch (m) {
	case MapId::LivingRoom: return "Living Room Table";
	case MapId::Desert: return "Desert Playmat";
	case MapId::Backyard: return "Backyard Picnic";
	default: return "?";
	}
}

inline const char* mapBlurb(MapId m)
{
	switch (m) {
	case MapId::LivingRoom: return "Green felt, toys, curious pets.";
	case MapId::Desert: return "Sandy mat — storms cut visibility.";
	case MapId::Backyard: return "Outdoor table — rain & puddles.";
	default: return "";
	}
}

inline const char* eventKindName(EventKind e)
{
	switch (e) {
	case EventKind::None: return "None";
	case EventKind::Sandstorm: return "Sandstorm";
	case EventKind::Rain: return "Rain";
	case EventKind::Flood: return "Flood";
	case EventKind::Cat: return "Cat";
	case EventKind::Dog: return "Dog";
	case EventKind::Blackout: return "Blackout";
	}
	return "?";
}

} // namespace toy
