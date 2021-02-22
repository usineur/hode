
#ifndef MDEC_H__
#define MDEC_H__

#include <stdint.h>

enum {
	kOutputPlaneY = 0,
	kOutputPlaneCb = 1,
	kOutputPlaneCr = 2
};

struct MdecOutput {
	int x, y;
	int w, h;
	struct {
		uint8_t *ptr;
		int pitch;
	} planes[3];
};

int decodeMDEC(const uint8_t *src, int len, const uint8_t *mbOrder, int mbLength, int w, int h, MdecOutput *out);

#endif // MDEC_H__
