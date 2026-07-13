#pragma once

// v1.2 capture stack (#77 GIF burst, #188 result card, #179 press tour).
// Windows-only (GDI/GDI+); every function is a safe no-op elsewhere.

namespace toy {

// One-shot PNG of the game window's client area. path = full output file.
bool captureWindowPng(const char* path);

// v1.2 #184: same one-shot PNG, center-cropped to a 9:16 portrait frame (TikTok/
// Shorts/Reels all share this ratio, so one preset covers them).
bool captureWindowPngVertical(const char* path);

// Rolling low-res ring of the last ~3 seconds (#77). Call tick every frame while
// capture is desired; saveGif flushes the ring through msf_gif.
void captureRingTick(float dt);
bool captureRingSaveGif(const char* path);
// v1.2 #184: 9:16 center-cropped export of the same ring buffer.
bool captureRingSaveGifVertical(const char* path);
void captureRingReset();

// %APPDATA%/toy-soldiers/screenshots/<prefix>-YYYYMMDD-HHMMSS.<ext> (dir auto-created).
// Returns false if the path could not be built.
bool captureMakePath(const char* prefix, const char* ext, char* out, int outCap);

} // namespace toy
