/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SCALER_H__
#define SCALER_H__

#include "intern.h"

typedef void (*ScaleProc32)(int factor, uint32_t *dst, int dstPitch, const uint32_t *src, int srcPitch, int w, int h);

#define SCALER_TAG 1

struct Scaler {
	uint32_t tag;
	const char *name;
	int factorMin, factorMax;
	ScaleProc32 scale;
};

extern const Scaler scaler_nearest;
extern const Scaler scaler_xbr;

#endif // SCALER_H__
