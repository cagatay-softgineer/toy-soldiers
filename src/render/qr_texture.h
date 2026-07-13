#pragma once

#include "sokol_gfx.h"

namespace toy {

// v1.2 #90: QR code rendered as a sokol-gfx texture for the host lobby (local
// network only — the code just encodes the room code + port as text; there is
// no phone client, this is a scan-with-any-QR-app convenience for photographing
// the join info off a shared screen).
struct QrTexture {
	sg_image image{};
	sg_view view{};
	int pixelSize = 0;
	bool valid = false;
};

// (Re)builds `out` to encode `text`. Destroys any previously held GPU resources
// first, so it's safe to call every frame with a cached "did the text change"
// check by the caller. Returns false (and leaves `out` empty) if `text` is too
// long to encode.
bool qrTextureBuild(const char* text, QrTexture& out);
void qrTextureDestroy(QrTexture& out);

} // namespace toy
