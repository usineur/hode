
#include <malloc.h>
#include <unistd.h>

#include <fat.h>

#include <gccore.h>
#include <ogc/audio.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <wupc/wupc.h>
#include <wiidrc/wiidrc.h>

#include "system.h"

struct System_Wii : System {

	int _shakeDx, _shakeDy;
	uint64_t _startTime;
	AudioCallback _audioCb;
	mutex_t _audioMutex;
	lwpq_t _audioQueue;
	lwp_t _audioThread;
	bool _audioOut;
	GXRModeObj *_rmodeObj;
	GXTlutObj _tlutObj;
	GXTexObj _texObj;
	int _projTop, _projLeft;
	int _gamma;

	System_Wii();
	virtual ~System_Wii();
	virtual void init(const char *title, int w, int h, bool fullscreen, bool widescreen, bool yuv);
	virtual void destroy();
	virtual void setScaler(const char *name, int multiplier);
	virtual void setGamma(float gamma);
	virtual void setPalette(const uint8_t *pal, int n, int depth);
	virtual void clearPalette();
	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch);
	virtual void copyYuv(int w, int h, const uint8_t *y, int ypitch, const uint8_t *u, int upitch, const uint8_t *v, int vpitch);
	virtual void fillRect(int x, int y, int w, int h, uint8_t color);
	virtual void copyRectWidescreen(int w, int h, const uint8_t *buf, const uint8_t *pal);
	virtual void shakeScreen(int dx, int dy);
	virtual void updateScreen(bool drawWidescreen);
	virtual void processEvents();
	virtual void sleep(int duration);
	virtual uint32_t getTimeStamp();
	virtual void startAudio(AudioCallback callback);
	virtual void stopAudio();
	virtual void lockAudio();
	virtual void unlockAudio();
	virtual AudioCallback setAudioCallback(AudioCallback callback);

	void setupVideo();

	void initGX();
	void finiGX();
	void drawTextureGX(int x, int y);
};

static System_Wii system_wii;
System *const g_system = &system_wii;

static uint32_t __attribute__((aligned(16))) _fifo[256 * 1024];
static uint32_t *_xfb[2];
static int _current_fb;

static const int GAME_W = 256;
static const int GAME_H = 192;

static uint16_t __attribute__((aligned(32))) _clut[256];
static uint8_t  __attribute__((aligned(32))) _texture[GAME_W * GAME_H];

static const int DMA_BUFFER_SAMPLES = 512;
static const int DMA_BUFFER_SIZE = DMA_BUFFER_SAMPLES * 2 * sizeof(int16_t); // stereo s16

static const int AUDIO_THREAD_PRIORITY = 80;

static const int AUDIO_BUFFER_SAMPLES = DMA_BUFFER_SAMPLES * 22050 / 32000;
static const int AUDIO_BUFFER_SIZE = AUDIO_BUFFER_SAMPLES * 2 * sizeof(int16_t); // stereo s16

static uint8_t __attribute__((aligned(32))) _audioThreadStack[16384];
static int16_t __attribute__((aligned(32))) _resample[DMA_BUFFER_SAMPLES * 2];
static uint8_t __attribute__((aligned(32))) _dma[2][DMA_BUFFER_SIZE];
static int _current_dma;

void System_earlyInit() {
	fatInitDefault();
}

void System_fatalError(const char *s) {
	GXRModeObj *rmodeObj = system_wii._rmodeObj;
	if (!rmodeObj) {
		VIDEO_Init();
		system_wii.setupVideo();
		rmodeObj = system_wii._rmodeObj;
	}
	console_init(_xfb[_current_fb], 20, 20, rmodeObj->fbWidth, rmodeObj->xfbHeight, rmodeObj->fbWidth * VI_DISPLAY_PIX_SZ);
	fputs(s, stdout);
	usleep(10 * 1000 * 1000);
}

void System_printLog(FILE *fp, const char *s) {
}

bool System_hasCommandLine() {
	return false;
}

System_Wii::System_Wii() {
	_rmodeObj = 0;
}

System_Wii::~System_Wii() {
}

void System_Wii::init(const char *title, int w, int h, bool fullscreen, bool widescreen, bool yuv) {
	memset(&inp, 0, sizeof(inp));
	memset(&pad, 0, sizeof(pad));

	_shakeDx = _shakeDy = 0;
	_startTime = gettime();

	memset(&_audioCb, 0, sizeof(_audioCb));
	_audioMutex = 0;
	_audioThread = LWP_THREAD_NULL;
	_audioOut = false;

	AUDIO_Init(0);

	_gamma = GX_GM_1_0;

	if (!_rmodeObj) {
		VIDEO_Init();
		setupVideo();
	}

	PAD_Init();
	WUPC_Init();
	WPAD_Init();
	WiiDRC_Init();

	initGX();
}

void System_Wii::destroy() {
	WUPC_Shutdown();
	WPAD_Shutdown();

	finiGX();

	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	free(MEM_K1_TO_K0(_xfb[0]));
	_xfb[0] = 0;
	free(MEM_K1_TO_K0(_xfb[1]));
	_xfb[1] = 0;

	fatUnmount("sd:/");
	fatUnmount("usb:/");
}

void System_Wii::setScaler(const char *name, int multiplier) {
}

void System_Wii::setGamma(float gamma) {
	if (gamma < 1.7f) {
		_gamma = GX_GM_1_0;
	} else if (gamma < 2.2f) {
		_gamma = GX_GM_1_7;
	} else {
		_gamma = GX_GM_2_2;
	}
}

void System_Wii::setPalette(const uint8_t *pal, int n, int depth) {
	const int shift = 8 - depth;
	for (int i = 0; i < n; ++i) {
		int r = pal[i * 3];
		int g = pal[i * 3 + 1];
		int b = pal[i * 3 + 2];
		if (shift != 0) {
			r = (r << shift) | (r >> depth);
			g = (g << shift) | (g >> depth);
			b = (b << shift) | (b >> depth);
		}
		_clut[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
	}
	DCFlushRange(_clut, sizeof(_clut));
	GX_LoadTlut(&_tlutObj, GX_TLUT0);
}

void System_Wii::clearPalette() {
	memset(_clut, 0, sizeof(_clut));
	DCFlushRange(_clut, sizeof(_clut));
	GX_LoadTlut(&_tlutObj, GX_TLUT0);
}

void System_Wii::copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) {
	uint64_t *dst = (uint64_t *)_texture + y * GAME_W + x;
	uint64_t *src = (uint64_t *)buf;
	assert((pitch & 7) == 0);
	pitch >>= 3;
	w >>= 3;
	for (int y = 0; y < h; y += 4) {
		for (int x = 0; x < w; ++x) {
			for (int i = 0; i < 4; ++i) {
				*dst++ = src[pitch * i + x];
			}
		}
		src += pitch * 4;
	}
	DCFlushRange(_texture, sizeof(_texture));
	GX_LoadTexObj(&_texObj, GX_TEXMAP0);
}

void System_Wii::copyYuv(int w, int h, const uint8_t *y, int ypitch, const uint8_t *u, int upitch, const uint8_t *v, int vpitch) {
}

void System_Wii::fillRect(int x, int y, int w, int h, uint8_t color) {
	uint8_t *p = _texture + y * GAME_W + x;
	if (w == GAME_W) {
		memset(p, color, w * h);
	} else {
		for (int y = 0; y < h; ++y) {
			memset(p, color, w);
			p += GAME_W;
		}
	}
	DCFlushRange(_texture, sizeof(_texture));
	GX_LoadTexObj(&_texObj, GX_TEXMAP0);
}

void System_Wii::copyRectWidescreen(int w, int h, const uint8_t *buf, const uint8_t *pal) {
}

void System_Wii::shakeScreen(int dx, int dy) {
	_shakeDx = dx;
	_shakeDy = dy;
}

void System_Wii::updateScreen(bool drawWidescreen) {

	GX_InvalidateTexAll();
	drawTextureGX(_shakeDx, _shakeDy);
	GX_DrawDone();

	_current_fb ^= 1;
	GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_CopyDisp(_xfb[_current_fb], GX_TRUE);
	GX_Flush();

	VIDEO_SetNextFramebuffer(_xfb[_current_fb]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}

void System_Wii::processEvents() {
	inp.prevMask = inp.mask;
	inp.mask = 0;

	if (SYS_ResetButtonDown()) {
		inp.quit = true;
	}

	static const struct {
		int pad;
		int wpad;
		int wdrc;
		int sys;
	} mapping[] = {
		{ PAD_BUTTON_UP,    (WPAD_CLASSIC_BUTTON_UP    | WPAD_BUTTON_RIGHT), WIIDRC_BUTTON_UP,    SYS_INP_UP },
		{ PAD_BUTTON_RIGHT, (WPAD_CLASSIC_BUTTON_RIGHT | WPAD_BUTTON_DOWN),  WIIDRC_BUTTON_RIGHT, SYS_INP_RIGHT },
		{ PAD_BUTTON_DOWN,  (WPAD_CLASSIC_BUTTON_DOWN  | WPAD_BUTTON_LEFT),  WIIDRC_BUTTON_DOWN,  SYS_INP_DOWN },
		{ PAD_BUTTON_LEFT,  (WPAD_CLASSIC_BUTTON_LEFT  | WPAD_BUTTON_UP),    WIIDRC_BUTTON_LEFT,  SYS_INP_LEFT },
		{ PAD_BUTTON_A,     (WPAD_CLASSIC_BUTTON_A     | WPAD_BUTTON_A),     WIIDRC_BUTTON_A,     SYS_INP_JUMP },
		{ PAD_BUTTON_B,     (WPAD_CLASSIC_BUTTON_B     | WPAD_BUTTON_B),     WIIDRC_BUTTON_B,     SYS_INP_RUN },
		{ PAD_BUTTON_X,     (WPAD_CLASSIC_BUTTON_X     | WPAD_BUTTON_1),     WIIDRC_BUTTON_X,     SYS_INP_SHOOT },
		{ PAD_BUTTON_Y,     (WPAD_CLASSIC_BUTTON_Y     | WPAD_BUTTON_2),     WIIDRC_BUTTON_Y,     SYS_INP_SHOOT | SYS_INP_RUN },
		{ PAD_BUTTON_START, (WPAD_CLASSIC_BUTTON_HOME  | WPAD_BUTTON_HOME),  WIIDRC_BUTTON_HOME,  SYS_INP_ESC },
		{ 0, 0, 0 }
	};
	PAD_ScanPads();
	const uint32_t padMask = PAD_ButtonsDown(0) | PAD_ButtonsHeld(0);
	WUPC_UpdateButtonStats();
	uint32_t wpadMask = WUPC_ButtonsDown(0) | WUPC_ButtonsHeld(0);
	WPAD_ScanPads();
	wpadMask |= WPAD_ButtonsDown(0) | WPAD_ButtonsHeld(0);
	WiiDRC_ScanPads();
	const uint32_t wdrcMask = WiiDRC_ButtonsDown() | WiiDRC_ButtonsHeld();
	for (int i = 0; mapping[i].pad != 0; ++i) {
		if ((mapping[i].pad & padMask) != 0 || (mapping[i].wpad & wpadMask) != 0 || (mapping[i].wdrc & wdrcMask) != 0) {
			inp.mask |= mapping[i].sys;
		}
	}
	const int x = PAD_StickX(0);
	if (x > 64) {
		inp.mask |= SYS_INP_RIGHT;
	} else if (x < -64) {
		inp.mask |= SYS_INP_LEFT;
	}
	const int y = PAD_StickY(0);
	if (y > 64) {
		inp.mask |= SYS_INP_UP;
	} else if (y < -64) {
		inp.mask |= SYS_INP_DOWN;
	}
}

void System_Wii::sleep(int duration) {
	usleep(duration * 1000);
}

uint32_t System_Wii::getTimeStamp() {
	const uint64_t ticks = diff_ticks(_startTime, gettime());
	return ticks_to_millisecs(ticks);
}

static void *audioThread(void *arg) {
	while (system_wii._audioOut) {

		memset(_dma[_current_dma], 0, DMA_BUFFER_SIZE);

		int16_t buf22khz[AUDIO_BUFFER_SAMPLES * 2]; // stereo
		memset(buf22khz, 0, sizeof(buf22khz));
		system_wii.lockAudio();
		(system_wii._audioCb.proc)(system_wii._audioCb.userdata, buf22khz, AUDIO_BUFFER_SAMPLES * 2);
		system_wii.unlockAudio();

		// point resampling
		static const int fracBits = 16;
		static const uint32_t step = (22050 << fracBits) / 32000;
		uint32_t len = 0;
		for (uint32_t pos = 0; len < DMA_BUFFER_SAMPLES * 2; pos += step) {
			const int16_t *p = buf22khz + MIN<uint32_t>((pos >> fracBits), AUDIO_BUFFER_SAMPLES - 1) * 2;
			_resample[len++] = p[0];
			_resample[len++] = p[1];
		}
		assert(len * sizeof(int16_t) == DMA_BUFFER_SIZE);

		memcpy(_dma[_current_dma], _resample, DMA_BUFFER_SIZE);
		DCFlushRange(_dma[_current_dma], DMA_BUFFER_SIZE);

		LWP_ThreadSleep(system_wii._audioQueue);
	}
	return 0;
}

static void dmaCallback() {
	_current_dma ^= 1;
	AUDIO_InitDMA((uint32_t)_dma[_current_dma], DMA_BUFFER_SIZE);
	LWP_ThreadSignal(system_wii._audioQueue);
}

void System_Wii::startAudio(AudioCallback callback) {
	_audioCb = callback;
	LWP_MutexInit(&_audioMutex, FALSE);
	LWP_InitQueue(&_audioQueue);
	_audioOut = true;
	AUDIO_SetDSPSampleRate(AI_SAMPLERATE_32KHZ);
	LWP_CreateThread(&_audioThread, audioThread, 0, _audioThreadStack, sizeof(_audioThreadStack), AUDIO_THREAD_PRIORITY);
	AUDIO_RegisterDMACallback(dmaCallback);
	dmaCallback();
	AUDIO_InitDMA((uint32_t)_dma[_current_dma], DMA_BUFFER_SIZE);
	AUDIO_StartDMA();
}

void System_Wii::stopAudio() {
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(0);
	_audioOut = false;
	LWP_ThreadSignal(_audioQueue);
	LWP_JoinThread(_audioThread, 0);
	LWP_MutexDestroy(_audioMutex);
	LWP_CloseQueue(_audioQueue);
}

void System_Wii::lockAudio() {
	LWP_MutexLock(_audioMutex);
}

void System_Wii::unlockAudio() {
	LWP_MutexUnlock(_audioMutex);
}

AudioCallback System_Wii::setAudioCallback(AudioCallback callback) {
	lockAudio();
	AudioCallback cb = _audioCb;
	_audioCb = callback;
	unlockAudio();
	return cb;
}

void System_Wii::setupVideo() {
	_rmodeObj = VIDEO_GetPreferredMode(0);
	_rmodeObj->viWidth = 640;
	_rmodeObj->viXOrigin = (VI_MAX_WIDTH_NTSC - _rmodeObj->viWidth) / 2;
	VIDEO_Configure(_rmodeObj);

	_xfb[0] = (uint32_t *)SYS_AllocateFramebuffer(_rmodeObj);
	_xfb[1] = (uint32_t *)SYS_AllocateFramebuffer(_rmodeObj);
	DCInvalidateRange(_xfb[0], VIDEO_GetFrameBufferSize(_rmodeObj));
	DCInvalidateRange(_xfb[1], VIDEO_GetFrameBufferSize(_rmodeObj));
	_xfb[0] = (uint32_t *)MEM_K0_TO_K1(_xfb[0]);
	_xfb[1] = (uint32_t *)MEM_K0_TO_K1(_xfb[1]);

	VIDEO_ClearFrameBuffer(_rmodeObj, _xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(_rmodeObj, _xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer(_xfb[0]);
	_current_fb = 0;

	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (_rmodeObj->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
}

void System_Wii::initGX() {
	memset(_fifo, 0, sizeof(_fifo));
	GX_Init(_fifo, sizeof(_fifo));

	GXColor background = { 0, 0, 0, 0xFF };
	GX_SetCopyClear(background, GX_MAX_Z24);

	GX_SetViewport(0, 0, _rmodeObj->fbWidth, _rmodeObj->efbHeight, 0, 1);
	const int xfbHeight = GX_SetDispCopyYScale(GX_GetYScaleFactor(_rmodeObj->efbHeight, _rmodeObj->xfbHeight));
	GX_SetScissor(0, 0, _rmodeObj->fbWidth, _rmodeObj->efbHeight);

	GX_SetDispCopySrc(0, 0, _rmodeObj->fbWidth, _rmodeObj->efbHeight);
	GX_SetDispCopyDst(_rmodeObj->fbWidth, xfbHeight);

	GX_SetCopyFilter(_rmodeObj->aa, _rmodeObj->sample_pattern, GX_TRUE, _rmodeObj->vfilter);
	GX_SetFieldMode(_rmodeObj->field_rendering, ((_rmodeObj->viHeight == 2 * _rmodeObj->xfbHeight) ? GX_ENABLE : GX_DISABLE));
	GX_SetPixelFmt(_rmodeObj->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetDispCopyGamma(_gamma);
	GX_SetCullMode(GX_CULL_NONE);

	Mtx m;
	const int h    = GAME_H;
	const int top  = 0;
	const int w    = GAME_W * 11 / 10;
	const int left = 0;
	guOrtho(m, top, h - top, left, w - left, 0, 1);
	GX_LoadProjectionMtx(m, GX_ORTHOGRAPHIC);
	_projTop  = (h - GAME_H) / 2;
	_projLeft = (w - GAME_W) / 2;

	GX_InvVtxCache();
	GX_InvalidateTexAll();

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_InitTlutObj(&_tlutObj, _clut, GX_TL_RGB565, 256);
	GX_InitTexObjCI(&_texObj, _texture, GAME_W, GAME_H, GX_TF_CI8, GX_CLAMP, GX_CLAMP, GX_FALSE, GX_TLUT0);
	GX_InitTexObjFilterMode(&_texObj, GX_LIN_MIP_NEAR, GX_LINEAR); // GX_NEAR, GX_NEAR);
}

void System_Wii::finiGX() {
	GX_AbortFrame();
	GX_Flush();
}

void System_Wii::drawTextureGX(int x, int y) {
	Mtx m;
	guMtxIdentity(m);
	guMtxTrans(m, x, y, 0);
	GX_LoadPosMtxImm(m, GX_PNMTX0);

	const int x1 = _projLeft;
	const int y1 = _projTop;
	const int x2 = x1 + GAME_W;
	const int y2 = y1 + GAME_H;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

		GX_Position3s16(x1, y1, 0);
		GX_Color1u32(0xFFFFFFFF);
		GX_TexCoord2f32(0, 0);

		GX_Position3s16(x2, y1, 0);
		GX_Color1u32(0xFFFFFFFF);
		GX_TexCoord2f32(1, 0);

		GX_Position3s16(x2, y2, 0);
		GX_Color1u32(0xFFFFFFFF);
		GX_TexCoord2f32(1, 1);

		GX_Position3s16(x1, y2, 0);
		GX_Color1u32(0xFFFFFFFF);
		GX_TexCoord2f32(0, 1);

	GX_End();
}
