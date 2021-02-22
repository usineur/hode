
#include "scaler.h"

template <int N>
static void scale_nearest(uint32_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h, const uint32_t *palette) {
	while (h--) {
		uint32_t *p = dst;
		for (int i = 0; i < w; ++i, p += N) {
			const uint32_t c = palette[src[i]];
			for (int j = 0; j < N; ++j) {
				for (int k = 0; k < N; ++k) {
					*(p + j * dstPitch + k) = c;
				}
			}
		}
		dst += dstPitch * N;
		src += srcPitch;
	}
}

const Scaler scaler_nearest = {
	"nearest",
	2, 4,
	0,
	{ scale_nearest<2>, scale_nearest<3>, scale_nearest<4> }
};
