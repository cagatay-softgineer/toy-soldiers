#include "app/i18n.h"
#include "game/cards.h"
#include "game/cosmetics.h"
#include "game/events.h"
#include "game/match.h"
#include "game/rules.h"
#include "game/snapshot.h"

#include <cstdio>
#include <cstring>
#include <string>

// v0.9 Identity & Content gate: maps, cosmetics, JSON catalog, unlocks, ants, TR text.

static int g_fails = 0;

#define CHECK(cond, msg)                                                                                               \
	do {                                                                                                               \
		if (!(cond)) {                                                                                                 \
			std::printf("FAIL: %s\n", msg);                                                                            \
			++g_fails;                                                                                                 \
		}                                                                                                              \
	} while (0)

using namespace toy;

int main(int argc, char** argv)
{
	// Bootstrap mode: write the shipping data file from the built-in catalog.
	if (argc >= 3 && std::strcmp(argv[1], "--export-cards") == 0) {
		const bool ok = cardsExportJson(argv[2]);
		std::printf(ok ? "exported %s\n" : "export FAILED %s\n", argv[2]);
		return ok ? 0 : 1;
	}

	// --- Catalog ≥ 30, TR text complete (#153), localized lookup ---
	{
		CHECK(static_cast<int>(cardCatalog().size()) >= 30, "catalog has >= 30 defs (v0.9 goal)");
		for (const CardDef& c : cardCatalog()) {
			CHECK(c.name && c.name[0], "card has a name");
			CHECK(c.description && c.description[0], "card has EN text");
			CHECK(c.descriptionTr && c.descriptionTr[0], "card has TR text (#153)");
		}
		const CardDef* volley = findCard(1);
		CHECK(volley != nullptr, "volley exists");
		i18nSetLang(Lang::En);
		CHECK(std::strstr(cardDescription(*volley), "Deal 3") != nullptr, "EN description");
		i18nSetLang(Lang::Tr);
		CHECK(std::strstr(cardDescription(*volley), "3 hasar") != nullptr, "TR description");
		i18nSetLang(Lang::En);
		std::printf("OK catalog + TR text (%d defs)\n", static_cast<int>(cardCatalog().size()));
	}

	// --- JSON export/load round-trip (#148) + broken-file safety ---
	{
		const char* tmp = "content_test_cards.json";
		const size_t builtinCount = cardCatalog().size();
		CHECK(cardsExportJson(tmp), "export json");
		cardsResetBuiltins();
		CHECK(cardsLoadJsonFile(tmp), "load exported json");
		CHECK(cardCatalog().size() == builtinCount, "round-trip keeps every card");
		const CardDef* marble = findCard(7);
		CHECK(marble && (marble->keywords & KwPierce), "keywords survive round-trip");
		CHECK(marble && marble->rarity == Rarity::Rare, "rarity survives round-trip");
		const CardDef* tinRain = findCard(6);
		CHECK(tinRain && (tinRain->keywords & KwAOE) && !tinRain->freeTarget, "flags survive round-trip");

		// Broken/short files must not clobber the catalog.
		CHECK(!cardsLoadJsonText("{ \"cards\": [ { \"id\": 1, \"name\": \"X\", \"desc\": \"y\" } ] }"),
			  "tiny catalog rejected");
		CHECK(!cardsLoadJsonText("garbage{{{"), "garbage rejected");
		CHECK(cardCatalog().size() == builtinCount, "catalog intact after bad loads");
		std::remove(tmp);

		// A custom catalog that drops a recipe card must not break deck building.
		std::string mini = "{ \"cards\": [";
		for (int i = 0; i < 12; ++i) {
			char obj[160];
			std::snprintf(obj, sizeof(obj),
						  "%s{ \"id\": %d, \"name\": \"Test%d\", \"desc\": \"d\", \"class\": \"Attack\", "
						  "\"power\": 2 }",
						  i ? "," : "", 100 + i, i);
			mini += obj;
		}
		mini += "] }";
		CHECK(cardsLoadJsonText(mini.c_str()), "custom mini catalog loads");
		Match m;
		MatchConfig cfg;
		cfg.seed = 5;
		cfg.eventsEnabled = false;
		startMatch(m, cfg); // recipe ids all missing -> decks come out empty but valid
		for (int i = 0; i < m.config.playerCount; ++i) {
			for (const CardInstance& ci : m.players[static_cast<size_t>(i)].deck) {
				CHECK(findCard(ci.defId) != nullptr, "no dangling defs in deck");
			}
		}
		cardsResetBuiltins();
		CHECK(cardCatalog().size() == builtinCount, "builtins restored");
		std::printf("OK cards.json round-trip + safety\n");
	}

	// --- 7 maps (#136-#139): names, blurbs, colors, and a full match on each ---
	{
		for (int mi = 0; mi < static_cast<int>(MapId::Count); ++mi) {
			const MapId map = static_cast<MapId>(mi);
			CHECK(std::strcmp(mapName(map), "?") != 0, "map has a name");
			CHECK(mapBlurb(map)[0] != '\0', "map has flavor text (#156)");
			float r = -1, g = -1, b = -1;
			mapTableColor(map, r, g, b);
			CHECK(r >= 0 && g >= 0 && b >= 0, "table color set");
			mapFloorColor(map, r, g, b);
			CHECK(r >= 0 && g >= 0 && b >= 0, "floor color set");
			mapClearColor(map, r, g, b);
			CHECK(r >= 0 && g >= 0 && b >= 0, "clear color set");

			Match m;
			MatchConfig cfg;
			cfg.seed = 900u + static_cast<uint32_t>(mi) * 7u;
			cfg.eventsEnabled = true;
			cfg.mapId = map;
			startMatch(m, cfg);
			int safety = 0;
			while (m.phase == Phase::Playing && safety < 900) {
				autoPlayBest(m);
				++safety;
			}
			char msg[64];
			std::snprintf(msg, sizeof(msg), "match finishes on %s", mapName(map));
			CHECK(m.phase == Phase::GameOver, msg);
		}
		std::printf("OK 7 maps\n");
	}

	// --- Ants event (#138) ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 77;
		cfg.mapId = MapId::PicnicBlanket;
		startMatch(m, cfg);
		CHECK(forceWorldEvent(m, EventKind::Ants, 1), "ants spawn");
		CHECK(m.world.kind == EventKind::Ants && m.world.focusSeat == 1, "ants focus seat");
		CHECK(m.pendingPhysicsImpulse.frames > 0, "ants set a physics cue");
		const int hp = m.players[1].towerHp;
		m.activePlayer = 1;
		tickWorldEvents(m);
		CHECK(m.players[1].towerHp == hp, "ants never deal HP damage");
		char status[160];
		formatWorldEventStatus(m, status, sizeof(status));
		CHECK(std::strstr(status, "Ants") != nullptr || m.world.kind == EventKind::None, "ants status text");
		std::printf("OK ants event\n");
	}

	// --- Cosmetics wave (#142-#144): counts, names, sane colors/extents ---
	{
		CHECK(plasticColorCount() == 20, "20 plastic colors");
		CHECK(towerSkinCount() == 8, "8 tower skins");
		CHECK(accessoryCount() == 11, "11 accessories");
		for (int i = 0; i < plasticColorCount(); ++i) {
			CHECK(std::strcmp(plasticColorName(static_cast<PlasticColor>(i)), "?") != 0, "plastic name");
			const b3Vec3 c = plasticColorRgb(static_cast<PlasticColor>(i));
			CHECK(c.x >= 0 && c.x <= 1.01f && c.y >= 0 && c.y <= 1.01f && c.z >= 0 && c.z <= 1.01f, "plastic rgb");
		}
		for (int i = 0; i < towerSkinCount(); ++i) {
			CHECK(std::strcmp(towerSkinName(static_cast<TowerSkin>(i)), "?") != 0, "skin name");
			float hx = 0, hy = 0, hz = 0;
			towerHalfExtents(static_cast<TowerSkin>(i), hx, hy, hz);
			CHECK(hx > 0 && hy > 0 && hz > 0, "skin extents positive");
		}
		for (int i = 0; i < accessoryCount(); ++i) {
			CHECK(std::strcmp(accessoryName(static_cast<Accessory>(i)), "?") != 0, "accessory name");
		}
		std::printf("OK cosmetics wave (39 items)\n");
	}

	// --- Unlock track (#146) + sets (#145) ---
	{
		CHECK(plasticUnlocked(PlasticColor::ClassicGreen, 0, 0), "base color free");
		CHECK(!plasticUnlocked(PlasticColor::GoldChrome, 100, 0), "gold chrome needs wins");
		CHECK(plasticUnlocked(PlasticColor::GoldChrome, 0, 8), "gold chrome at 8 wins");
		CHECK(!towerSkinUnlocked(TowerSkin::DiceTower, 0, 0), "dice tower locked at start");
		CHECK(towerSkinUnlocked(TowerSkin::DiceTower, 0, 4), "dice tower at 4 wins");
		CHECK(!accessoryUnlocked(Accessory::Flag, 0, 4), "flag needs 5 wins");
		char txt[48];
		plasticUnlockText(PlasticColor::GoldChrome, txt, sizeof(txt));
		CHECK(std::strstr(txt, "win") != nullptr, "unlock rule text");
		CHECK(cosmeticSetCount() >= 5, "at least 5 cosmetic sets");
		for (int i = 0; i < cosmeticSetCount(); ++i) {
			const CosmeticSet& s = cosmeticSet(i);
			CHECK(s.name && s.name[0], "set named");
			CHECK(static_cast<int>(s.plastic) < plasticColorCount(), "set plastic valid");
			CHECK(static_cast<int>(s.towerSkin) < towerSkinCount(), "set skin valid");
			CHECK(static_cast<int>(s.accessory) < accessoryCount(), "set accessory valid");
		}
		CHECK(cosmeticSetUnlocked(0, 0, 0), "Birthday Platoon free (base items)");
		std::printf("OK unlocks + sets\n");
	}

	// --- Snapshot carries new map + cosmetics values ---
	{
		Match m;
		MatchConfig cfg;
		cfg.seed = 88;
		cfg.mapId = MapId::ChristmasTable;
		startMatch(m, cfg);
		m.players[0].cosmetics.plastic = PlasticColor::GoldChrome;
		m.players[0].cosmetics.towerSkin = TowerSkin::DiceTower;
		m.players[0].cosmetics.accessory = Accessory::Flag;
		const auto bytes = serializeMatch(m);
		Match m2;
		CHECK(deserializeMatch(m2, bytes.data(), bytes.size()), "deserialize");
		CHECK(m2.config.mapId == MapId::ChristmasTable, "new map survives");
		CHECK(m2.players[0].cosmetics.plastic == PlasticColor::GoldChrome, "new plastic survives");
		CHECK(m2.players[0].cosmetics.towerSkin == TowerSkin::DiceTower, "new skin survives");
		CHECK(m2.players[0].cosmetics.accessory == Accessory::Flag, "new accessory survives");
		std::printf("OK snapshot with v0.9 content\n");
	}

	if (g_fails) {
		std::printf("%d failure(s)\n", g_fails);
		return 1;
	}
	std::printf("OK v0.9 content tests\n");
	return 0;
}
