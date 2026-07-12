#pragma once

#include "game/types.h"

#include <span>
#include <vector>

namespace toy {

// v0.9 catalog — 30 defs across Attack / Defense / Tactic / Event with keywords,
// optionally overridden from data/cards.json (#148).
std::span<const CardDef> cardCatalog();
const CardDef* findCard(int defId);

// v0.9 #153: localized description (falls back to EN when no TR text).
const char* cardDescription(const CardDef& def);

// --- v0.9 #148/#149: data-driven catalog ---
// Replace the catalog from a JSON file/text. Returns false (and keeps the current
// catalog) when the data is missing or clearly broken (< 10 valid cards).
bool cardsLoadJsonFile(const char* path);
bool cardsLoadJsonText(const char* json);
// Restore the compiled-in catalog (tests / hot-reload fallback).
void cardsResetBuiltins();
// Write the current catalog as cards.json (bootstraps the data file).
bool cardsExportJson(const char* path);
// Debug hot-reload: re-load when the file's mtime changed. Returns true on reload.
bool cardsReloadIfChanged(const char* path);

// Build a shuffled starter deck for one player. Mode picks the recipe (Quick Duel = 15 cards);
// the player's tower injects a signature card and deck-builder bans/extras apply (#41, #47).
std::vector<CardInstance> makeStarterDeck(int playerId, uint32_t& rngState, int& nextInstanceId, GameMode mode,
										  const Player& forPlayer);

// xorshift32 — deterministic, no heap, good enough for jam prototypes.
uint32_t xorshift32(uint32_t& state);
int randRange(uint32_t& state, int lo, int hiInclusive);

void shuffleInPlace(std::vector<CardInstance>& cards, uint32_t& rngState);

} // namespace toy
