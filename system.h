/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SYSTEM_H__
#define SYSTEM_H__

#include "intern.h"

#define SYS_INP_UP    (1 << 0)
#define SYS_INP_RIGHT (1 << 1)
#define SYS_INP_DOWN  (1 << 2)
#define SYS_INP_LEFT  (1 << 3)
#define SYS_INP_RUN   (1 << 4) /* (1 << 0) */
#define SYS_INP_JUMP  (1 << 5) /* (1 << 1) */
#define SYS_INP_SHOOT (1 << 6) /* (1 << 2) */
#define SYS_INP_ESC   (1 << 7)

struct PlayerInput {
	uint8_t prevMask, mask;
	bool quit;
	bool screenshot;

	bool keyPressed(int keyMask) const {
		return (prevMask & keyMask) == 0 && (mask & keyMask) == keyMask;
	}
	bool keyReleased(int keyMask) const {
		return (prevMask & keyMask) == keyMask && (mask & keyMask) == 0;
	}
	void flush() {
		prevMask = mask = 0;
	}
};

struct AudioCallback {
	void (*proc)(void *param, int16_t *stream, int len);
	void *userdata;
};

struct System {
	PlayerInput inp, pad;

	virtual ~System() {}

	virtual void init(const char *title, int w, int h, bool fullscreen, bool widescreen, bool yuv) = 0;
	virtual void destroy() = 0;

	virtual void setScaler(const char *name, int multiplier) = 0;
	virtual void setGamma(float gamma) = 0;

	virtual void setPalette(const uint8_t *pal, int n, int depth = 8) = 0;
	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) = 0;
	virtual void copyYuv(int w, int h, const uint8_t *y, int ypitch, const uint8_t *u, int upitch, const uint8_t *v, int vpitch) = 0;
	virtual void fillRect(int x, int y, int w, int h, uint8_t color) = 0;
	virtual void copyRectWidescreen(int w, int h, const uint8_t *buf, const uint8_t *pal) = 0;
	virtual void shakeScreen(int dx, int dy) = 0;
	virtual void updateScreen(bool drawWidescreen) = 0;

	virtual void processEvents() = 0;
	virtual void sleep(int duration) = 0;
	virtual uint32_t getTimeStamp() = 0;

	virtual void startAudio(AudioCallback callback) = 0;
	virtual void stopAudio() = 0;
	virtual uint32_t getOutputSampleRate() = 0;
	virtual void lockAudio() = 0;
	virtual void unlockAudio() = 0;
	virtual AudioCallback setAudioCallback(AudioCallback callback) = 0;
};

extern System *const g_system;

#endif // SYSTEM_H__
