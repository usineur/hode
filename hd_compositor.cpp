/*
 * Heart of Darkness engine rewrite
 * HD rendering compositor with multi-resolution and 16:9 dynamic borders
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "hd_compositor.h"
#include "fileio.h"
#include "resource.h"
#include "sprite_upscaler.h"
#include "video.h"

HdCompositor::HdCompositor(int scale, const char *cachePath) {
	_scale = scale;

	// Compute actual scale from chain (e.g. scale=6 -> chain 3*2 = 6)
	int first, second;
	SpriteUpscaler::getScaleChain(scale, &first, &second);
	const int actualScale = first * (second > 1 ? second : 1);

	_hdW = Video::W * actualScale;
	_hdH = Video::H * actualScale;
	_hdFramebuffer = (uint32_t *)calloc(_hdW * _hdH, sizeof(uint32_t));
	_hdBackground = (uint32_t *)calloc(_hdW * _hdH, sizeof(uint32_t));
	_wideFramebuffer = 0;
	_wideW = 0;
	_wideH = 0;
	_widescreenEnabled = false;
	_enabled = false;
	_upscaler = new SpriteUpscaler(scale, cachePath);
	_cachedScreenNum = -1;
	_cachedBackgroundId = -1;
	memset(_palette, 0, sizeof(_palette));
	_paletteValid = false;
	_borderColorTop = 0;
	_borderColorBottom = 0;
	_borderColorAvg = 0;
	_progress = 0;
}

HdCompositor::~HdCompositor() {
	free(_hdFramebuffer);
	free(_hdBackground);
	free(_wideFramebuffer);
	delete _upscaler;
}

void HdCompositor::enableWidescreen(bool on) {
	_widescreenEnabled = on;
	if (on && !_wideFramebuffer) {
		// 16:9 from same height: width = height * 16/9
		_wideW = _hdH * 16 / 9;
		_wideH = _hdH;
		_wideFramebuffer = (uint32_t *)calloc(_wideW * _wideH, sizeof(uint32_t));
	}
}

int HdCompositor::scaleForResolution(int targetW, int targetH) {
	// Find the largest scale where both dimensions fit
	// Game is 256x192 (4:3)
	const int scaleW = targetW / Video::W;
	const int scaleH = targetH / Video::H;
	int scale = (scaleW < scaleH) ? scaleW : scaleH;
	if (scale < 2) scale = 2;
	if (scale > 16) scale = 16;
	return scale;
}

void HdCompositor::updatePalette(const uint8_t *pal, int n, int depth) {
	const int shift = 8 - depth;
	for (int i = 0; i < n && i < 256; ++i) {
		int r = pal[i * 3 + 0];
		int g = pal[i * 3 + 1];
		int b = pal[i * 3 + 2];
		if (shift != 0) {
			r = (r << shift) | (r >> (depth - shift));
			g = (g << shift) | (g >> (depth - shift));
			b = (b << shift) | (b >> (depth - shift));
		}
		_palette[i] = (r << 16) | (g << 8) | b;
	}
	_paletteValid = true;
	_cachedScreenNum = -1;
}

void HdCompositor::computeBorderColors(const uint8_t *bgLayer) {
	// Sample edge pixels to compute border colors
	// Top edge: average of top row
	long rT = 0, gT = 0, bT = 0;
	for (int x = 0; x < Video::W; ++x) {
		const uint32_t c = _palette[bgLayer[x]];
		rT += (c >> 16) & 0xFF;
		gT += (c >> 8) & 0xFF;
		bT += c & 0xFF;
	}
	rT /= Video::W; gT /= Video::W; bT /= Video::W;
	_borderColorTop = (rT << 16) | (gT << 8) | bT;

	// Bottom edge
	long rB = 0, gB = 0, bB = 0;
	const int bottomRow = (Video::H - 1) * Video::W;
	for (int x = 0; x < Video::W; ++x) {
		const uint32_t c = _palette[bgLayer[bottomRow + x]];
		rB += (c >> 16) & 0xFF;
		gB += (c >> 8) & 0xFF;
		bB += c & 0xFF;
	}
	rB /= Video::W; gB /= Video::W; bB /= Video::W;
	_borderColorBottom = (rB << 16) | (gB << 8) | bB;

	// Overall average (darken by 50% for subtle borders)
	const long rA = (rT + rB) / 4;
	const long gA = (gT + gB) / 4;
	const long bA = (bT + bB) / 4;
	_borderColorAvg = (rA << 16) | (gA << 8) | bA;
}

void HdCompositor::beginFrame(const uint8_t *bgLayer, const uint8_t *palette,
	int screenNum, int backgroundId)
{
	if (!_enabled) return;

	// The engine's palette is 6-bit per channel (values 0-63), so naively
	// assigning the bytes leaves _palette ~4x dimmer than intended. That made
	// computeBorderColors() sample dark values, and compositeWidescreen()'s
	// extra /3 darkening dropped the bars to ~black. Expand 6 -> 8 bits here.
	if (palette) {
		for (int i = 0; i < 256; ++i) {
			int r = palette[i*3 + 0] & 0x3F;
			int g = palette[i*3 + 1] & 0x3F;
			int b = palette[i*3 + 2] & 0x3F;
			r = (r << 2) | (r >> 4);
			g = (g << 2) | (g >> 4);
			b = (b << 2) | (b >> 4);
			_palette[i] = (r << 16) | (g << 8) | b;
		}
		_paletteValid = true;
	}

	if (screenNum != _cachedScreenNum || backgroundId != _cachedBackgroundId) {
		upscaleAndCacheBackground(bgLayer, palette);
		if (_widescreenEnabled) {
			computeBorderColors(bgLayer);
		}
		_cachedScreenNum = screenNum;
		_cachedBackgroundId = backgroundId;
	}

	memcpy(_hdFramebuffer, _hdBackground, _hdW * _hdH * sizeof(uint32_t));
}

void HdCompositor::upscaleAndCacheBackground(const uint8_t *bgLayer,
	const uint8_t *palette)
{
	_upscaler->upscaleBackground(bgLayer, Video::W, Video::H,
		_palette, _hdBackground, _hdW, _hdH);
}

void HdCompositor::drawSprite(const uint8_t *bitmapBits, int x, int y,
	uint16_t w, uint16_t h, uint8_t flags)
{
	if (!_enabled || !bitmapBits) return;

	const HdSprite *hd = _upscaler->getOrUpscale(bitmapBits, w, h, flags, _palette);
	if (!hd) return;

	// Scale position, accounting for actual vs requested scale
	const int dstX = x * _hdW / Video::W;
	const int dstY = y * _hdH / Video::H;
	blitHdSprite(hd->pixels, hd->width, hd->height, dstX, dstY);
}

void HdCompositor::blitHdSprite(const uint32_t *pixels, int sprW, int sprH,
	int dstX, int dstY)
{
	int srcX = 0, srcY = 0;
	int drawW = sprW, drawH = sprH;

	if (dstX < 0) { srcX = -dstX; drawW += dstX; dstX = 0; }
	if (dstY < 0) { srcY = -dstY; drawH += dstY; dstY = 0; }
	if (dstX + drawW > _hdW) drawW = _hdW - dstX;
	if (dstY + drawH > _hdH) drawH = _hdH - dstY;
	if (drawW <= 0 || drawH <= 0) return;

	for (int j = 0; j < drawH; ++j) {
		const uint32_t *srcRow = pixels + (srcY + j) * sprW + srcX;
		uint32_t *dstRow = _hdFramebuffer + (dstY + j) * _hdW + dstX;
		for (int i = 0; i < drawW; ++i) {
			const uint32_t px = srcRow[i];
			if ((px >> 24) != 0) {
				dstRow[i] = px;
			}
		}
	}
}

void HdCompositor::compositeWidescreen() {
	if (!_wideFramebuffer) return;

	const int borderW = (_wideW - _hdW) / 2;

	// Fill entire buffer with gradient from border colors
	for (int y = 0; y < _wideH; ++y) {
		// Vertical gradient: top color -> bottom color
		const float t = (float)y / (float)(_wideH - 1);
		const int rT = (_borderColorTop >> 16) & 0xFF;
		const int gT = (_borderColorTop >> 8) & 0xFF;
		const int bT = _borderColorTop & 0xFF;
		const int rB = (_borderColorBottom >> 16) & 0xFF;
		const int gB = (_borderColorBottom >> 8) & 0xFF;
		const int bB = _borderColorBottom & 0xFF;
		const int r = rT + (int)((rB - rT) * t);
		const int g = gT + (int)((gB - gT) * t);
		const int b = bT + (int)((bB - bT) * t);
		// Darken for subtlety
		const uint32_t borderColor = ((r/3) << 16) | ((g/3) << 8) | (b/3);

		uint32_t *row = _wideFramebuffer + y * _wideW;

		// Left border
		for (int x = 0; x < borderW; ++x) {
			// Fade gradient: darker at edge, brighter near game
			const float edgeFade = (float)x / (float)borderW;
			const int fr = (int)(((borderColor >> 16) & 0xFF) * edgeFade);
			const int fg = (int)(((borderColor >> 8) & 0xFF) * edgeFade);
			const int fb = (int)((borderColor & 0xFF) * edgeFade);
			row[x] = (fr << 16) | (fg << 8) | fb;
		}

		// Game area
		memcpy(row + borderW, _hdFramebuffer + y * _hdW,
			_hdW * sizeof(uint32_t));

		// Right border (mirror of left)
		for (int x = 0; x < borderW; ++x) {
			const float edgeFade = (float)(borderW - 1 - x) / (float)borderW;
			const int fr = (int)(((borderColor >> 16) & 0xFF) * edgeFade);
			const int fg = (int)(((borderColor >> 8) & 0xFF) * edgeFade);
			const int fb = (int)((borderColor & 0xFF) * edgeFade);
			row[borderW + _hdW + x] = (fr << 16) | (fg << 8) | fb;
		}
	}
}

void HdCompositor::endFrame() {
	if (_widescreenEnabled) {
		compositeWidescreen();
	}
}

void HdCompositor_drawProgressBar(const char *label, int done, int total, double elapsed) {
	const int width = 32;
	const int filled = total > 0 ? (done * width) / total : width;
	const int pct = total > 0 ? (done * 100) / total : 100;
	char bar[64];
	for (int i = 0; i < width; ++i) bar[i] = (i < filled) ? '#' : '.';
	bar[width] = 0;
	double eta = 0.0;
	if (done > 0 && done < total && elapsed > 0.0) {
		eta = elapsed * (total - done) / done;
	}
	fprintf(stderr, "\r%s [%s] %d/%d (%d%%) %.1fs eta %.1fs   ",
		label ? label : "prerender", bar, done, total, pct, elapsed, eta);
	fflush(stderr);
}

struct WalkCtx {
	LvlObjectData *const *table;
	int tableSize;
};

static int collectWalks(Resource *res, WalkCtx *walks, int maxWalks) {
	int walkCount = 0;
	if (walkCount < maxWalks) {
		walks[walkCount].table = res->_resLevelData0x2988PtrTable;
		walks[walkCount].tableSize = (int)kMaxSpriteTypes;
		++walkCount;
	}
	for (int s = 0; s < (int)res->_lvlHdr.screensCount && s < (int)kMaxScreens; ++s) {
		if (walkCount >= maxWalks) break;
		walks[walkCount].table = res->_resLvlScreenBackgroundDataTable[s].backgroundLvlObjectDataTable;
		walks[walkCount].tableSize = 8;
		++walkCount;
	}
	return walkCount;
}

int HdCompositor::countLevelSpriteFrames(Resource *res) {
	if (!res) return 0;
	WalkCtx walks[1 + kMaxScreens];
	const int walkCount = collectWalks(res, walks, 1 + kMaxScreens);
	int total = 0;
	for (int w = 0; w < walkCount; ++w) {
		for (int i = 0; i < walks[w].tableSize; ++i) {
			LvlObjectData *dat = walks[w].table[i];
			if (!dat) continue;
			total += dat->framesCount * 2;
		}
	}
	return total;
}

void HdCompositor::prerenderLevelSprites(Resource *res, const char *label) {
	if (!_enabled || !_upscaler || !res) return;

	WalkCtx walks[1 + kMaxScreens];
	const int walkCount = collectWalks(res, walks, 1 + kMaxScreens);

	const int spriteTotal = countLevelSpriteFrames(res);
	if (spriteTotal == 0) return;

	const bool externalProgress = (_progress != 0);
	struct timespec t0;
	int localTotal = spriteTotal, localDone = 0, lastBar = -1;
	if (!externalProgress) {
		clock_gettime(CLOCK_MONOTONIC, &t0);
		HdCompositor_drawProgressBar(label, 0, localTotal, 0.0);
	}
	for (int w = 0; w < walkCount; ++w) {
		for (int i = 0; i < walks[w].tableSize; ++i) {
			LvlObjectData *dat = walks[w].table[i];
			if (!dat) continue;
			for (int f = 0; f < dat->framesCount; ++f) {
				for (int flip = 0; flip < 2; ++flip) {
					uint16_t fw = 0, fh = 0;
					const uint8_t *sprData = res->getLvlSpriteFramePtr(dat, f, &fw, &fh);
					if (sprData && fw > 0 && fh > 0) {
						_upscaler->getOrUpscale(sprData, fw, fh, (uint8_t)flip, _palette);
					}
					if (externalProgress) {
						++_progress->done;
						const int bar = _progress->done * 64 / _progress->total;
						if (bar != _progress->lastBar) {
							struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
							const double elapsed = (t1.tv_sec - _progress->t0.tv_sec) +
								(t1.tv_nsec - _progress->t0.tv_nsec) / 1e9;
							HdCompositor_drawProgressBar(_progress->label,
								_progress->done, _progress->total, elapsed);
							_progress->lastBar = bar;
						}
					} else {
						++localDone;
						const int bar = localDone * 64 / localTotal;
						if (bar != lastBar) {
							struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
							const double elapsed = (t1.tv_sec - t0.tv_sec) +
								(t1.tv_nsec - t0.tv_nsec) / 1e9;
							HdCompositor_drawProgressBar(label, localDone, localTotal, elapsed);
							lastBar = bar;
						}
					}
				}
			}
		}
	}
	if (!externalProgress) {
		struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
		const double elapsed = (t1.tv_sec - t0.tv_sec) +
			(t1.tv_nsec - t0.tv_nsec) / 1e9;
		HdCompositor_drawProgressBar(label, localTotal, localTotal, elapsed);
		fprintf(stderr, "\n");
	}
}

void HdCompositor::getFramebuffer(uint32_t **buf, int *w, int *h) {
	if (_widescreenEnabled && _wideFramebuffer) {
		*buf = _wideFramebuffer;
		*w = _wideW;
		*h = _wideH;
	} else {
		*buf = _hdFramebuffer;
		*w = _hdW;
		*h = _hdH;
	}
}
