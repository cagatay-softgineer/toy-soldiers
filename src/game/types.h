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
constexpr uint16_t kProtocolVersion = 4; // M5: attackBonusNext rename in snapshot

enum class Phase : uint8_t {
	Lobby,
	Playing,
	GameOver,
};

enum class CardClass : uint8_t {
	Attack,
	Defense,
	Tactic,
};

enum class Rarity : uint8_t {
	Common,
	Rare,
	Epic,
	Legendary,
};

enum class TowerType : uint8_t {
	MachineGun,
	Sniper,
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
	}
	return "?";
}

inline const char* towerTypeName(TowerType t)
{
	switch (t) {
	case TowerType::MachineGun: return "Machine Gun";
	case TowerType::Sniper: return "Sniper";
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
	}
	return "?";
}

} // namespace toy
