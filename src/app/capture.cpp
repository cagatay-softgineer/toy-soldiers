#include "app/capture.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h> // IStream, required before gdiplus on lean builds
#include <gdiplus.h>

#define MSF_GIF_IMPL
#include "msf_gif.h" // vendored, extern/msf_gif (#77)

#include <string>
#include <vector>

namespace toy {

namespace {

constexpr int kRingFrames = 24;   // 3 seconds at 8 fps
constexpr float kRingStep = 0.125f;
constexpr int kRingDiv = 4; // quarter resolution keeps the ring ~10 MB worst case

struct RingFrame {
	std::vector<uint8_t> rgba;
	int w = 0;
	int h = 0;
	bool valid = false;
};

RingFrame g_ring[kRingFrames];
int g_ringHead = 0;
float g_ringAccum = 0.0f;

ULONG_PTR g_gdiplusToken = 0;

bool ensureGdiplus()
{
	if (g_gdiplusToken) {
		return true;
	}
	Gdiplus::GdiplusStartupInput input;
	return Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr) == Gdiplus::Ok;
}

// CLSID of the PNG encoder.
bool pngClsid(CLSID& out)
{
	UINT num = 0, size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0) {
		return false;
	}
	std::vector<uint8_t> buf(size);
	auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
	Gdiplus::GetImageEncoders(num, size, codecs);
	for (UINT i = 0; i < num; ++i) {
		if (wcscmp(codecs[i].MimeType, L"image/png") == 0) {
			out = codecs[i].Clsid;
			return true;
		}
	}
	return false;
}

// Grab the foreground game window client area as a 32-bit RGBA buffer.
// Uses PrintWindow(..., PW_RENDERFULLCONTENT) rather than BitBlt-from-GetDC: the
// window renders through a D3D11/DXGI swapchain, and a plain GDI BitBlt against the
// window's own DC is unreliable (often blank/stale) with modern flip-model swap
// chains. PrintWindow explicitly asks DWM to composite the real, current frame.
// divisor > 1 downsamples the full-res capture afterward (ring frames).
bool grabClient(int divisor, std::vector<uint8_t>& rgbaOut, int& wOut, int& hOut)
{
	HWND wnd = GetActiveWindow();
	if (!wnd) {
		wnd = GetForegroundWindow();
	}
	if (!wnd) {
		return false;
	}
	RECT rc{};
	if (!GetClientRect(wnd, &rc)) {
		return false;
	}
	const int srcW = rc.right - rc.left;
	const int srcH = rc.bottom - rc.top;
	if (srcW < 8 || srcH < 8) {
		return false;
	}

	HDC winDc = GetDC(wnd);
	HDC fullDc = CreateCompatibleDC(winDc);
	BITMAPINFO fullBmi{};
	fullBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	fullBmi.bmiHeader.biWidth = srcW;
	fullBmi.bmiHeader.biHeight = -srcH; // top-down
	fullBmi.bmiHeader.biPlanes = 1;
	fullBmi.bmiHeader.biBitCount = 32;
	fullBmi.bmiHeader.biCompression = BI_RGB;
	void* fullBits = nullptr;
	HBITMAP fullDib = CreateDIBSection(fullDc, &fullBmi, DIB_RGB_COLORS, &fullBits, nullptr, 0);
	if (!fullDib || !fullBits) {
		if (fullDib) {
			DeleteObject(fullDib);
		}
		DeleteDC(fullDc);
		ReleaseDC(wnd, winDc);
		return false;
	}
	HGDIOBJ oldFull = SelectObject(fullDc, fullDib);

	constexpr UINT kPwRenderFullContent = 0x00000002; // PW_RENDERFULLCONTENT (Win 8.1+)
	BOOL painted = PrintWindow(wnd, fullDc, kPwRenderFullContent);
	if (!painted) {
		painted = PrintWindow(wnd, fullDc, 0); // older-OS fallback
	}
	GdiFlush();
	ReleaseDC(wnd, winDc);
	if (!painted) {
		SelectObject(fullDc, oldFull);
		DeleteObject(fullDib);
		DeleteDC(fullDc);
		return false;
	}

	const int dstW = srcW / divisor;
	const int dstH = srcH / divisor;
	const void* srcBits = fullBits;
	HDC smallDc{};
	HBITMAP smallDib{};
	if (divisor > 1) {
		smallDc = CreateCompatibleDC(fullDc);
		BITMAPINFO smallBmi{};
		smallBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		smallBmi.bmiHeader.biWidth = dstW;
		smallBmi.bmiHeader.biHeight = -dstH;
		smallBmi.bmiHeader.biPlanes = 1;
		smallBmi.bmiHeader.biBitCount = 32;
		smallBmi.bmiHeader.biCompression = BI_RGB;
		void* smallBits = nullptr;
		smallDib = CreateDIBSection(smallDc, &smallBmi, DIB_RGB_COLORS, &smallBits, nullptr, 0);
		if (smallDib && smallBits) {
			HGDIOBJ oldSmall = SelectObject(smallDc, smallDib);
			SetStretchBltMode(smallDc, HALFTONE);
			// Plain GDI-surface-to-GDI-surface StretchBlt — no swapchain involved here,
			// so this part is unaffected by the flip-model caveat above.
			StretchBlt(smallDc, 0, 0, dstW, dstH, fullDc, 0, 0, srcW, srcH, SRCCOPY);
			GdiFlush();
			srcBits = smallBits;
			SelectObject(smallDc, oldSmall);
		} else {
			smallDib = nullptr; // fall through to full-res on allocation failure
		}
	}

	const int outW = smallDib ? dstW : srcW;
	const int outH = smallDib ? dstH : srcH;
	rgbaOut.resize(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4);
	const uint8_t* src = static_cast<const uint8_t*>(srcBits);
	for (size_t i = 0; i < rgbaOut.size(); i += 4) {
		rgbaOut[i + 0] = src[i + 2]; // BGRA -> RGBA
		rgbaOut[i + 1] = src[i + 1];
		rgbaOut[i + 2] = src[i + 0];
		rgbaOut[i + 3] = 0xFF;
	}
	wOut = outW;
	hOut = outH;

	if (smallDib) {
		DeleteObject(smallDib);
	}
	if (smallDc) {
		DeleteDC(smallDc);
	}
	SelectObject(fullDc, oldFull);
	DeleteObject(fullDib);
	DeleteDC(fullDc);
	return true;
}

// v1.2 #184: center-crop an RGBA buffer down to a 9:16 portrait frame by
// trimming width only (the capture window is landscape, so height is already
// the limiting dimension). Byte order is irrelevant here — it's a per-row
// block copy, so this works whether callers hand it RGBA or BGRA.
std::vector<uint8_t> cropCenterVertical(const uint8_t* src, int srcW, int srcH, int& outW, int& outH)
{
	int cropW = (srcH * 9) / 16;
	if (cropW <= 0 || cropW > srcW) {
		cropW = srcW; // degenerate window shape: best effort, not true 9:16
	}
	const int x0 = (srcW - cropW) / 2;
	std::vector<uint8_t> out(static_cast<size_t>(cropW) * static_cast<size_t>(srcH) * 4);
	for (int y = 0; y < srcH; ++y) {
		const uint8_t* srow = src + (static_cast<size_t>(y) * srcW + x0) * 4;
		uint8_t* drow = out.data() + static_cast<size_t>(y) * cropW * 4;
		std::memcpy(drow, srow, static_cast<size_t>(cropW) * 4);
	}
	outW = cropW;
	outH = srcH;
	return out;
}

std::wstring widen(const char* s)
{
	std::wstring out;
	const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
	if (n > 0) {
		out.resize(static_cast<size_t>(n - 1));
		MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), n);
	}
	return out;
}

} // namespace

bool captureWindowPng(const char* path)
{
	if (!path || !ensureGdiplus()) {
		return false;
	}
	std::vector<uint8_t> rgba;
	int w = 0, h = 0;
	if (!grabClient(1, rgba, w, h)) {
		return false;
	}
	// GDI+ 32bppARGB expects BGRA byte order; grabClient handed us RGBA — swap back.
	for (size_t i = 0; i < rgba.size(); i += 4) {
		const uint8_t r = rgba[i];
		rgba[i] = rgba[i + 2];
		rgba[i + 2] = r;
	}
	Gdiplus::Bitmap bmp2(w, h, w * 4, PixelFormat32bppARGB, rgba.data());
	CLSID clsid;
	if (!pngClsid(clsid)) {
		return false;
	}
	const std::wstring wpath = widen(path);
	return bmp2.Save(wpath.c_str(), &clsid, nullptr) == Gdiplus::Ok;
}

bool captureWindowPngVertical(const char* path)
{
	if (!path || !ensureGdiplus()) {
		return false;
	}
	std::vector<uint8_t> rgba;
	int w = 0, h = 0;
	if (!grabClient(1, rgba, w, h)) {
		return false;
	}
	int cw = 0, ch = 0;
	std::vector<uint8_t> cropped = cropCenterVertical(rgba.data(), w, h, cw, ch);
	// GDI+ 32bppARGB expects BGRA byte order; grabClient handed us RGBA — swap back.
	for (size_t i = 0; i < cropped.size(); i += 4) {
		const uint8_t r = cropped[i];
		cropped[i] = cropped[i + 2];
		cropped[i + 2] = r;
	}
	Gdiplus::Bitmap bmp2(cw, ch, cw * 4, PixelFormat32bppARGB, cropped.data());
	CLSID clsid;
	if (!pngClsid(clsid)) {
		return false;
	}
	const std::wstring wpath = widen(path);
	return bmp2.Save(wpath.c_str(), &clsid, nullptr) == Gdiplus::Ok;
}

void captureRingTick(float dt)
{
	g_ringAccum += dt;
	if (g_ringAccum < kRingStep) {
		return;
	}
	g_ringAccum = 0.0f;
	RingFrame& f = g_ring[g_ringHead];
	f.valid = grabClient(kRingDiv, f.rgba, f.w, f.h);
	g_ringHead = (g_ringHead + 1) % kRingFrames;
}

void captureRingReset()
{
	for (RingFrame& f : g_ring) {
		f.valid = false;
	}
	g_ringHead = 0;
	g_ringAccum = 0.0f;
}

bool captureRingSaveGif(const char* path)
{
	// Find dimensions from the newest valid frame; skip mismatched ones (resizes).
	int w = 0, h = 0, count = 0;
	for (int k = 0; k < kRingFrames; ++k) {
		const RingFrame& f = g_ring[(g_ringHead + kRingFrames - 1 - k) % kRingFrames];
		if (f.valid) {
			w = f.w;
			h = f.h;
			break;
		}
	}
	if (w == 0 || h == 0) {
		return false;
	}
	MsfGifState gif = {};
	if (!msf_gif_begin(&gif, w, h)) {
		return false;
	}
	for (int k = 0; k < kRingFrames; ++k) {
		RingFrame& f = g_ring[(g_ringHead + k) % kRingFrames]; // oldest first
		if (!f.valid || f.w != w || f.h != h) {
			continue;
		}
		msf_gif_frame(&gif, f.rgba.data(), 13, 16, w * 4); // ~8fps (13 centiseconds)
		++count;
	}
	MsfGifResult res = msf_gif_end(&gif);
	bool ok = false;
	if (res.data && count > 0) {
		FILE* fp = std::fopen(path, "wb");
		if (fp) {
			ok = std::fwrite(res.data, 1, res.dataSize, fp) == res.dataSize;
			std::fclose(fp);
		}
	}
	msf_gif_free(res);
	return ok;
}

bool captureRingSaveGifVertical(const char* path)
{
	int w = 0, h = 0, count = 0;
	for (int k = 0; k < kRingFrames; ++k) {
		const RingFrame& f = g_ring[(g_ringHead + kRingFrames - 1 - k) % kRingFrames];
		if (f.valid) {
			w = f.w;
			h = f.h;
			break;
		}
	}
	if (w == 0 || h == 0) {
		return false;
	}
	int cw = 0, ch = 0;
	MsfGifState gif = {};
	for (int k = 0; k < kRingFrames; ++k) {
		RingFrame& f = g_ring[(g_ringHead + k) % kRingFrames]; // oldest first
		if (!f.valid || f.w != w || f.h != h) {
			continue;
		}
		std::vector<uint8_t> cropped = cropCenterVertical(f.rgba.data(), f.w, f.h, cw, ch);
		if (count == 0 && !msf_gif_begin(&gif, cw, ch)) {
			return false;
		}
		msf_gif_frame(&gif, cropped.data(), 13, 16, cw * 4); // ~8fps (13 centiseconds)
		++count;
	}
	MsfGifResult res = msf_gif_end(&gif);
	bool ok = false;
	if (res.data && count > 0) {
		FILE* fp = std::fopen(path, "wb");
		if (fp) {
			ok = std::fwrite(res.data, 1, res.dataSize, fp) == res.dataSize;
			std::fclose(fp);
		}
	}
	msf_gif_free(res);
	return ok;
}

bool captureMakePath(const char* prefix, const char* ext, char* out, int outCap)
{
	const char* appdata = std::getenv("APPDATA");
	char dir[512];
	if (appdata && appdata[0]) {
		std::snprintf(dir, sizeof(dir), "%s\\toy-soldiers\\screenshots", appdata);
	} else {
		std::snprintf(dir, sizeof(dir), "screenshots");
	}
	CreateDirectoryA(dir, nullptr);
	const time_t now = time(nullptr);
	tm tmv{};
	localtime_s(&tmv, &now);
	char stamp[32];
	std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tmv);
	std::snprintf(out, static_cast<size_t>(outCap), "%s\\%s-%s.%s", dir, prefix ? prefix : "shot", stamp,
				  ext ? ext : "png");
	return true;
}

} // namespace toy

#else // !_WIN32 — capture is a Windows-only nicety; keep the API safe elsewhere.

namespace toy {

bool captureWindowPng(const char*)
{
	return false;
}
bool captureWindowPngVertical(const char*)
{
	return false;
}
void captureRingTick(float) {}
bool captureRingSaveGif(const char*)
{
	return false;
}
bool captureRingSaveGifVertical(const char*)
{
	return false;
}
void captureRingReset() {}
bool captureMakePath(const char*, const char*, char*, int)
{
	return false;
}

} // namespace toy

#endif
