/*
 * Heart of Darkness engine rewrite
 * HD rendering compositor with multi-resolution and 16:9 borders
 */

#ifndef HD_COMPOSITOR_H__
#define HD_COMPOSITOR_H__

#include "intern.h"
#include "defs.h"

struct Resource;
struct SpriteUpscaler;
struct Video;

#include <time.h>

// Shared progress tracker so the level-sprite walker and PAF frame callback
// can drive a single console bar across both phases.
struct PrerenderProgress {
	int done;
	int total;
	int lastBar;
	char label[80];
	struct timespec t0;
};

// Render one frame of the prerender progress bar to stderr (with carriage
// return — caller emits the trailing newline at the end of the run).
void HdCompositor_drawProgressBar(const char *label, int done, int total, double elapsed);

struct HdCompositor {
	// Resolution presets (scale factors from 256x192)
	enum ScalePreset {
		kScale_HD     = 6,   // 1536x1152
		kScale_FullHD = 8,   // 2048x1536
		kScale_QHD    = 10,  // 2560x1920
		kScale_4K     = 15,  // 3840x2880
		kDefaultScale = 6
	};

	uint32_t *_hdFramebuffer;     // main HD output (4:3 game area)
	uint32_t *_hdBackground;      // cached upscaled background
	uint32_t *_wideFramebuffer;   // 16:9 output with borders (if enabled)
	int _scale;
	int _hdW, _hdH;              // game area dimensions (e.g. 1536x1152)
	int _wideW, _wideH;          // 16:9 dimensions (e.g. 2048x1152)
	bool _enabled;
	bool _widescreenEnabled;
	SpriteUpscaler *_upscaler;
	int _cachedScreenNum;
	int _cachedBackgroundId;
	uint32_t _palette[256];
	bool _paletteValid;

	// When non-null, prerender helpers report into this shared counter rather
	// than driving their own bar. Allows the sprite walker and PAF frame
	// callback to advance one unified progress bar.
	PrerenderProgress *_progress;

	// Dynamic border colors (computed from palette)
	uint32_t _borderColorTop;
	uint32_t _borderColorBottom;
	uint32_t _borderColorAvg;

	HdCompositor(int scale = kDefaultScale, const char *cachePath = 0);
	~HdCompositor();

	void enable(bool on) { _enabled = on; }
	bool isEnabled() const { return _enabled; }
	void enableWidescreen(bool on);

	void updatePalette(const uint8_t *pal, int n, int depth);

	void beginFrame(const uint8_t *bgLayer, const uint8_t *palette,
		int screenNum, int backgroundId);

	void drawSprite(const uint8_t *bitmapBits, int x, int y,
		uint16_t w, uint16_t h, uint8_t flags);

	void endFrame();

	// Walk every sprite frame for the loaded level (in res) and force the
	// upscaler to populate its RAM/disk cache. If _progress is non-null,
	// each frame ticks that shared counter; otherwise this prints its own bar.
	void prerenderLevelSprites(Resource *res, const char *label = 0);

	// Count units of work the sprite walker would do — used by the unified
	// progress driver to know the sprite portion of the total ahead of time.
	int countLevelSpriteFrames(Resource *res);

	// Returns the appropriate framebuffer (widescreen or 4:3)
	void getFramebuffer(uint32_t **buf, int *w, int *h);

	static int scaleForResolution(int targetW, int targetH);

private:
	void upscaleAndCacheBackground(const uint8_t *bgLayer, const uint8_t *palette);
	void blitHdSprite(const uint32_t *pixels, int sprW, int sprH, int dstX, int dstY);
	void computeBorderColors(const uint8_t *bgLayer);
	void compositeWidescreen();
};

#endif // HD_COMPOSITOR_H__
