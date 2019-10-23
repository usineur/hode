
#include "scaler.h"

static void scale_nearest(int factor, uint32_t *dst, int dstPitch, const uint32_t *src, int srcPitch, int w, int h) {
	switch (factor) {
	case 2:
		while (h--) {
			uint32_t *p = dst;
			for (int i = 0; i < w; ++i, p += 2) {
				uint32_t c = *(src + i);
				*(p) = c;
				*(p + 1) = c;
				*(p + dstPitch) = c;
				*(p + dstPitch + 1) = c;
			}
			dst += dstPitch * 2;
			src += srcPitch;
		}
		break;
	case 3:
		while (h--) {
			uint32_t *p = dst;
			for (int i = 0; i < w; ++i, p += 3) {
				uint32_t c = *(src + i);
				*(p) = c;
				*(p + 1) = c;
				*(p + 2) = c;
				*(p + dstPitch) = c;
				*(p + dstPitch + 1) = c;
				*(p + dstPitch + 2) = c;
				*(p + 2 * dstPitch) = c;
				*(p + 2 * dstPitch + 1) = c;
				*(p + 2 * dstPitch + 2) = c;
			}
			dst += dstPitch * 3;
			src += srcPitch;
		}
		break;
	}
}

const Scaler scaler_nearest = {
	SCALER_TAG,
	"nearest",
	2, 3,
	scale_nearest
};
