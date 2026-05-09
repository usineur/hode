/*
 * Heart of Darkness engine rewrite
 * Sprite upscaling with xBRZ and caching (memory + disk)
 */

#ifndef SPRITE_UPSCALER_H__
#define SPRITE_UPSCALER_H__

#include "intern.h"

struct HdSprite {
	uint32_t *pixels;
	int width;
	int height;
};

struct SpriteUpscaler {
	enum {
		kMaxCacheEntries = 8192,
		kMaxCacheBytes = 512 * 1024 * 1024 // 512MB RAM
	};

	int _scale;
	char _diskCachePath[256]; // e.g. "cache/6x/"
	bool _diskCacheEnabled;

	struct CacheEntry {
		uintptr_t key;
		HdSprite sprite;
		uint32_t lastUsed;
		CacheEntry *next;
	};

	enum { kHashSize = 2048 };
	CacheEntry *_hashTable[kHashSize];
	int _cacheCount;
	size_t _cacheBytes;
	uint32_t _accessCounter;
	uint8_t *_tempBuffer;
	int _tempBufferSize;

	SpriteUpscaler(int scale = 6, const char *cachePath = 0);
	~SpriteUpscaler();

	void setDiskCache(const char *basePath);

	const HdSprite *getOrUpscale(
		const uint8_t *sprData,
		uint16_t spr_w, uint16_t spr_h,
		uint8_t flags,
		const uint32_t *palette
	);

	void upscaleBackground(
		const uint8_t *src, int srcW, int srcH,
		const uint32_t *palette,
		uint32_t *dst, int dstW, int dstH
	);

	void clearCache();

	// Compute the chained scale factors for any target scale
	// e.g. 6 = 3*2, 8 = 4*2, 12 = 4*3, 15 = 5*3, etc.
	static void getScaleChain(int scale, int *first, int *second);

private:
	uintptr_t makeKey(const uint8_t *sprData, uint8_t flags) const;
	CacheEntry *findEntry(uintptr_t key);
	void insertEntry(uintptr_t key, const HdSprite &sprite);
	void evictOldest();
	void decodeSprToTemp(const uint8_t *src, int w, int h, uint8_t flags);

	// Disk cache
	bool loadFromDisk(uintptr_t key, uint16_t w, uint16_t h, HdSprite *out);
	void saveToDisk(uintptr_t key, uint16_t w, uint16_t h, const HdSprite *spr);
	void makeDiskPath(char *buf, int bufSize, uintptr_t key, uint16_t w, uint16_t h) const;
};

// xBRZ RGBA scaling functions (2x through 5x)
void xbrz_scale2x_rgba(const uint32_t *src, int srcW, int srcH,
	uint32_t *dst, int dstPitch);
void xbrz_scale3x_rgba(const uint32_t *src, int srcW, int srcH,
	uint32_t *dst, int dstPitch);
void xbrz_scale4x_rgba(const uint32_t *src, int srcW, int srcH,
	uint32_t *dst, int dstPitch);
void xbrz_scale5x_rgba(const uint32_t *src, int srcW, int srcH,
	uint32_t *dst, int dstPitch);

// Arbitrary scale via chaining
void xbrz_scaleNx_rgba(const uint32_t *src, int srcW, int srcH,
	uint32_t *dst, int dstW, int dstH, int scale);

#endif // SPRITE_UPSCALER_H__
