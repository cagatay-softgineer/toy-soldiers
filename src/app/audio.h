#pragma once

#include "game/types.h"

namespace toy {

// v0.9 #130-#135: procedural toy-box audio — no asset files, everything synthesized.
// Safe to call every function even if audioInit failed (silently no-ops).

enum class Sfx : uint8_t {
	CardPlay,
	Damage,
	BigDamage, // >= 5 dmg
	Shield,
	Draw,
	TowerDestroy,
	UiClick,
	IllegalBuzz,
	YourTurn,
	Win,
};

enum class MusicTrack : uint8_t {
	None,
	Menu,
	Match,
	ResultsSting, // one-shot, then silence
};

bool audioInit();
void audioShutdown();
bool audioReady();

// Volumes 0..1 (#134). Applied immediately.
void audioSetVolumes(float master, float sfx, float music);
// v1.0 #199: duck everything to ~15% while the window is unfocused.
void audioSetDucked(bool ducked);

void sfxPlay(Sfx s, float pan = 0.0f); // pan: -1 left .. +1 right (#135)
// #132: short stinger themed per world event.
void sfxEventStinger(EventKind kind, float pan = 0.0f);
// #133: switch background loop (no-op if already playing).
void musicPlay(MusicTrack t);

} // namespace toy
