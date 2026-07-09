#include "game/cards.h"

#include <algorithm>
#include <cassert>

namespace toy {

namespace {

// M5-balanced catalog (Attack / Defense / Tactic). Values tuned for ~30–50 turn FFA.
const CardDef kCatalog[] = {
	// --- Attack ---
	{ 1, "Plastic Volley", "Deal 3 damage to a tower.", CardClass::Attack, Rarity::Common, 1, 3, 1, true },
	{ 2, "Bayonet Rush", "Deal 4 damage to a tower.", CardClass::Attack, Rarity::Rare, 2, 4, 1, true },
	{ 3, "Toy Mortar", "Deal 4 damage (sniper still +1).", CardClass::Attack, Rarity::Rare, 2, 4, 2, true },
	{ 4, "March Fire", "Deal 2+2 damage (one strike).", CardClass::Attack, Rarity::Epic, 3, 2, 1, true },
	{ 5, "Pellet Shot", "Deal 2 damage. Reliable chip.", CardClass::Attack, Rarity::Common, 1, 2, 1, true },
	// --- Defense ---
	{ 10, "Sandbag Wall", "Gain 1 turn of shield (halve incoming).", CardClass::Defense, Rarity::Common, 1, 1, 0, false },
	{ 11, "Toy Fort", "Gain 2 turns of shield.", CardClass::Defense, Rarity::Rare, 2, 2, 0, false },
	{ 12, "Glue Trap", "Gain 1 shield turn; draw 1 card.", CardClass::Defense, Rarity::Epic, 2, 1, 0, false },
	{ 13, "Duck Tape", "Gain 1 shield turn.", CardClass::Defense, Rarity::Common, 1, 1, 0, false },
	// --- Tactic ---
	{ 20, "Resupply", "Draw 2 cards.", CardClass::Tactic, Rarity::Common, 1, 2, 0, false },
	{ 21, "Field Rations", "Draw 1 card. Heal tower 3 HP.", CardClass::Tactic, Rarity::Rare, 1, 3, 0, false },
	{ 22, "Scout Peek", "Draw 1. Your next attack +1 (keeps across turns).", CardClass::Tactic, Rarity::Rare, 1, 1, 0, false },
	{ 23, "Rearrange Toys", "Draw 1 card.", CardClass::Tactic, Rarity::Common, 0, 1, 0, false },
	{ 24, "Ambush Order", "Deal 3; +2 if target has no shield.", CardClass::Attack, Rarity::Legendary, 3, 3, 1, true },
	{ 25, "Medkit Foam", "Heal tower 4 HP.", CardClass::Tactic, Rarity::Epic, 2, 4, 0, false },
};

// 22-card starter: commons heavy, one legendary.
const int kDeckRecipe[] = {
	// Attack (9)
	1, 1, 1, 5, 5, 2, 2, 3, 4,
	// Defense (6)
	10, 10, 13, 13, 11, 12,
	// Tactic (6) + Ambush (1)
	20, 20, 21, 22, 23, 25, 24,
};

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

std::vector<CardInstance> makeStarterDeck(int /*playerId*/, uint32_t& rngState, int& nextInstanceId)
{
	std::vector<CardInstance> deck;
	deck.reserve(sizeof(kDeckRecipe) / sizeof(kDeckRecipe[0]));
	for (int defId : kDeckRecipe) {
		CardInstance inst;
		inst.defId = defId;
		inst.instanceId = nextInstanceId++;
		deck.push_back(inst);
	}
	shuffleInPlace(deck, rngState);
	return deck;
}

} // namespace toy
