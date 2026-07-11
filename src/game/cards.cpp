#include "game/cards.h"

#include <algorithm>
#include <cassert>

namespace toy {

namespace {

// v0.7 Deep Toybox catalog (Attack / Defense / Tactic / Event) with keywords (#42, #43).
const CardDef kCatalog[] = {
	// --- Attack ---
	{ 1, "Plastic Volley", "Deal 3 damage to a tower.", CardClass::Attack, Rarity::Common, 1, 3, 1, true, KwNone },
	{ 2, "Bayonet Rush", "Deal 4 damage to a tower.", CardClass::Attack, Rarity::Rare, 2, 4, 1, true, KwNone },
	{ 3, "Toy Mortar", "Deal 4 damage (sniper still +1).", CardClass::Attack, Rarity::Rare, 2, 4, 2, true, KwNone },
	{ 4, "March Fire", "Deal 2+2 damage (one strike).", CardClass::Attack, Rarity::Epic, 3, 2, 1, true, KwNone },
	{ 5, "Pellet Shot", "Deal 2 damage. Reliable chip.", CardClass::Attack, Rarity::Common, 1, 2, 1, true, KwNone },
	{ 6, "Tin Rain", "Deal 1 damage to BOTH adjacent enemies.", CardClass::Attack, Rarity::Rare, 1, 1, 1, false,
	  KwAOE }, // #44
	{ 7, "Marble Strike", "Deal 3 damage. Pierce: ignores shield.", CardClass::Attack, Rarity::Rare, 2, 3, 1, true,
	  KwPierce },
	{ 8, "Slingshot", "Deal 1 damage. Draw 1 card.", CardClass::Attack, Rarity::Common, 1, 1, 1, true, KwDraw },
	// --- Defense ---
	{ 10, "Sandbag Wall", "Gain 1 turn of shield (halve incoming).", CardClass::Defense, Rarity::Common, 1, 1, 0, false,
	  KwShield },
	{ 11, "Toy Fort", "Gain 2 turns of shield.", CardClass::Defense, Rarity::Rare, 2, 2, 0, false, KwShield },
	{ 12, "Glue Trap", "Gain 1 shield turn; draw 1 card.", CardClass::Defense, Rarity::Epic, 2, 1, 0, false,
	  KwShield | KwDraw },
	{ 13, "Duck Tape", "Gain 1 shield turn.", CardClass::Defense, Rarity::Common, 1, 1, 0, false, KwShield },
	{ 14, "Cardboard Bunker", "Gain 1 shield turn. Heal 2 HP.", CardClass::Defense, Rarity::Rare, 2, 1, 0, false,
	  KwShield | KwHeal },
	// --- Tactic ---
	{ 20, "Resupply", "Draw 2 cards.", CardClass::Tactic, Rarity::Common, 1, 2, 0, false, KwDraw },
	{ 21, "Field Rations", "Draw 1 card. Heal tower 3 HP.", CardClass::Tactic, Rarity::Rare, 1, 3, 0, false,
	  KwDraw | KwHeal },
	{ 22, "Scout Peek", "Draw 1. Your next attack +1 (keeps across turns).", CardClass::Tactic, Rarity::Rare, 1, 1, 0,
	  false, KwDraw },
	{ 23, "Rearrange Toys", "Draw 1 card.", CardClass::Tactic, Rarity::Common, 0, 1, 0, false, KwDraw },
	{ 24, "Ambush Order", "Deal 3; +2 if target has no shield.", CardClass::Attack, Rarity::Legendary, 3, 3, 1, true,
	  KwNone },
	{ 25, "Medkit Foam", "Heal tower 4 HP.", CardClass::Tactic, Rarity::Epic, 2, 4, 0, false, KwHeal },
	{ 26, "Magnet Hand", "Steal 1 shield turn from target.", CardClass::Attack, Rarity::Epic, 2, 0, 1, true,
	  KwShield }, // #45
	{ 27, "Line Cutter", "Next player skips their turn (no stacking).", CardClass::Tactic, Rarity::Epic, 3, 0, 0, false,
	  KwNone }, // #46
	// --- Event cards (#63): max 2 per deck ---
	{ 30, "Call the Cat", "Event: the cat pounces someone. Soldiers scatter, no tower damage.", CardClass::Event,
	  Rarity::Rare, 1, 0, 0, false, KwEvent }, // #64
	{ 31, "Pocket Sand", "Event: everyone targets adjacent-only for 1 turn.", CardClass::Event, Rarity::Common, 1, 0, 0,
	  false, KwEvent }, // #65
};

// Classic FFA starter (30 cards, exactly 2 Event class — deck cap #63).
const int kDeckRecipe[] = {
	// Attack (12)
	1, 1, 1, 5, 5, 2, 2, 3, 4, 6, 7, 8,
	// Defense (7)
	10, 10, 13, 13, 11, 12, 14,
	// Tactic (8) + Ambush (1)
	20, 20, 21, 22, 23, 25, 27, 24,
	// Events (2)
	30, 31,
};

// Quick Duel: lean 15-card deck (#53). 1 event max.
const int kDuelRecipe[] = {
	// Attack (7)
	1, 1, 5, 5, 2, 7, 6,
	// Defense (4)
	10, 13, 11, 14,
	// Tactic (3)
	20, 21, 23,
	// Event (1)
	31,
};

// v0.7 #41 (P2): each tower injects one signature starter card.
int towerStarterCard(TowerType t)
{
	switch (t) {
	case TowerType::MachineGun: return 5;  // Pellet Shot — steady chip
	case TowerType::Sniper: return 22;     // Scout Peek — set up the big shot
	case TowerType::ShieldBearer: return 13; // Duck Tape — lean into shields
	case TowerType::Sapper: return 2;      // Bayonet Rush — spike the opener
	default: return 0;
	}
}

bool isBanned(const Player& p, int defId)
{
	return defId != 0 && (p.bannedDefs[0] == defId || p.bannedDefs[1] == defId);
}

} // namespace

std::span<const CardDef> cardCatalog()
{
	return std::span<const CardDef>(kCatalog, sizeof(kCatalog) / sizeof(kCatalog[0]));
}

const CardDef* findCard(int defId)
{
	for (const CardDef& c : kCatalog) {
		if (c.id == defId) {
			return &c;
		}
	}
	return nullptr;
}

uint32_t xorshift32(uint32_t& state)
{
	uint32_t x = state ? state : 1u;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	state = x;
	return x;
}

int randRange(uint32_t& state, int lo, int hiInclusive)
{
	assert(hiInclusive >= lo);
	const uint32_t span = static_cast<uint32_t>(hiInclusive - lo + 1);
	return lo + static_cast<int>(xorshift32(state) % span);
}

void shuffleInPlace(std::vector<CardInstance>& cards, uint32_t& rngState)
{
	for (int i = static_cast<int>(cards.size()) - 1; i > 0; --i) {
		const int j = randRange(rngState, 0, i);
		std::swap(cards[static_cast<size_t>(i)], cards[static_cast<size_t>(j)]);
	}
}

std::vector<CardInstance> makeStarterDeck(int /*playerId*/, uint32_t& rngState, int& nextInstanceId, GameMode mode,
										  const Player& forPlayer)
{
	const int* recipe = kDeckRecipe;
	size_t recipeN = sizeof(kDeckRecipe) / sizeof(kDeckRecipe[0]);
	if (mode == GameMode::QuickDuel) {
		recipe = kDuelRecipe;
		recipeN = sizeof(kDuelRecipe) / sizeof(kDuelRecipe[0]);
	}

	std::vector<int> defs;
	defs.reserve(recipeN + 3);
	int eventCount = 0;
	for (size_t i = 0; i < recipeN; ++i) {
		const int defId = recipe[i];
		if (isBanned(forPlayer, defId)) {
			continue; // #47: banned defs removed entirely
		}
		const CardDef* d = findCard(defId);
		if (d && d->klass == CardClass::Event) {
			++eventCount;
		}
		defs.push_back(defId);
	}

	// #47 sideboard adds (one copy each), respecting the 2-event-card deck cap.
	for (int extra : forPlayer.extraDefs) {
		const CardDef* d = findCard(extra);
		if (!d || isBanned(forPlayer, extra)) {
			continue;
		}
		if (d->klass == CardClass::Event) {
			if (eventCount >= 2) {
				continue;
			}
			++eventCount;
		}
		defs.push_back(extra);
	}

	// #41: tower signature card (never a banned one).
	const int sig = towerStarterCard(forPlayer.tower);
	if (sig != 0 && !isBanned(forPlayer, sig)) {
		defs.push_back(sig);
	}

	std::vector<CardInstance> deck;
	deck.reserve(defs.size());
	for (int defId : defs) {
		CardInstance inst;
		inst.defId = defId;
		inst.instanceId = nextInstanceId++;
		deck.push_back(inst);
	}
	shuffleInPlace(deck, rngState);
	return deck;
}

} // namespace toy
