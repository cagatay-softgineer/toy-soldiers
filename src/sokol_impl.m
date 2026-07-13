// v1.2 #161: Objective-C twin of sokol_impl.c. sokol_gfx.h's Metal backend and
// sokol_app.h's macOS/iOS backend use `id<...>` / `__bridge` / message-send
// syntax that only compiles under Objective-C — plain .c can't build this on
// Apple. Content is otherwise identical; CMake selects this file on APPLE.
//
// Backend define comes from the sokol INTERFACE target (CMake).
#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
// v0.9: audio (vendored separately under extern/sokol — WASAPI/ALSA/CoreAudio backend).
#include "sokol_audio.h"
