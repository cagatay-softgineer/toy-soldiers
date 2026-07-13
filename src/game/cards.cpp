#include "game/cards.h"

#include "app/i18n.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <string>
#include <sys/stat.h>

namespace toy {

namespace {

// v0.9 built-in catalog: 30 defs (#43 grew it to 23; #148-era adds 7 sideboard cards).
// TR descriptions use the project's ASCII-Turkish convention (#153).
const CardDef kBuiltinCatalog[] = {
	// --- Attack ---
	{ 1, "Plastic Volley", "Deal 3 damage to a tower.", CardClass::Attack, Rarity::Common, 1, 3, 1, true, KwNone,
	  "Bir kuleye 3 hasar ver." },
	{ 2, "Bayonet Rush", "Deal 4 damage to a tower.", CardClass::Attack, Rarity::Rare, 2, 4, 1, true, KwNone,
	  "Bir kuleye 4 hasar ver." },
	{ 3, "Toy Mortar", "Deal 4 damage (sniper still +1).", CardClass::Attack, Rarity::Rare, 2, 4, 2, true, KwNone,
	  "4 hasar ver (keskin nisanci yine +1)." },
	{ 4, "March Fire", "Deal 2+2 damage (one strike).", CardClass::Attack, Rarity::Epic, 3, 2, 1, true, KwNone,
	  "2+2 hasar ver (tek vurus)." },
	{ 5, "Pellet Shot", "Deal 2 damage. Reliable chip.", CardClass::Attack, Rarity::Common, 1, 2, 1, true, KwNone,
	  "2 hasar ver. Guvenilir kemirme." },
	{ 6, "Tin Rain", "Deal 1 damage to BOTH adjacent enemies.", CardClass::Attack, Rarity::Rare, 1, 1, 1, false, KwAOE,
	  "HER IKI komsu dusmana 1 hasar ver." },
	{ 7, "Marble Strike", "Deal 3 damage. Pierce: ignores shield.", CardClass::Attack, Rarity::Rare, 2, 3, 1, true,
	  KwPierce, "3 hasar ver. Delici: kalkani yok sayar." },
	{ 8, "Slingshot", "Deal 1 damage. Draw 1 card.", CardClass::Attack, Rarity::Common, 1, 1, 1, true, KwDraw,
	  "1 hasar ver. 1 kart cek." },
	{ 9, "Domino Push", "Deal 3 damage. Adjacent targets only.", CardClass::Attack, Rarity::Rare, 1, 3, 1, false,
	  KwAdjacentOnly, "3 hasar ver. Sadece komsu hedefler." },
	// --- Defense ---
	{ 10, "Sandbag Wall", "Gain 1 turn of shield (halve incoming).", CardClass::Defense, Rarity::Common, 1, 1, 0, false,
	  KwShield, "1 tur kalkan kazan (gelen hasar yariya iner)." },
	{ 11, "Toy Fort", "Gain 2 turns of shield.", CardClass::Defense, Rarity::Rare, 2, 2, 0, false, KwShield,
	  "2 tur kalkan kazan." },
	{ 12, "Glue Trap", "Gain 1 shield turn; draw 1 card.", CardClass::Defense, Rarity::Epic, 2, 1, 0, false,
	  KwShield | KwDraw, "1 tur kalkan kazan; 1 kart cek." },
	{ 13, "Duck Tape", "Gain 1 shield turn.", CardClass::Defense, Rarity::Common, 1, 1, 0, false, KwShield,
	  "1 tur kalkan kazan." },
	{ 14, "Cardboard Bunker", "Gain 1 shield turn. Heal 2 HP.", CardClass::Defense, Rarity::Rare, 2, 1, 0, false,
	  KwShield | KwHeal, "1 tur kalkan kazan. 2 HP iyiles." },
	{ 15, "Blanket Fort", "Gain 3 turns of shield.", CardClass::Defense, Rarity::Epic, 3, 3, 0, false, KwShield,
	  "3 tur kalkan kazan." },
	{ 16, "Bubble Wrap", "Gain 1 shield turn. Heal 1 HP.", CardClass::Defense, Rarity::Common, 1, 1, 0, false,
	  KwShield | KwHeal, "1 tur kalkan kazan. 1 HP iyiles." },
	// --- Tactic ---
	{ 20, "Resupply", "Draw 2 cards.", CardClass::Tactic, Rarity::Common, 1, 2, 0, false, KwDraw, "2 kart cek." },
	{ 21, "Field Rations", "Draw 1 card. Heal tower 3 HP.", CardClass::Tactic, Rarity::Rare, 1, 3, 0, false,
	  KwDraw | KwHeal, "1 kart cek. Kuleyi 3 HP iyilestir." },
	{ 22, "Scout Peek", "Draw 1. Your next attack +1 (keeps across turns).", CardClass::Tactic, Rarity::Rare, 1, 1, 0,
	  false, KwDraw, "1 kart cek. Bir sonraki saldirin +1 (turlar boyunca kalir)." },
	{ 23, "Rearrange Toys", "Draw 1 card.", CardClass::Tactic, Rarity::Common, 0, 1, 0, false, KwDraw, "1 kart cek." },
	{ 24, "Ambush Order", "Deal 3; +2 if target has no shield.", CardClass::Attack, Rarity::Legendary, 3, 3, 1, true,
	  KwNone, "3 hasar ver; hedefin kalkani yoksa +2." },
	{ 25, "Medkit Foam", "Heal tower 4 HP.", CardClass::Tactic, Rarity::Epic, 2, 4, 0, false, KwHeal,
	  "Kuleyi 4 HP iyilestir." },
	{ 26, "Magnet Hand", "Steal 1 shield turn from target.", CardClass::Attack, Rarity::Epic, 2, 0, 1, true, KwShield,
	  "Hedeften 1 tur kalkan cal." },
	{ 27, "Line Cutter", "Next player skips their turn (no stacking).", CardClass::Tactic, Rarity::Epic, 3, 0, 0, false,
	  KwNone, "Siradaki oyuncu turunu atlar (uste binmez)." },
	{ 28, "Tidy Up", "Heal tower 2 HP.", CardClass::Tactic, Rarity::Common, 1, 2, 0, false, KwHeal,
	  "Kuleyi 2 HP iyilestir." },
	{ 29, "Battle Plan", "Draw 3 cards.", CardClass::Tactic, Rarity::Rare, 2, 3, 0, false, KwDraw, "3 kart cek." },
	// --- Late sideboard attacks ---
	{ 33, "Glitter Bomb", "Deal 2 damage to BOTH adjacent enemies.", CardClass::Attack, Rarity::Epic, 3, 2, 1, false,
	  KwAOE, "HER IKI komsu dusmana 2 hasar ver." },
	{ 34, "Night Watch", "Heal 2 HP. Gain 1 shield turn.", CardClass::Tactic, Rarity::Rare, 2, 2, 0, false,
	  KwHeal | KwShield, "2 HP iyiles. 1 tur kalkan kazan." },
	// --- Event cards (#63): max 2 per deck ---
	{ 30, "Call the Cat", "Event: the cat pounces someone. Soldiers scatter, no tower damage.", CardClass::Event,
	  Rarity::Rare, 1, 0, 0, false, KwEvent, "Olay: kedi birine atlar. Askerler dagilir, kule hasari yok." },
	{ 31, "Pocket Sand", "Event: everyone targets adjacent-only for 1 turn.", CardClass::Event, Rarity::Common, 1, 0, 0,
	  false, KwEvent, "Olay: herkes 1 tur boyunca sadece komsuyu hedefler." },
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

// --- v0.9 #148: runtime catalog (built-ins by default, JSON override optional) ---

std::vector<CardDef>& catalogVec()
{
	static std::vector<CardDef> cat(kBuiltinCatalog,
									kBuiltinCatalog + sizeof(kBuiltinCatalog) / sizeof(kBuiltinCatalog[0]));
	return cat;
}

// Owns strings referenced by JSON-loaded CardDefs (deque = stable addresses).
std::deque<std::string>& stringPool()
{
	static std::deque<std::string> pool;
	return pool;
}

const char* pooled(const std::string& s)
{
	stringPool().push_back(s);
	return stringPool().back().c_str();
}

// Minimal tolerant JSON helpers (schema-specific, no external deps).
bool jsonField(const std::string& obj, const char* key, std::string& out)
{
	char pat[64];
	std::snprintf(pat, sizeof(pat), "\"%s\"", key);
	size_t p = obj.find(pat);
	if (p == std::string::npos) {
		return false;
	}
	p = obj.find(':', p + std::strlen(pat));
	if (p == std::string::npos) {
		return false;
	}
	++p;
	while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' || obj[p] == '\r')) {
		++p;
	}
	if (p >= obj.size()) {
		return false;
	}
	if (obj[p] == '"') {
		++p;
		std::string v;
		while (p < obj.size() && obj[p] != '"') {
			if (obj[p] == '\\' && p + 1 < obj.size()) {
				++p;
			}
			v.push_back(obj[p]);
			++p;
		}
		out = v;
		return true;
	}
	// number / bool / bare word
	std::string v;
	while (p < obj.size() && obj[p] != ',' && obj[p] != '}' && obj[p] != ']' && obj[p] != '\n') {
		v.push_back(obj[p]);
		++p;
	}
	while (!v.empty() && (v.back() == ' ' || v.back() == '\r' || v.back() == '\t')) {
		v.pop_back();
	}
	out = v;
	return !v.empty();
}

CardClass classFromString(const std::string& s)
{
	if (s == "Defense") {
		return CardClass::Defense;
	}
	if (s == "Tactic") {
		return CardClass::Tactic;
	}
	if (s == "Event") {
		return CardClass::Event;
	}
	return CardClass::Attack;
}

Rarity rarityFromString(const std::string& s)
{
	if (s == "Rare") {
		return Rarity::Rare;
	}
	if (s == "Epic") {
		return Rarity::Epic;
	}
	if (s == "Legendary") {
		return Rarity::Legendary;
	}
	return Rarity::Common;
}

uint16_t keywordsFromString(const std::string& s)
{
	uint16_t kw = KwNone;
	if (s.find("Shield") != std::string::npos) {
		kw |= KwShield;
	}
	if (s.find("Pierce") != std::string::npos) {
		kw |= KwPierce;
	}
	if (s.find("Draw") != std::string::npos) {
		kw |= KwDraw;
	}
	if (s.find("Heal") != std::string::npos) {
		kw |= KwHeal;
	}
	if (s.find("AdjacentOnly") != std::string::npos) {
		kw |= KwAdjacentOnly;
	}
	if (s.find("AOE") != std::string::npos) {
		kw |= KwAOE;
	}
	if (s.find("Event") != std::string::npos) {
		kw |= KwEvent;
	}
	return kw;
}

const char* classToString(CardClass c)
{
	switch (c) {
	case CardClass::Defense: return "Defense";
	case CardClass::Tactic: return "Tactic";
	case CardClass::Event: return "Event";
	default: return "Attack";
	}
}

const char* rarityToString(Rarity r)
{
	switch (r) {
	case Rarity::Rare: return "Rare";
	case Rarity::Epic: return "Epic";
	case Rarity::Legendary: return "Legendary";
	default: return "Common";
	}
}

long long fileMtime(const char* path)
{
	struct stat st {};
	if (stat(path, &st) != 0) {
		return -1;
	}
	return static_cast<long long>(st.st_mtime);
}

} // namespace

std::span<const CardDef> cardCatalog()
{
	return std::span<const CardDef>(catalogVec().data(), catalogVec().size());
}

const CardDef* findCard(int defId)
{
	for (const CardDef& c : catalogVec()) {
		if (c.id == defId) {
			return &c;
		}
	}
	return nullptr;
}

const char* cardDescription(const CardDef& def)
{
	if (i18nLang() == Lang::Tr && def.descriptionTr && def.descriptionTr[0]) {
		return def.descriptionTr;
	}
	return def.description;
}

void cardsResetBuiltins()
{
	catalogVec().assign(kBuiltinCatalog, kBuiltinCatalog + sizeof(kBuiltinCatalog) / sizeof(kBuiltinCatalog[0]));
}

bool cardsLoadJsonText(const char* json)
{
	if (!json || !json[0]) {
		return false;
	}
	const std::string text(json);
	std::vector<CardDef> loaded;
	// Walk top-level-ish objects: every {...} that contains an "id" field.
	size_t pos = 0;
	while (true) {
		const size_t open = text.find('{', pos);
		if (open == std::string::npos) {
			break;
		}
		// The outermost { of the document also matches; requiring "id" filters it
		// only if it directly contains one before a nested brace — so scan balanced.
		int depth = 1;
		size_t i = open + 1;
		bool inStr = false;
		while (i < text.size() && depth > 0) {
			const char ch = text[i];
			if (inStr) {
				if (ch == '\\') {
					++i;
				} else if (ch == '"') {
					inStr = false;
				}
			} else if (ch == '"') {
				inStr = true;
			} else if (ch == '{') {
				++depth;
			} else if (ch == '}') {
				--depth;
			}
			++i;
		}
		const std::string obj = text.substr(open, i - open);
		// Leaf card objects contain "id" and no nested '{'.
		if (obj.find('{', 1) == std::string::npos && obj.find("\"id\"") != std::string::npos) {
			std::string v;
			CardDef def{};
			if (jsonField(obj, "id", v)) {
				def.id = std::atoi(v.c_str());
			}
			std::string name, desc, descTr;
			if (def.id > 0 && jsonField(obj, "name", name) && jsonField(obj, "desc", desc)) {
				def.name = pooled(name);
				def.description = pooled(desc);
				def.descriptionTr = jsonField(obj, "descTr", descTr) ? pooled(descTr) : "";
				if (jsonField(obj, "class", v)) {
					def.klass = classFromString(v);
				}
				if (jsonField(obj, "rarity", v)) {
					def.rarity = rarityFromString(v);
				}
				if (jsonField(obj, "cost", v)) {
					def.cost = std::atoi(v.c_str());
				}
				if (jsonField(obj, "power", v)) {
					def.power = std::atoi(v.c_str());
				}
				if (jsonField(obj, "range", v)) {
					def.range = std::atoi(v.c_str());
				}
				if (jsonField(obj, "freeTarget", v)) {
					def.freeTarget = (v[0] == 't' || v[0] == '1');
				}
				if (jsonField(obj, "keywords", v)) {
					def.keywords = keywordsFromString(v);
				}
				loaded.push_back(def);
			}
			pos = open + obj.size();
		} else {
			pos = open + 1;
		}
	}

	// Safety: refuse suspiciously small catalogs so a broken file can't nuke the game.
	if (loaded.size() < 10) {
		return false;
	}
	catalogVec() = std::move(loaded);
	return true;
}

bool cardsLoadJsonFile(const char* path)
{
	if (!path) {
		return false;
	}
	FILE* f = std::fopen(path, "rb");
	if (!f) {
		return false;
	}
	std::fseek(f, 0, SEEK_END);
	const long sz = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > 512 * 1024) {
		std::fclose(f);
		return false;
	}
	std::string buf(static_cast<size_t>(sz), '\0');
	const size_t rd = std::fread(buf.data(), 1, static_cast<size_t>(sz), f);
	std::fclose(f);
	buf.resize(rd);
	return cardsLoadJsonText(buf.c_str());
}

bool cardsExportJson(const char* path)
{
	if (!path) {
		return false;
	}
	FILE* f = std::fopen(path, "wb");
	if (!f) {
		return false;
	}
	std::fputs("{\n  \"comment\": \"toy-soldiers card catalog (v0.9 #148). Debug builds hot-reload this file.\",\n", f);
	std::fputs("  \"cards\": [\n", f);
	const auto cat = cardCatalog();
	for (size_t i = 0; i < cat.size(); ++i) {
		const CardDef& c = cat[i];
		std::string kw;
		const struct {
			uint16_t bit;
			const char* nm;
		} kws[] = { { KwShield, "Shield" }, { KwPierce, "Pierce" }, { KwDraw, "Draw" },   { KwHeal, "Heal" },
					{ KwAdjacentOnly, "AdjacentOnly" }, { KwAOE, "AOE" }, { KwEvent, "Event" } };
		for (const auto& k : kws) {
			if (c.keywords & k.bit) {
				if (!kw.empty()) {
					kw += ",";
				}
				kw += k.nm;
			}
		}
		std::fprintf(f,
					 "    { \"id\": %d, \"name\": \"%s\", \"desc\": \"%s\", \"descTr\": \"%s\", \"class\": \"%s\", "
					 "\"rarity\": \"%s\", \"cost\": %d, \"power\": %d, \"range\": %d, \"freeTarget\": %s, "
					 "\"keywords\": \"%s\" }%s\n",
					 c.id, c.name, c.description, c.descriptionTr ? c.descriptionTr : "", classToString(c.klass),
					 rarityToString(c.rarity), c.cost, c.power, c.range, c.freeTarget ? "true" : "false", kw.c_str(),
					 i + 1 < cat.size() ? "," : "");
	}
	std::fputs("  ]\n}\n", f);
	std::fclose(f);
	return true;
}

bool cardsReloadIfChanged(const char* path)
{
	static long long lastMtime = -1;
	const long long m = fileMtime(path);
	if (m < 0 || m == lastMtime) {
		return false;
	}
	lastMtime = m;
	return cardsLoadJsonFile(path); // first call = startup load, later calls = #149 hot reload
}

namespace {

uint32_t fnv1a(const uint8_t* data, size_t n, uint32_t hash = 0x811C9DC5u)
{
	for (size_t i = 0; i < n; ++i) {
		hash ^= data[i];
		hash *= 0x01000193u;
	}
	return hash;
}

} // namespace

uint32_t seedCommitHash(uint32_t seed)
{
	// Salted so the commitment does not trivially equal a hash table lookup of small seeds.
	const uint32_t salted[2] = { seed, 0x544F5921u }; // 'TOY!'
	return fnv1a(reinterpret_cast<const uint8_t*>(salted), sizeof(salted));
}

uint32_t deckCheckHash(const std::vector<CardInstance>& deck)
{
	std::vector<int> ids;
	ids.reserve(deck.size());
	for (const CardInstance& c : deck) {
		ids.push_back(c.defId);
	}
	std::sort(ids.begin(), ids.end()); // order-independent (decks are shuffled)
	uint32_t h = 0x811C9DC5u;
	for (int id : ids) {
		h = fnv1a(reinterpret_cast<const uint8_t*>(&id), sizeof(id), h);
	}
	return h;
}

void featuredWeeklyTweak(int& banDefOut, int& addDefOut, char* textOut, int textCap, long long weekOverride)
{
	// Rotate over commons to ban and sideboard cards to try.
	static const int kBanPool[] = { 1, 5, 10, 13, 20, 23 };
	static const int kAddPool[] = { 9, 15, 16, 28, 29, 33, 34 };
	long long week = weekOverride;
	if (week < 0) {
		week = static_cast<long long>(std::time(nullptr)) / (86400LL * 7LL);
	}
	banDefOut = kBanPool[static_cast<size_t>(week % 6)];
	addDefOut = kAddPool[static_cast<size_t>(week % 7)];
	if (textOut && textCap > 0) {
		const CardDef* ban = findCard(banDefOut);
		const CardDef* add = findCard(addDefOut);
		std::snprintf(textOut, static_cast<size_t>(textCap), "Featured tweak: bench %s, field-test %s",
					  ban ? ban->name : "?", add ? add->name : "?");
	}
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
		if (!d) {
			continue; // JSON catalog may drop recipe cards — skip cleanly (#148)
		}
		if (d->klass == CardClass::Event) {
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
	if (sig != 0 && !isBanned(forPlayer, sig) && findCard(sig)) {
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
