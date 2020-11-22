
#include <pspaudio.h>
#include <pspaudiolib.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include "system.h"

struct System_PSP : System {

	uint32_t _buttons;
	int _shakeDx, _shakeDy;
	bool _stretchScreen;
	AudioCallback _audioCb;
	int _audioChannel;
	SceUID _audioMutex;
	uint32_t _vramOffset;

	System_PSP();
	virtual ~System_PSP();
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
};

static System_PSP system_psp;
System *const g_system = &system_psp;

// static const int AUDIO_FREQ = 44100;
static const int AUDIO_SAMPLES_COUNT = 2048;

static const int SCREEN_W = 480;
static const int SCREEN_H = 272;
static const int SCREEN_PITCH = 512;

static const int GAME_W = 256;
static const int GAME_H = 192;

static const int BLUR_TEX_W = 16;
static const int BLUR_TEX_H = 16;

static uint32_t WHITE_COLOR = GU_RGBA(255, 255, 255, 255);

static uint32_t __attribute__((aligned(16))) _dlist[1024];
static uint32_t __attribute__((aligned(16))) _clut[256];
static uint8_t  __attribute__((aligned(16))) _texture[256 * 256];
static uint32_t __attribute__((aligned(16))) _clut2[256];
static uint8_t  __attribute__((aligned(16))) _texture2[256 * 256];

PSP_MODULE_INFO("Heart of Darkness", 0, 2, 9);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

struct Vertex {
	uint16_t u, v;
	int16_t x, y, z;
};

static void initVertex2D(Vertex *vertices, int w, int h, int targetW, int targetH) {
	vertices[0].u = 0; vertices[1].u = w;
	vertices[0].v = 0; vertices[1].v = h;
	vertices[0].x = 0; vertices[1].x = targetW;
	vertices[0].y = 0; vertices[1].y = targetH;
	vertices[0].z = 0; vertices[1].z = 0;
}

void System_printLog(FILE *fp, const char *s) {
	if (fp == stderr) {
		static bool firstOpen = false;
		if (!firstOpen) {
			fp = fopen("stderr.txt", "w");
			firstOpen = true;
		} else {
			fp = fopen("stderr.txt", "a");
		}
	} else if (fp == stdout) {
		static bool firstOpen = false;
		if (!firstOpen) {
			fp = fopen("stdout.txt", "w");
			firstOpen = true;
		} else {
			fp = fopen("stdout.txt", "a");
		}
	} else {
		return;
	}
	fprintf(fp, "%s\n", s);
	fclose(fp);
}

void System_fatalError(const char *s) {
	sceKernelExitGame();
}

bool System_hasCommandLine() {
	return false;
}

static int exitCallback(int arg1, int arg2, void *common) {
	g_system->inp.quit = true;
	return 0;
}

static int callbackThread(SceSize args, void *argp) {
	const int cb = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
	sceKernelRegisterExitCallback(cb);
	sceKernelSleepThreadCB();
	return 0;
}

System_PSP::System_PSP() {
}

System_PSP::~System_PSP() {
}

void System_PSP::init(const char *title, int w, int h, bool fullscreen, bool widescreen, bool yuv) {

	memset(&inp, 0, sizeof(inp));
	memset(&pad, 0, sizeof(pad));
	_buttons = 0;
	_shakeDx = _shakeDy = 0;
	_stretchScreen = false; // keep 4:3 AR

	memset(&_audioCb, 0, sizeof(_audioCb));
	_audioChannel = -1;
	_audioMutex = 0;

	const int th = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, 0);
	if (th >= 0) {
		sceKernelStartThread(th, 0, 0);
	}

	sceKernelDcacheWritebackAll();

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	sceGuInit();
	sceGuStart(GU_DIRECT, _dlist);

	const int fbSize = SCREEN_PITCH * SCREEN_H * sizeof(uint32_t); // rgba
	const int zbSize = SCREEN_PITCH * SCREEN_H * sizeof(uint16_t); // 16 bits
	_vramOffset = 0;
	sceGuDrawBuffer(GU_PSM_8888, (void *)_vramOffset, SCREEN_PITCH); _vramOffset += fbSize;
	sceGuDispBuffer(SCREEN_W, SCREEN_H, (void *)_vramOffset, SCREEN_PITCH); _vramOffset += fbSize;
	sceGuDepthBuffer((void *)_vramOffset, SCREEN_PITCH); _vramOffset += zbSize;

	sceGuOffset(2048 - (SCREEN_W / 2), 2048 - (SCREEN_H / 2));
	sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
	sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

	sceGuFinish();
	sceGuSync(GU_SYNC_WHAT_DONE, GU_SYNC_FINISH);

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
}

void System_PSP::destroy() {
	sceGuTerm();
	sceKernelExitGame();
}

void System_PSP::setScaler(const char *name, int multiplier) {
}

void System_PSP::setGamma(float gamma) {
}

void System_PSP::setPalette(const uint8_t *pal, int n, int depth) {
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
		_clut[i] = GU_RGBA(r, g, b, 255);
	}
	sceKernelDcacheWritebackRange(_clut, sizeof(_clut));
}

void System_PSP::clearPalette() {
	for (int i = 0; i < 256; ++i) {
		_clut[i] = GU_RGBA(0, 0, 0, 255);
	}
	sceKernelDcacheWritebackRange(_clut, sizeof(_clut));
}

void System_PSP::copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) {
	uint8_t *p = _texture + y * GAME_W + x;
	if (w == GAME_W && w == pitch) {
		memcpy(p, buf, w * h);
	} else {
		for (int i = 0; i < h; ++i) {
			memcpy(p, buf, w);
			p += GAME_W;
			buf += pitch;
		}
	}
	sceKernelDcacheWritebackRange(_texture, sizeof(_texture));
}

void System_PSP::copyYuv(int w, int h, const uint8_t *y, int ypitch, const uint8_t *u, int upitch, const uint8_t *v, int vpitch) {
}

void System_PSP::fillRect(int x, int y, int w, int h, uint8_t color) {
	uint8_t *p = _texture + y * GAME_W + x;
	if (w == GAME_W) {
		memset(p, color, w * h);
	} else {
		for (int i = 0; i < h; ++i) {
			memset(p, color, w);
			p += GAME_W;
		}
	}
	sceKernelDcacheWritebackRange(_texture, sizeof(_texture));
}

void System_PSP::copyRectWidescreen(int w, int h, const uint8_t *buf, const uint8_t *pal) {
	for (int i = 0; i < 256; ++i) {
		_clut2[i] = GU_RGBA(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2], 255);
	}
	sceKernelDcacheWritebackRange(_clut2, sizeof(_clut2));
	memcpy(_texture2, buf, w * h);
	sceKernelDcacheWritebackRange(_texture2, sizeof(_texture2));

	sceGuStart(GU_DIRECT, _dlist);

	sceGuDrawBufferList(GU_PSM_8888, (void *)_vramOffset, BLUR_TEX_W);
	sceGuOffset(2048 - (BLUR_TEX_W / 2), 2048 - (BLUR_TEX_H / 2));
	sceGuViewport(2048, 2048, BLUR_TEX_W, BLUR_TEX_H);
	sceGuClearColor(WHITE_COLOR);
	sceGuClear(GU_COLOR_BUFFER_BIT);

	sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
	sceGuClutLoad(256 / 8, _clut2);
	sceGuTexMode(GU_PSM_T8, 0, 0, 0);
	sceGuTexImage(0, 256, 256, 256, _texture2);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);

	Vertex *vertices = (Vertex *)sceGuGetMemory(2 * sizeof(struct Vertex));
	initVertex2D(vertices, GAME_W, GAME_H, BLUR_TEX_W, BLUR_TEX_H);
	sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);

	sceGuFinish();
}

void System_PSP::shakeScreen(int dx, int dy) {
	_shakeDx = dx;
	_shakeDy = dy;
}

void System_PSP::updateScreen(bool drawWidescreen) {

	sceGuStart(GU_DIRECT, _dlist);

	sceGuClearColor(0);
	sceGuClear(GU_COLOR_BUFFER_BIT);

	if (!_stretchScreen && drawWidescreen) {

		sceGuTexMode(GU_PSM_8888, 0, 0, 0);
		sceGuTexImage(0, BLUR_TEX_W, BLUR_TEX_H, BLUR_TEX_W, (uint8_t *)sceGeEdramGetAddr() + _vramOffset);
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
		sceGuTexFilter(GU_LINEAR, GU_LINEAR);

		Vertex *vertices = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
		initVertex2D(vertices, BLUR_TEX_W, BLUR_TEX_H, SCREEN_W, SCREEN_H);
		sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
	sceGuClutLoad(256 / 8, _clut);
	sceGuTexMode(GU_PSM_T8, 0, 0, 0);
	sceGuTexImage(0, 256, 256, 256, _texture);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuTexFilter(GU_LINEAR, GU_LINEAR);

	Vertex *vertices = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
	initVertex2D(vertices, GAME_W, GAME_H, SCREEN_W, SCREEN_H);
	vertices[0].x += _shakeDx;
	vertices[1].x += _shakeDx;
	vertices[0].y += _shakeDy;
	vertices[1].y += _shakeDy;
	if (!_stretchScreen) {
		const int w = (SCREEN_H * GAME_W / GAME_H);
		const int dx = (SCREEN_W - w) / 2;
		vertices[0].x += dx;
		vertices[1].x -= dx;
	}
	sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);

	sceGuFinish();
	sceGuSync(GU_SYNC_WHAT_DONE, GU_SYNC_FINISH);

	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

void System_PSP::processEvents() {
	inp.prevMask = inp.mask;
	inp.mask = 0;

	static const struct {
		int psp;
		int sys;
	} mapping[] = {
		{ PSP_CTRL_UP, SYS_INP_UP },
		{ PSP_CTRL_RIGHT, SYS_INP_RIGHT },
		{ PSP_CTRL_DOWN, SYS_INP_DOWN },
		{ PSP_CTRL_LEFT, SYS_INP_LEFT },
		{ PSP_CTRL_CROSS, SYS_INP_JUMP },
		{ PSP_CTRL_SQUARE, SYS_INP_RUN },
		{ PSP_CTRL_CIRCLE, SYS_INP_SHOOT },
		{ PSP_CTRL_TRIANGLE, SYS_INP_SHOOT | SYS_INP_RUN },
		{ PSP_CTRL_START, SYS_INP_ESC },
		{ 0, 0 }
	};
	SceCtrlData data;
	sceCtrlPeekBufferPositive(&data, 1);
	for (int i = 0; mapping[i].psp != 0; ++i) {
		if (data.Buttons & mapping[i].psp) {
			inp.mask |= mapping[i].sys;
		}
	}
	static const int lxMargin = 64;
	if (data.Lx < 127 - lxMargin) {
		inp.mask |= SYS_INP_LEFT;
	} else if (data.Lx > 127 + lxMargin) {
		inp.mask |= SYS_INP_RIGHT;
	}
	static const int lyMargin = 64;
	if (data.Ly < 127 - lyMargin) {
		inp.mask |= SYS_INP_UP;
	} else if (data.Ly > 127 + lyMargin) {
		inp.mask |= SYS_INP_DOWN;
	}

	const uint32_t mask = data.Buttons ^ _buttons;
	if ((data.Buttons & PSP_CTRL_LTRIGGER) & mask) {
		_stretchScreen = !_stretchScreen;
	}
	if ((data.Buttons & PSP_CTRL_RTRIGGER) & mask) {
	}
	_buttons = data.Buttons;
}

void System_PSP::sleep(int duration) {
	sceKernelDelayThread(duration * 1000);
}

uint32_t System_PSP::getTimeStamp() {
	struct timeval tv;
	sceKernelLibcGettimeofday(&tv, 0);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void audioCallback(void *buf, unsigned int samples, void *userdata) { // 44100hz S16 stereo
	int16_t buf22khz[samples];
	memset(buf22khz, 0, sizeof(buf22khz));
	system_psp.lockAudio();
	(system_psp._audioCb.proc)(system_psp._audioCb.userdata, buf22khz, samples);
	system_psp.unlockAudio();
	uint32_t *buf44khz = (uint32_t *)buf;
	static int16_t prev;
	for (unsigned int i = 0; i < samples; ++i) {
		const int16_t current = buf22khz[i];
		buf44khz[i] = (current << 16) | (((prev + current) >> 1) & 0xFFFF);
		prev = current;
	}
}

void System_PSP::startAudio(AudioCallback callback) {
	// sceAudioSetFrequency(AUDIO_FREQ);
	pspAudioInit();
	_audioCb = callback;
	_audioMutex = sceKernelCreateSema("audio_lock", 0, 1, 1, 0);
	_audioChannel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_SAMPLES_COUNT, PSP_AUDIO_FORMAT_STEREO);
	pspAudioSetChannelCallback(_audioChannel, audioCallback, 0);
}

void System_PSP::stopAudio() {
	sceAudioChRelease(_audioChannel);
	sceKernelDeleteSema(_audioMutex);
	pspAudioEnd();
}

void System_PSP::lockAudio() {
	sceKernelWaitSema(_audioMutex, 1, 0);
}

void System_PSP::unlockAudio() {
	sceKernelSignalSema(_audioMutex, 1);
}

AudioCallback System_PSP::setAudioCallback(AudioCallback callback) {
	AudioCallback cb = _audioCb;
	lockAudio();
	_audioCb = callback;
	unlockAudio();
	return cb;
}
