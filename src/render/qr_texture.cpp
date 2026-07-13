#include "render/qr_texture.h"

#include "qrcodegen.h"

#include <cstdint>
#include <vector>

namespace toy {

namespace {
constexpr int kModulePixels = 6; // upscale factor per QR module, so it's readable on a 1440p screen
constexpr int kQuietZone = 4;    // modules of white border required by the QR spec
constexpr int kMaxVersion = 10;  // room-code strings are short; caps the mask-search cost
} // namespace

void qrTextureDestroy(QrTexture& out)
{
	if (out.view.id) {
		sg_destroy_view(out.view);
	}
	if (out.image.id) {
		sg_destroy_image(out.image);
	}
	out = QrTexture{};
}

bool qrTextureBuild(const char* text, QrTexture& out)
{
	qrTextureDestroy(out);

	uint8_t tmp[qrcodegen_BUFFER_LEN_MAX];
	uint8_t qr[qrcodegen_BUFFER_LEN_MAX];
	if (!qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, kMaxVersion,
							   qrcodegen_Mask_AUTO, true)) {
		return false;
	}

	const int modules = qrcodegen_getSize(qr);
	const int side = (modules + kQuietZone * 2) * kModulePixels;
	std::vector<uint8_t> pixels(static_cast<size_t>(side) * static_cast<size_t>(side) * 4, 0xFF);

	for (int y = 0; y < modules; ++y) {
		for (int x = 0; x < modules; ++x) {
			if (!qrcodegen_getModule(qr, x, y)) {
				continue;
			}
			const int px0 = (x + kQuietZone) * kModulePixels;
			const int py0 = (y + kQuietZone) * kModulePixels;
			for (int py = 0; py < kModulePixels; ++py) {
				uint8_t* row = &pixels[(static_cast<size_t>(py0 + py) * side + px0) * 4];
				for (int px = 0; px < kModulePixels; ++px) {
					row[px * 4 + 0] = 0;
					row[px * 4 + 1] = 0;
					row[px * 4 + 2] = 0;
				}
			}
		}
	}

	sg_image_desc idesc{};
	idesc.width = side;
	idesc.height = side;
	idesc.pixel_format = SG_PIXELFORMAT_RGBA8;
	idesc.data.mip_levels[0].ptr = pixels.data();
	idesc.data.mip_levels[0].size = pixels.size();
	idesc.label = "qr-room-code";
	out.image = sg_make_image(&idesc);
	if (out.image.id == 0) {
		return false;
	}

	sg_view_desc vdesc{};
	vdesc.texture.image = out.image;
	out.view = sg_make_view(&vdesc);
	if (out.view.id == 0) {
		sg_destroy_image(out.image);
		out.image = sg_image{};
		return false;
	}

	out.pixelSize = side;
	out.valid = true;
	return true;
}

} // namespace toy
