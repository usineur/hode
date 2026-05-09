/*
 * Heart of Darkness engine rewrite
 * Sprite upscaling with xBRZ and disk+memory caching
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "sprite_upscaler.h"
#include "edge_smooth.h"
#include "video.h"

// --- Color helpers ---

static inline int colorDiff(uint32_t a, uint32_t b) {
	if (a == b) return 0;
	const int r1 = (a >> 16) & 0xFF, g1 = (a >> 8) & 0xFF, b1 = a & 0xFF;
	const int r2 = (b >> 16) & 0xFF, g2 = (b >> 8) & 0xFF, b2 = b & 0xFF;
	const int dr = r1 - r2, dg = g1 - g2, db = b1 - b2;
	const int y = (dr * 299 + dg * 587 + db * 114) / 1000;
	const int u = (db * 436 - dr * 147 - dg * 289) / 1000;
	const int v = (dr * 615 - dg * 515 - db * 100) / 1000;
	return abs(y) * 2 + abs(u) + abs(v);
}

static inline bool colorsEqual(uint32_t a, uint32_t b) {
	return colorDiff(a, b) < 30;
}

static inline uint32_t blendPixels(uint32_t a, uint32_t b, int wa, int wb) {
	const int total = wa + wb;
	const int r = (((a >> 16) & 0xFF) * wa + ((b >> 16) & 0xFF) * wb) / total;
	const int g = (((a >> 8) & 0xFF) * wa + ((b >> 8) & 0xFF) * wb) / total;
	const int bl = ((a & 0xFF) * wa + (b & 0xFF) * wb) / total;
	const int al = (((a >> 24) & 0xFF) * wa + ((b >> 24) & 0xFF) * wb) / total;
	return (al << 24) | (r << 16) | (g << 8) | bl;
}

// Helper to sample with clamping
static inline uint32_t sampleClamped(const uint32_t *src, int x, int y, int w, int h) {
	x = (x < 0) ? 0 : (x >= w) ? w - 1 : x;
	y = (y < 0) ? 0 : (y >= h) ? h - 1 : y;
	return src[y * w + x];
}

// --- xBRZ scalers ---

void xbrz_scale2x_rgba(const uint32_t *src, int srcW, int srcH,
	uint32_t *dst, int dstPitch)
{
	for (int y = 0; y < srcH; ++y) {
		for (int x = 0; x < srcW; ++x) {
			const uint32_t C = src[y * srcW + x];
			const uint32_t U = sampleClamped(src, x, y-1, srcW, srcH);
			const uint32_t D = sampleClamped(src, x, y+1, srcW, srcH);
			const uint32_t L = sampleClamped(src, x-1, y, srcW, srcH);
			const uint32_t R = sampleClamped(src, x+1, y, srcW, srcH);

			uint32_t p0, p1, p2, p3;
			if (!colorsEqual(L, R) && !colorsEqual(U, D)) {
				p0 = colorsEqual(U, L) ? blendPixels(U, C, 3, 1) : C;
				p1 = colorsEqual(U, R) ? blendPixels(U, C, 3, 1) : C;
				p2 = colorsEqual(D, L) ? blendPixels(D, C, 3, 1) : C;
				p3 = colorsEqual(D, R) ? blendPixels(D, C, 3, 1) : C;
			} else {
				p0 = p1 = p2 = p3 = C;
			}
			const int dx = x * 2, dy = y * 2;
			dst[dy * dstPitch + dx] = p0;
			dst[dy * dstPitch + dx + 1] = p1;
			dst[(dy+1) * dstPitch + dx] = p2;
			dst[(dy+1) * dstPitch + dx + 1] = p3;
		}
	}
}

void xbrz_scale3x_rgba(const uint32_t *src, int srcW, int srcH,
	uint32_t *dst, int dstPitch)
{
	for (int y = 0; y < srcH; ++y) {
		for (int x = 0; x < srcW; ++x) {
			const uint32_t C = src[y * srcW + x];
			const uint32_t U = sampleClamped(src, x, y-1, srcW, srcH);
			const uint32_t D = sampleClamped(src, x, y+1, srcW, srcH);
			const uint32_t L = sampleClamped(src, x-1, y, srcW, srcH);
			const uint32_t R = sampleClamped(src, x+1, y, srcW, srcH);
			const uint32_t UL = sampleClamped(src, x-1, y-1, srcW, srcH);
			const uint32_t UR = sampleClamped(src, x+1, y-1, srcW, srcH);
			const uint32_t DL = sampleClamped(src, x-1, y+1, srcW, srcH);
			const uint32_t DR = sampleClamped(src, x+1, y+1, srcW, srcH);

			uint32_t p[9];
			p[4] = C;
			if (!colorsEqual(L, R) && !colorsEqual(U, D)) {
				p[0] = colorsEqual(U, L) ? blendPixels(U, C, 3, 1) : C;
				p[1] = (colorsEqual(U, L) && !colorsEqual(C, UR)) ||
				       (colorsEqual(U, R) && !colorsEqual(C, UL)) ? U : C;
				p[2] = colorsEqual(U, R) ? blendPixels(U, C, 3, 1) : C;
				p[3] = (colorsEqual(U, L) && !colorsEqual(C, DL)) ||
				       (colorsEqual(D, L) && !colorsEqual(C, UL)) ? L : C;
				p[5] = (colorsEqual(U, R) && !colorsEqual(C, DR)) ||
				       (colorsEqual(D, R) && !colorsEqual(C, UR)) ? R : C;
				p[6] = colorsEqual(D, L) ? blendPixels(D, C, 3, 1) : C;
				p[7] = (colorsEqual(D, L) && !colorsEqual(C, DR)) ||
				       (colorsEqual(D, R) && !colorsEqual(C, DL)) ? D : C;
				p[8] = colorsEqual(D, R) ? blendPixels(D, C, 3, 1) : C;
			} else {
				for (int i = 0; i < 9; ++i) p[i] = C;
			}
			const int dx = x * 3, dy = y * 3;
			for (int j = 0; j < 3; ++j)
				for (int i = 0; i < 3; ++i)
					dst[(dy+j) * dstPitch + dx + i] = p[j*3+i];
		}
	}
}

void xbrz_scale4x_rgba(const uint32_t *src, int srcW, int srcH,
	uint32_t *dst, int dstPitch)
{
	// 4x = 2x applied twice
	const int midW = srcW * 2, midH = srcH * 2;
	uint32_t *mid = (uint32_t *)malloc(midW * midH * sizeof(uint32_t));
	xbrz_scale2x_rgba(src, srcW, srcH, mid, midW);
	xbrz_scale2x_rgba(mid, midW, midH, dst, dstPitch);
	free(mid);
}

void xbrz_scale5x_rgba(const uint32_t *src, int srcW, int srcH,
	uint32_t *dst, int dstPitch)
{
	// 5x: not directly supported, use nearest-neighbor on top of 4x
	// or bilinear. For quality, we do: scale to 3x then scale that ~1.67x via bilinear
	// Simpler: just do nearest from original
	for (int dy = 0; dy < srcH * 5; ++dy) {
		const int sy = dy / 5;
		for (int dx = 0; dx < srcW * 5; ++dx) {
			const int sx = dx / 5;
			dst[dy * dstPitch + dx] = src[sy * srcW + sx];
		}
	}
}

// Chain two scale factors to achieve target
void SpriteUpscaler::getScaleChain(int scale, int *first, int *second) {
	// Find best decomposition into two factors, each 2-4
	if (scale <= 4) {
		*first = scale; *second = 1; return;
	}
	// Try to decompose
	static const int factors[][2] = {
		{0,0}, {1,1}, {2,1}, {3,1}, {4,1},  // 0-4
		{3,2}, // 5 = 3*2 (approximate: actually need 5, round up)
		{3,2}, // 6 = 3*2
		{4,2}, // 7 ~ 4*2 = 8 (closest)
		{4,2}, // 8 = 4*2
		{3,3}, // 9 = 3*3
		{4,3}, // 10 ~ 4*3 = 12 (closest achievable)
		{4,3}, // 11 ~ 4*3
		{4,3}, // 12 = 4*3
		{4,4}, // 13 ~ 4*4 = 16
		{4,4}, // 14 ~ 4*4
		{4,4}, // 15 ~ 4*4 = 16 (then SDL downscales to fit)
		{4,4}, // 16 = 4*4
	};
	if (scale > 16) scale = 16;
	*first = factors[scale][0];
	*second = factors[scale][1];
}

void xbrz_scaleNx_rgba(const uint32_t *src, int srcW, int srcH,
	uint32_t *dst, int dstW, int dstH, int scale)
{
	int first, second;
	SpriteUpscaler::getScaleChain(scale, &first, &second);

	if (second <= 1) {
		// Single pass
		switch (first) {
		case 2: xbrz_scale2x_rgba(src, srcW, srcH, dst, dstW); break;
		case 3: xbrz_scale3x_rgba(src, srcW, srcH, dst, dstW); break;
		case 4: xbrz_scale4x_rgba(src, srcW, srcH, dst, dstW); break;
		default:
			// Nearest neighbor fallback
			for (int y = 0; y < dstH; ++y)
				for (int x = 0; x < dstW; ++x)
					dst[y * dstW + x] = src[(y/scale) * srcW + x/scale];
			break;
		}
		return;
	}

	// Two-pass chain
	const int midW = srcW * first, midH = srcH * first;
	uint32_t *mid = (uint32_t *)malloc(midW * midH * sizeof(uint32_t));

	switch (first) {
	case 2: xbrz_scale2x_rgba(src, srcW, srcH, mid, midW); break;
	case 3: xbrz_scale3x_rgba(src, srcW, srcH, mid, midW); break;
	case 4: xbrz_scale4x_rgba(src, srcW, srcH, mid, midW); break;
	default: break;
	}

	const int finalW = midW * second;
	switch (second) {
	case 2: xbrz_scale2x_rgba(mid, midW, midH, dst, finalW); break;
	case 3: xbrz_scale3x_rgba(mid, midW, midH, dst, finalW); break;
	case 4: xbrz_scale4x_rgba(mid, midW, midH, dst, finalW); break;
	default: break;
	}

	free(mid);

	// If actual product differs from target, crop/pad is handled by caller
}

// --- SpriteUpscaler ---

SpriteUpscaler::SpriteUpscaler(int scale, const char *cachePath) {
	_scale = scale;
	memset(_hashTable, 0, sizeof(_hashTable));
	_cacheCount = 0;
	_cacheBytes = 0;
	_accessCounter = 0;
	_tempBufferSize = 256 * 256;
	_tempBuffer = (uint8_t *)calloc(_tempBufferSize, 1);
	_diskCacheEnabled = false;
	memset(_diskCachePath, 0, sizeof(_diskCachePath));
	if (cachePath) {
		setDiskCache(cachePath);
	}
}

SpriteUpscaler::~SpriteUpscaler() {
	clearCache();
	free(_tempBuffer);
}

void SpriteUpscaler::setDiskCache(const char *basePath) {
	snprintf(_diskCachePath, sizeof(_diskCachePath), "%s/%dx", basePath, _scale);
	// Create cache directories
	mkdir(basePath, 0755);
	mkdir(_diskCachePath, 0755);
	_diskCacheEnabled = true;
}

void SpriteUpscaler::makeDiskPath(char *buf, int bufSize, uintptr_t key,
	uint16_t w, uint16_t h) const
{
	snprintf(buf, bufSize, "%s/spr_%016lx_%dx%d.raw",
		_diskCachePath, (unsigned long)key, w, h);
}

bool SpriteUpscaler::loadFromDisk(uintptr_t key, uint16_t w, uint16_t h,
	HdSprite *out)
{
	if (!_diskCacheEnabled) return false;

	char path[512];
	makeDiskPath(path, sizeof(path), key, w, h);

	FILE *fp = fopen(path, "rb");
	if (!fp) return false;

	// Read header: width(4) height(4)
	int32_t dw, dh;
	if (fread(&dw, 4, 1, fp) != 1 || fread(&dh, 4, 1, fp) != 1) {
		fclose(fp);
		return false;
	}

	const int pixCount = dw * dh;
	out->pixels = (uint32_t *)malloc(pixCount * sizeof(uint32_t));
	out->width = dw;
	out->height = dh;

	if ((int)fread(out->pixels, sizeof(uint32_t), pixCount, fp) != pixCount) {
		free(out->pixels);
		out->pixels = 0;
		fclose(fp);
		return false;
	}

	fclose(fp);
	return true;
}

void SpriteUpscaler::saveToDisk(uintptr_t key, uint16_t w, uint16_t h,
	const HdSprite *spr)
{
	if (!_diskCacheEnabled) return;

	char path[512];
	makeDiskPath(path, sizeof(path), key, w, h);

	FILE *fp = fopen(path, "wb");
	if (!fp) return;

	int32_t dw = spr->width, dh = spr->height;
	fwrite(&dw, 4, 1, fp);
	fwrite(&dh, 4, 1, fp);
	fwrite(spr->pixels, sizeof(uint32_t), dw * dh, fp);
	fclose(fp);
}

uintptr_t SpriteUpscaler::makeKey(const uint8_t *sprData, uint8_t flags) const {
	// Legacy pointer-based key — kept only to keep the symbol; not used.
	(void)sprData; (void)flags;
	return 0;
}

// FNV-1a 64-bit. Mixes (decoded indexed bytes, w/h/flags, palette).
// Palette is part of the key so that the same sprite content rendered with
// a different on-screen palette (between screens, fades, cutscenes) gets
// its own cache entry instead of reusing stale colors.
static uintptr_t hashSpriteContent(const uint8_t *buf, int n,
	uint16_t w, uint16_t h, uint8_t flags, uint64_t paletteHash)
{
	uint64_t hash = 0xcbf29ce484222325ULL;
	for (int i = 0; i < n; ++i) {
		hash ^= (uint64_t)buf[i];
		hash *= 0x100000001b3ULL;
	}
	const uint8_t tail[5] = {
		(uint8_t)(w & 0xFF), (uint8_t)(w >> 8),
		(uint8_t)(h & 0xFF), (uint8_t)(h >> 8),
		(uint8_t)(flags & 3),
	};
	for (int i = 0; i < 5; ++i) {
		hash ^= (uint64_t)tail[i];
		hash *= 0x100000001b3ULL;
	}
	for (int i = 0; i < 8; ++i) {
		hash ^= (paletteHash >> (i * 8)) & 0xFF;
		hash *= 0x100000001b3ULL;
	}
	return (uintptr_t)hash;
}

static uint64_t hashPalette(const uint32_t *palette) {
	uint64_t hash = 0xcbf29ce484222325ULL;
	for (int i = 0; i < 256; ++i) {
		const uint32_t v = palette[i];
		for (int b = 0; b < 4; ++b) {
			hash ^= (v >> (b * 8)) & 0xFF;
			hash *= 0x100000001b3ULL;
		}
	}
	return hash;
}

SpriteUpscaler::CacheEntry *SpriteUpscaler::findEntry(uintptr_t key) {
	const int idx = (int)(key % kHashSize);
	CacheEntry *e = _hashTable[idx];
	while (e) {
		if (e->key == key) {
			e->lastUsed = ++_accessCounter;
			return e;
		}
		e = e->next;
	}
	return 0;
}

void SpriteUpscaler::insertEntry(uintptr_t key, const HdSprite &sprite) {
	while (_cacheBytes > (size_t)kMaxCacheBytes && _cacheCount > 0) {
		evictOldest();
	}
	CacheEntry *e = (CacheEntry *)malloc(sizeof(CacheEntry));
	e->key = key;
	e->sprite = sprite;
	e->lastUsed = ++_accessCounter;
	const int idx = (int)(key % kHashSize);
	e->next = _hashTable[idx];
	_hashTable[idx] = e;
	_cacheCount++;
	_cacheBytes += sprite.width * sprite.height * sizeof(uint32_t);
}

void SpriteUpscaler::evictOldest() {
	uint32_t oldest = _accessCounter;
	int oldestIdx = -1;
	CacheEntry *oldestEntry = 0, *oldestPrev = 0;

	for (int i = 0; i < kHashSize; ++i) {
		CacheEntry *prev = 0;
		CacheEntry *e = _hashTable[i];
		while (e) {
			if (e->lastUsed < oldest) {
				oldest = e->lastUsed;
				oldestIdx = i;
				oldestEntry = e;
				oldestPrev = prev;
			}
			prev = e;
			e = e->next;
		}
	}
	if (oldestEntry) {
		if (oldestPrev) oldestPrev->next = oldestEntry->next;
		else _hashTable[oldestIdx] = oldestEntry->next;
		_cacheBytes -= oldestEntry->sprite.width * oldestEntry->sprite.height * sizeof(uint32_t);
		free(oldestEntry->sprite.pixels);
		free(oldestEntry);
		_cacheCount--;
	}
}

void SpriteUpscaler::clearCache() {
	for (int i = 0; i < kHashSize; ++i) {
		CacheEntry *e = _hashTable[i];
		while (e) {
			CacheEntry *next = e->next;
			free(e->sprite.pixels);
			free(e);
			e = next;
		}
		_hashTable[i] = 0;
	}
	_cacheCount = 0;
	_cacheBytes = 0;
	_accessCounter = 0;
}

void SpriteUpscaler::decodeSprToTemp(const uint8_t *src, int w, int h, uint8_t flags) {
	const int size = w * h;
	if (size > _tempBufferSize) {
		_tempBufferSize = size;
		_tempBuffer = (uint8_t *)realloc(_tempBuffer, _tempBufferSize);
	}
	memset(_tempBuffer, 0, size);

	const bool hflip = (flags & 1) != 0;
	int x = 0, y = 0;
	while (y < h) {
		const uint8_t code = *src++;
		const int count = code & 0x3F;
		switch (code >> 6) {
		case 0:
			if (count == 0) break;
			for (int i = 0; i < count && x < w; ++i) {
				const int px = hflip ? (w - 1 - x) : x;
				if (px >= 0 && px < w && y >= 0 && y < h)
					_tempBuffer[y * w + px] = *src;
				++src; ++x;
			}
			break;
		case 1: {
			const uint8_t color = *src++;
			for (int i = 0; i < count && x < w; ++i) {
				const int px = hflip ? (w - 1 - x) : x;
				if (px >= 0 && px < w && y >= 0 && y < h)
					_tempBuffer[y * w + px] = color;
				++x;
			}
			break;
		}
		case 2: { int skip = count; if (skip == 0) skip = *src++; x += skip; break; }
		case 3: { int lines = count; if (lines == 0) lines = *src++; y += lines; x = *src++; break; }
		}
	}
}

const HdSprite *SpriteUpscaler::getOrUpscale(
	const uint8_t *sprData, uint16_t spr_w, uint16_t spr_h,
	uint8_t flags, const uint32_t *palette)
{
	if (!sprData || spr_w == 0 || spr_h == 0) return 0;

	// Decode SPR first so the cache key can be derived from sprite content
	// (heap pointers change every run and would defeat the disk cache).
	decodeSprToTemp(sprData, spr_w, spr_h, flags);
	const uint64_t paletteHash = palette ? hashPalette(palette) : 0;
	const uintptr_t key = hashSpriteContent(_tempBuffer, spr_w * spr_h,
		spr_w, spr_h, flags, paletteHash);

	// Check RAM cache
	CacheEntry *entry = findEntry(key);
	if (entry) return &entry->sprite;

	// Check disk cache
	HdSprite diskSprite;
	if (loadFromDisk(key, spr_w, spr_h, &diskSprite)) {
		insertEntry(key, diskSprite);
		return &findEntry(key)->sprite;
	}

	// Convert to RGBA
	const int srcSize = spr_w * spr_h;
	uint32_t *rgba = (uint32_t *)malloc(srcSize * sizeof(uint32_t));
	for (int i = 0; i < srcSize; ++i) {
		const uint8_t idx = _tempBuffer[i];
		rgba[i] = (idx == 0) ? 0 : (palette[idx] | 0xFF000000);
	}

	// Compute actual output size via scale chain
	int first, second;
	getScaleChain(_scale, &first, &second);
	const int actualScale = first * (second > 1 ? second : 1);
	const int dst_w = spr_w * actualScale;
	const int dst_h = spr_h * actualScale;

	uint32_t *final_buf = (uint32_t *)malloc(dst_w * dst_h * sizeof(uint32_t));
	xbrz_scaleNx_rgba(rgba, spr_w, spr_h, final_buf, dst_w, dst_h, _scale);
	free(rgba);

	// Apply edge smoothing
	mlaa_smooth(final_buf, dst_w, dst_h);

	HdSprite sprite;
	sprite.pixels = final_buf;
	sprite.width = dst_w;
	sprite.height = dst_h;

	// Save to disk before inserting to RAM
	saveToDisk(key, spr_w, spr_h, &sprite);
	insertEntry(key, sprite);
	return &findEntry(key)->sprite;
}

void SpriteUpscaler::upscaleBackground(
	const uint8_t *src, int srcW, int srcH,
	const uint32_t *palette, uint32_t *dst, int dstW, int dstH)
{
	const int srcSize = srcW * srcH;
	uint32_t *rgba = (uint32_t *)malloc(srcSize * sizeof(uint32_t));
	for (int i = 0; i < srcSize; ++i) {
		rgba[i] = palette[src[i]] | 0xFF000000;
	}

	int first, second;
	getScaleChain(_scale, &first, &second);
	const int actualScale = first * (second > 1 ? second : 1);

	uint32_t *scaledBuf = dst;
	bool needFree = false;

	// If actual scale product != target dimensions, allocate temp and crop
	const int actualW = srcW * actualScale, actualH = srcH * actualScale;
	if (actualW != dstW || actualH != dstH) {
		scaledBuf = (uint32_t *)malloc(actualW * actualH * sizeof(uint32_t));
		needFree = true;
	}

	xbrz_scaleNx_rgba(rgba, srcW, srcH, scaledBuf, actualW, actualH, _scale);
	free(rgba);

	mlaa_smooth(scaledBuf, actualW, actualH);

	// Copy/crop to destination if needed
	if (needFree) {
		const int copyW = (actualW < dstW) ? actualW : dstW;
		const int copyH = (actualH < dstH) ? actualH : dstH;
		memset(dst, 0, dstW * dstH * sizeof(uint32_t));
		for (int y = 0; y < copyH; ++y) {
			memcpy(dst + y * dstW, scaledBuf + y * actualW, copyW * sizeof(uint32_t));
		}
		free(scaledBuf);
	}
}
