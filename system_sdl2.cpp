/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include <stdarg.h>
#include <math.h>
#include "scaler.h"
#include "system.h"
#include "util.h"

static const char *kIconBmp = "icon.bmp";

static const int kJoystickCommitValue = 3200;

static int _scalerMultiplier = 3;
static const Scaler *_scaler = &scaler_xbr;
static const float _gamma = 1.f;

static const int _pixelFormat = SDL_PIXELFORMAT_RGB888;

static const struct {
	const char *name;
	const Scaler *scaler;
} _scalers[] = {
	{ "nearest", &scaler_nearest },
	{ "xbr", &scaler_xbr },
	{ 0, 0 }
};

struct KeyMapping {
	int keyCode;
	int mask;
};

struct System_SDL2 : System {
	enum {
		kCopyRectsSize = 200,
		kKeyMappingsSize = 20,
		kAudioHz = 22050,
	};

	uint8_t *_offscreenLut;
	uint32_t *_offscreenRgb;
	SDL_Window *_window;
	SDL_Renderer *_renderer;
	SDL_Texture *_texture;
	int _texW, _texH;
	SDL_PixelFormat *_fmt;
	uint32_t _pal[256];
	int _screenW, _screenH;
	int _shakeDx, _shakeDy;
	SDL_Texture *_widescreenTexture;
	KeyMapping _keyMappings[kKeyMappingsSize];
	int _keyMappingsCount;
	AudioCallback _audioCb;
	uint8_t _gammaLut[256];
	SDL_GameController *_controller;
	SDL_Joystick *_joystick;

	System_SDL2();
	virtual ~System_SDL2() {}
	virtual void init(const char *title, int w, int h, bool fullscreen, bool widescreen);
	virtual void destroy();
	virtual void setScaler(const char *name, int multiplier);
	virtual void setGamma(float gamma);
	virtual void setPalette(const uint8_t *pal, int n, int depth);
	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch);
	virtual void fillRect(int x, int y, int w, int h, uint8_t color);
	virtual void copyRectWidescreen(int w, int h, const uint8_t *buf, const uint8_t *pal);
	virtual void shakeScreen(int dx, int dy);
	virtual void updateScreen(bool drawWidescreen);
	virtual void processEvents();
	virtual void sleep(int duration);
	virtual uint32_t getTimeStamp();

	virtual void startAudio(AudioCallback callback);
	virtual void stopAudio();
	virtual uint32_t getOutputSampleRate();
	virtual void lockAudio();
	virtual void unlockAudio();
	virtual AudioCallback setAudioCallback(AudioCallback callback);

	void addKeyMapping(int key, uint8_t mask);
	void setupDefaultKeyMappings();
	void updateKeys(PlayerInput *inp);
	void prepareScaledGfx(const char *caption, bool fullscreen, bool widescreen);
};

System *System_SDL2_create() {
	return new System_SDL2();
}

System_SDL2::System_SDL2() :
	_offscreenLut(0), _offscreenRgb(0),
	_window(0), _renderer(0), _texture(0), _fmt(0), _widescreenTexture(0),
	_controller(0), _joystick(0) {
	for (int i = 0; i < 256; ++i) {
		_gammaLut[i] = i;
	}
}

void System_SDL2::init(const char *title, int w, int h, bool fullscreen, bool widescreen) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	SDL_ShowCursor(SDL_DISABLE);
	setupDefaultKeyMappings();
	memset(&inp, 0, sizeof(inp));
	memset(&pad, 0, sizeof(pad));
	_screenW = w;
	_screenH = h;
	_shakeDx = _shakeDy = 0;
	memset(_pal, 0, sizeof(_pal));
	const int offscreenSize = w * h;
	_offscreenLut = (uint8_t *)malloc(offscreenSize);
	if (!_offscreenLut) {
		error("System_SDL2::init() Unable to allocate offscreen buffer");
	}
	_offscreenRgb = (uint32_t *)malloc(offscreenSize * sizeof(uint32_t));
	if (!_offscreenRgb) {
		error("System_SDL2::init() Unable to allocate RGB offscreen buffer");
	}
	memset(_offscreenLut, 0, offscreenSize);
	prepareScaledGfx(title, fullscreen, widescreen);
	_joystick = 0;
	_controller = 0;
	const int count = SDL_NumJoysticks();
	if (count > 0) {
		SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
		for (int i = 0; i < count; ++i) {
			if (SDL_IsGameController(i)) {
				_controller = SDL_GameControllerOpen(i);
				if (_controller) {
					fprintf(stdout, "Using controller '%s'\n", SDL_GameControllerName(_controller));
					break;
				}
			}
			_joystick = SDL_JoystickOpen(i);
			if (_joystick) {
				fprintf(stdout, "Using joystick '%s'", SDL_JoystickName(_joystick));
				break;
			}
		}
	}
}

void System_SDL2::destroy() {
	free(_offscreenLut);
	_offscreenLut = 0;
	free(_offscreenRgb);
	_offscreenRgb = 0;

	if (_fmt) {
		SDL_FreeFormat(_fmt);
		_fmt = 0;
	}
	if (_texture) {
		SDL_DestroyTexture(_texture);
		_texture = 0;
	}
	if (_widescreenTexture) {
		SDL_DestroyTexture(_widescreenTexture);
		_widescreenTexture = 0;
	}
	if (_renderer) {
		SDL_DestroyRenderer(_renderer);
		_renderer = 0;
	}
	if (_window) {
		SDL_DestroyWindow(_window);
		_window = 0;
	}

	if (_controller) {
		SDL_GameControllerClose(_controller);
		_controller = 0;
	}
	if (_joystick) {
		SDL_JoystickClose(_joystick);
		_joystick = 0;
	}
}


static void blur_h(int radius, const uint32_t *src, int srcPitch, int w, int h, const SDL_PixelFormat *fmt, uint32_t *dst, int dstPitch) {

	const int count = 2 * radius + 1;

	for (int y = 0; y < h; ++y) {

		uint32_t r = 0;
		uint32_t g = 0;
		uint32_t b = 0;

		uint32_t color;

		for (int x = -radius; x <= radius; ++x) {
			color = src[MAX(x, 0)];
			r += (color & fmt->Rmask) >> fmt->Rshift;
			g += (color & fmt->Gmask) >> fmt->Gshift;
			b += (color & fmt->Bmask) >> fmt->Bshift;
		}
		dst[0] = ((r / count) << fmt->Rshift) | ((g / count) << fmt->Gshift) | ((b / count) << fmt->Bshift);

		for (int x = 1; x < w; ++x) {
			color = src[MIN(x + radius, w - 1)];
			r += (color & fmt->Rmask) >> fmt->Rshift;
			g += (color & fmt->Gmask) >> fmt->Gshift;
			b += (color & fmt->Bmask) >> fmt->Bshift;

			color = src[MAX(x - radius - 1, 0)];
			r -= (color & fmt->Rmask) >> fmt->Rshift;
			g -= (color & fmt->Gmask) >> fmt->Gshift;
			b -= (color & fmt->Bmask) >> fmt->Bshift;

			dst[x] = ((r / count) << fmt->Rshift) | ((g / count) << fmt->Gshift) | ((b / count) << fmt->Bshift);
		}

		src += srcPitch;
		dst += dstPitch;
	}
}

static void blur_v(int radius, const uint32_t *src, int srcPitch, int w, int h, const SDL_PixelFormat *fmt, uint32_t *dst, int dstPitch) {

	const int count = 2 * radius + 1;

	for (int x = 0; x < w; ++x) {

		uint32_t r = 0;
		uint32_t g = 0;
		uint32_t b = 0;

		uint32_t color;

		for (int y = -radius; y <= radius; ++y) {
			color = src[MAX(y, 0) * srcPitch];
			r += (color & fmt->Rmask) >> fmt->Rshift;
			g += (color & fmt->Gmask) >> fmt->Gshift;
			b += (color & fmt->Bmask) >> fmt->Bshift;
		}
		dst[0] = ((r / count) << fmt->Rshift) | ((g / count) << fmt->Gshift) | ((b / count) << fmt->Bshift);

		for (int y = 1; y < h; ++y) {
			color = src[MIN(y + radius, h - 1) * srcPitch];
			r += (color & fmt->Rmask) >> fmt->Rshift;
			g += (color & fmt->Gmask) >> fmt->Gshift;
			b += (color & fmt->Bmask) >> fmt->Bshift;

			color = src[MAX(y - radius - 1, 0) * srcPitch];
			r -= (color & fmt->Rmask) >> fmt->Rshift;
			g -= (color & fmt->Gmask) >> fmt->Gshift;
			b -= (color & fmt->Bmask) >> fmt->Bshift;

			dst[y * dstPitch] = ((r / count) << fmt->Rshift) | ((g / count) << fmt->Gshift) | ((b / count) << fmt->Bshift);
		}

		++src;
		++dst;
	}
}

void System_SDL2::copyRectWidescreen(int w, int h, const uint8_t *buf, const uint8_t *pal) {
	if (!_widescreenTexture) {
		return;
	}

	assert(w == _screenW && h == _screenH);
	void *ptr = 0;
	int pitch = 0;
	if (SDL_LockTexture(_widescreenTexture, 0, &ptr, &pitch) == 0) {
		assert((pitch & 3) == 0);

		uint32_t *src = (uint32_t *)malloc(w * sizeof(uint32_t) * h * sizeof(uint32_t));
		uint32_t *tmp = (uint32_t *)malloc(w * sizeof(uint32_t) * h * sizeof(uint32_t));
		uint32_t *dst = (uint32_t *)ptr;

		if (src && tmp) {
			for (int i = 0; i < w * h; ++i) {
				const uint8_t color = buf[i];
				src[i] = SDL_MapRGB(_fmt, _gammaLut[pal[color * 3]], _gammaLut[pal[color * 3 + 1]], _gammaLut[pal[color * 3 + 2]]);
			}
			static const int radius = 8;
			blur_h(radius, src, w, w, h, _fmt, tmp, w);
			blur_v(radius, tmp, w, w, h, _fmt, dst, pitch / sizeof(uint32_t));
		}

		free(src);
		free(tmp);

		SDL_UnlockTexture(_widescreenTexture);
	}
}

void System_SDL2::setScaler(const char *name, int multiplier) {
	if (multiplier != 0) {
		_scalerMultiplier = multiplier;
	}
	if (name) {
		for (int i = 0; _scalers[i].name; ++i) {
			if (strcmp(name, _scalers[i].name) == 0) {
				_scaler = _scalers[i].scaler;
				break;
			}
		}
	}
}

void System_SDL2::setGamma(float gamma) {
	for (int i = 0; i < 256; ++i) {
		_gammaLut[i] = (uint8_t)round(pow(i / 255., 1. / gamma) * 255);
	}
}

void System_SDL2::setPalette(const uint8_t *pal, int n, int depth) {
	assert(n <= 256);
	assert(depth <= 8);
	const int shift = 8 - depth;
	for (int i = 0; i < n; ++i) {
		int r = pal[i * 3 + 0];
		int g = pal[i * 3 + 1];
		int b = pal[i * 3 + 2];
		if (shift != 0) {
			r = (r << shift) | (r >> depth);
			g = (g << shift) | (g >> depth);
			b = (b << shift) | (b >> depth);
		}
		r = _gammaLut[r];
		g = _gammaLut[g];
		b = _gammaLut[b];
		_pal[i] = SDL_MapRGB(_fmt, r, g, b);
	}
}

void System_SDL2::copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) {
	assert(x >= 0 && x + w <= _screenW && y >= 0 && y + h <= _screenH);
	for (int i = 0; i < h; ++i) {
		memcpy(_offscreenLut + y * _screenW + x, buf, w);
		buf += pitch;
		++y;
	}
}

void System_SDL2::fillRect(int x, int y, int w, int h, uint8_t color) {
	assert(x >= 0 && x + w <= _screenW && y >= 0 && y + h <= _screenH);
	for (int i = 0; i < h; ++i) {
		memset(_offscreenLut + y * _screenW + x, color, w);
		++y;
	}
}

void System_SDL2::shakeScreen(int dx, int dy) {
	_shakeDx = dx;
	_shakeDy = dy;
}

static void clearScreen(uint32_t *dst, int dstPitch, int x, int y, int w, int h) {
	uint32_t *p = dst + (y * dstPitch + x) * _scalerMultiplier;
	for (int j = 0; j < h * _scalerMultiplier; ++j) {
		memset(p, 0, w * sizeof(uint32_t) * _scalerMultiplier);
		p += dstPitch;
	}
}

void System_SDL2::updateScreen(bool drawWidescreen) {
	void *texturePtr = 0;
	int texturePitch = 0;
	if (SDL_LockTexture(_texture, 0, &texturePtr, &texturePitch) != 0) {
		return;
	}
	int w = _screenW;
	int h = _screenH;
	const uint8_t *src = _offscreenLut;
	uint32_t *dst = (uint32_t *)texturePtr;
	assert((texturePitch & 3) == 0);
	const int dstPitch = texturePitch / sizeof(uint32_t);
	const int srcPitch = _screenW;
	if (!_widescreenTexture) {
		if (_shakeDy > 0) {
			clearScreen(dst, dstPitch, 0, 0, w, _shakeDy);
			h -= _shakeDy;
			dst += _shakeDy * dstPitch;
		} else if (_shakeDy < 0) {
			h += _shakeDy;
			clearScreen(dst, dstPitch, 0, h, w, -_shakeDy);
			src -= _shakeDy * srcPitch;
		}
		if (_shakeDx > 0) {
			clearScreen(dst, dstPitch, 0, 0, _shakeDx, h);
			w -= _shakeDx;
			dst += _shakeDx;
		} else if (_shakeDx < 0) {
			w += _shakeDx;
			clearScreen(dst, dstPitch, w, 0, -_shakeDx, h);
			src -= _shakeDx;
		}
	}
	uint32_t *p = (_scalerMultiplier == 1) ? dst : _offscreenRgb;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			p[x] = _pal[src[y * w + x]];
		}
		p += w;
	}
	if (_scalerMultiplier != 1) {
		_scaler->scale(_scalerMultiplier, dst, dstPitch, _offscreenRgb, srcPitch, w, h);
	}
	SDL_UnlockTexture(_texture);

	SDL_RenderClear(_renderer);
	if (_widescreenTexture) {
		if (drawWidescreen) {
			SDL_RenderCopy(_renderer, _widescreenTexture, 0, 0);
		}
		SDL_Rect r;
		r.x = _shakeDx * _scalerMultiplier;
		r.y = _shakeDy * _scalerMultiplier;
		SDL_RenderGetLogicalSize(_renderer, &r.w, &r.h);
		r.x += (r.w - _texW) / 2;
		r.w = _texW;
		r.y += (r.h - _texH) / 2;
		r.h = _texH;
		SDL_RenderCopy(_renderer, _texture, 0, &r);
	} else {
		SDL_RenderCopy(_renderer, _texture, 0, 0);
	}
	SDL_RenderPresent(_renderer);
	_shakeDx = _shakeDy = 0;
}

void System_SDL2::processEvents() {
	SDL_Event ev;
	pad.prevMask = pad.mask;
	while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_KEYUP:
			if (ev.key.keysym.sym == SDLK_s) {
				inp.screenshot = true;
			}
			break;
		case SDL_JOYHATMOTION:
			if (_joystick) {
				pad.mask = 0;
				if (ev.jhat.value & SDL_HAT_UP) {
					pad.mask |= SYS_INP_UP;
				}
				if (ev.jhat.value & SDL_HAT_DOWN) {
					pad.mask |= SYS_INP_DOWN;
				}
				if (ev.jhat.value & SDL_HAT_LEFT) {
					pad.mask |= SYS_INP_LEFT;
				}
				if (ev.jhat.value & SDL_HAT_RIGHT) {
					pad.mask |= SYS_INP_RIGHT;
				}
			}
			break;
		case SDL_JOYAXISMOTION:
			if (_joystick) {
				switch (ev.jaxis.axis) {
				case 0:
					pad.mask &= ~(SYS_INP_RIGHT | SYS_INP_LEFT);
					if (ev.jaxis.value > kJoystickCommitValue) {
						pad.mask |= SYS_INP_RIGHT;
					} else if (ev.jaxis.value < -kJoystickCommitValue) {
						pad.mask |= SYS_INP_LEFT;
					}
					break;
				case 1:
					pad.mask &= ~(SYS_INP_UP | SYS_INP_DOWN);
					if (ev.jaxis.value > kJoystickCommitValue) {
						pad.mask |= SYS_INP_DOWN;
					} else if (ev.jaxis.value < -kJoystickCommitValue) {
						pad.mask |= SYS_INP_UP;
					}
					break;
				}
			}
			break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			if (_joystick) {
				const bool pressed = (ev.jbutton.state == SDL_PRESSED);
				switch (ev.jbutton.button) {
				case 0:
					if (pressed) {
						pad.mask |= SYS_INP_RUN;
					} else {
						pad.mask &= ~SYS_INP_RUN;
					}
					break;
				case 1:
					if (pressed) {
						pad.mask |= SYS_INP_JUMP;
					} else {
						pad.mask &= ~SYS_INP_JUMP;
					}
					break;
				case 2:
					if (pressed) {
						pad.mask |= SYS_INP_SHOOT;
					} else {
						pad.mask &= ~SYS_INP_SHOOT;
					}
					break;
				}
			}
			break;
		case SDL_CONTROLLERAXISMOTION:
			if (_controller) {
				switch (ev.caxis.axis) {
				case SDL_CONTROLLER_AXIS_LEFTX:
				case SDL_CONTROLLER_AXIS_RIGHTX:
					if (ev.caxis.value < -kJoystickCommitValue) {
						pad.mask |= SYS_INP_LEFT;
					} else {
						pad.mask &= ~SYS_INP_LEFT;
					}
					if (ev.caxis.value > kJoystickCommitValue) {
						pad.mask |= SYS_INP_RIGHT;
					} else {
						pad.mask &= ~SYS_INP_RIGHT;
					}
					break;
				case SDL_CONTROLLER_AXIS_LEFTY:
				case SDL_CONTROLLER_AXIS_RIGHTY:
					if (ev.caxis.value < -kJoystickCommitValue) {
						pad.mask |= SYS_INP_UP;
					} else {
						pad.mask &= ~SYS_INP_UP;
					}
					if (ev.caxis.value > kJoystickCommitValue) {
						pad.mask |= SYS_INP_DOWN;
					} else {
						pad.mask &= ~SYS_INP_DOWN;
					}
					break;
				}
			}
			break;
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			if (_controller) {
				const bool pressed = (ev.cbutton.state == SDL_PRESSED);
				switch (ev.cbutton.button) {
				case SDL_CONTROLLER_BUTTON_A:
					if (pressed) {
						pad.mask |= SYS_INP_RUN;
					} else {
						pad.mask &= ~SYS_INP_RUN;
					}
					break;
				case SDL_CONTROLLER_BUTTON_B:
					if (pressed) {
						pad.mask |= SYS_INP_JUMP;
					} else {
						pad.mask &= ~SYS_INP_JUMP;
					}
					break;
				case SDL_CONTROLLER_BUTTON_X:
					if (pressed) {
						pad.mask |= SYS_INP_SHOOT;
					} else {
						pad.mask &= ~SYS_INP_SHOOT;
					}
					break;
				case SDL_CONTROLLER_BUTTON_Y:
					break;
				case SDL_CONTROLLER_BUTTON_BACK:
				case SDL_CONTROLLER_BUTTON_START:
					inp.quit = pressed;
					break;
				case SDL_CONTROLLER_BUTTON_DPAD_UP:
					if (pressed) {
						pad.mask |= SYS_INP_UP;
					} else {
						pad.mask &= ~SYS_INP_UP;
					}
					break;
				case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
					if (pressed) {
						pad.mask |= SYS_INP_DOWN;
					} else {
						pad.mask &= ~SYS_INP_DOWN;
					}
					break;
				case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
					if (pressed) {
						pad.mask |= SYS_INP_LEFT;
					} else {
						pad.mask &= ~SYS_INP_LEFT;
					}
					break;
				case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
					if (pressed) {
						pad.mask |= SYS_INP_RIGHT;
					} else {
						pad.mask &= ~SYS_INP_RIGHT;
					}
					break;
				}
			}
			break;
		case SDL_QUIT:
			inp.quit = true;
			break;

		}
	}
	updateKeys(&inp);
}

void System_SDL2::sleep(int duration) {
	SDL_Delay(duration);
}

uint32_t System_SDL2::getTimeStamp() {
	return SDL_GetTicks();
}

static void mixAudioS16(void *param, uint8_t *buf, int len) {
	System_SDL2 *stub = (System_SDL2 *)param;
	memset(buf, 0, len);
	stub->_audioCb.proc(stub->_audioCb.userdata, (int16_t *)buf, len / 2);
}

void System_SDL2::startAudio(AudioCallback callback) {
	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));
	desired.freq = kAudioHz;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = 4096;
	desired.callback = mixAudioS16;
	desired.userdata = this;
	if (SDL_OpenAudio(&desired, 0) == 0) {
		_audioCb = callback;
		SDL_PauseAudio(0);
	} else {
		error("System_SDL2::startAudio() Unable to open sound device");
	}
}

void System_SDL2::stopAudio() {
	SDL_CloseAudio();
}

uint32_t System_SDL2::getOutputSampleRate() {
	return kAudioHz;
}

void System_SDL2::lockAudio() {
	SDL_LockAudio();
}

void System_SDL2::unlockAudio() {
	SDL_UnlockAudio();
}

AudioCallback System_SDL2::setAudioCallback(AudioCallback callback) {
	SDL_LockAudio();
	AudioCallback cb = _audioCb;
	_audioCb = callback;
	SDL_UnlockAudio();
	return cb;
}

void System_SDL2::addKeyMapping(int key, uint8_t mask) {
	if (_keyMappingsCount < kKeyMappingsSize) {
		for (int i = 0; i < _keyMappingsCount; ++i) {
			if (_keyMappings[i].keyCode == key) {
				_keyMappings[i].mask = mask;
				return;
			}
		}
		if (_keyMappingsCount < kKeyMappingsSize) {
			_keyMappings[_keyMappingsCount].keyCode = key;
			_keyMappings[_keyMappingsCount].mask = mask;
			++_keyMappingsCount;
		}
	}
}

void System_SDL2::setupDefaultKeyMappings() {
	_keyMappingsCount = 0;
	memset(_keyMappings, 0, sizeof(_keyMappings));

	/* original key mappings of the PC version */

	addKeyMapping(SDL_SCANCODE_LEFT,     SYS_INP_LEFT);
	addKeyMapping(SDL_SCANCODE_UP,       SYS_INP_UP);
	addKeyMapping(SDL_SCANCODE_RIGHT,    SYS_INP_RIGHT);
	addKeyMapping(SDL_SCANCODE_DOWN,     SYS_INP_DOWN);
//	addKeyMapping(SDL_SCANCODE_PAGEUP,   SYS_INP_UP | SYS_INP_RIGHT);
//	addKeyMapping(SDL_SCANCODE_HOME,     SYS_INP_UP | SYS_INP_LEFT);
//	addKeyMapping(SDL_SCANCODE_END,      SYS_INP_DOWN | SYS_INP_LEFT);
//	addKeyMapping(SDL_SCANCODE_PAGEDOWN, SYS_INP_DOWN | SYS_INP_RIGHT);

	addKeyMapping(SDL_SCANCODE_RETURN,   SYS_INP_JUMP);
	addKeyMapping(SDL_SCANCODE_LCTRL,    SYS_INP_RUN);
//	addKeyMapping(SDL_SCANCODE_f,        SYS_INP_RUN);
//	addKeyMapping(SDL_SCANCODE_LALT,     SYS_INP_JUMP);
//	addKeyMapping(SDL_SCANCODE_g,        SYS_INP_JUMP);
	addKeyMapping(SDL_SCANCODE_LSHIFT,   SYS_INP_SHOOT);
//	addKeyMapping(SDL_SCANCODE_h,        SYS_INP_SHOOT);
//	addKeyMapping(SDL_SCANCODE_d,        SYS_INP_SHOOT | SYS_INP_RUN);
//	addKeyMapping(SDL_SCANCODE_SPACE,    SYS_INP_SHOOT | SYS_INP_RUN);
	addKeyMapping(SDL_SCANCODE_ESCAPE,   SYS_INP_ESC);
}

void System_SDL2::updateKeys(PlayerInput *inp) {
	inp->prevMask = inp->mask;
	const uint8_t *keyState = SDL_GetKeyboardState(NULL);
	for (int i = 0; i < _keyMappingsCount; ++i) {
		KeyMapping *keyMap = &_keyMappings[i];
		if (keyState[keyMap->keyCode]) {
			inp->mask |= keyMap->mask;
		} else {
			inp->mask &= ~keyMap->mask;
		}
	}
	inp->mask |= pad.mask;
}

void System_SDL2::prepareScaledGfx(const char *caption, bool fullscreen, bool widescreen) {
	int flags = 0;
	if (fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else {
		flags |= SDL_WINDOW_RESIZABLE;
	}
	_texW = _screenW * _scalerMultiplier;
	_texH = _screenH * _scalerMultiplier;
	const int windowW = widescreen ? _texH * 16 / 9 : _texW;
	const int windowH = _texH;
	_window = SDL_CreateWindow(caption, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowW, windowH, flags);
	SDL_Surface *icon = SDL_LoadBMP(kIconBmp);
	if (icon) {
		SDL_SetWindowIcon(_window, icon);
		SDL_FreeSurface(icon);
	}
	_renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
	SDL_RenderSetLogicalSize(_renderer, windowW, windowH);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	_texture = SDL_CreateTexture(_renderer, _pixelFormat, SDL_TEXTUREACCESS_STREAMING, _texW, _texH);
	if (widescreen) {
		_widescreenTexture = SDL_CreateTexture(_renderer, _pixelFormat, SDL_TEXTUREACCESS_STREAMING, _screenW, _screenH);
	} else {
		_widescreenTexture = 0;
	}
	_fmt = SDL_AllocFormat(_pixelFormat);
}
