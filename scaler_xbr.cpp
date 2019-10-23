
#include "scaler.h"
extern "C" {
#include "3p/libxbr-standalone/filters.h"
}

static bool _xbr_init;
static xbr_data _xbr_data;

static void scale_xbr(int factor, uint32_t *dst, int dstPitch, const uint32_t *src, int srcPitch, int w, int h) {
	if (!_xbr_init) {
		xbr_init_data(&_xbr_data);
		_xbr_init = true;
	}
	xbr_params params;
	params.input = (uint8_t *)src;
	params.output = (uint8_t *)dst;
	params.inWidth = w;
	params.inHeight = h;
	params.inPitch = srcPitch * sizeof(uint32_t);
	params.outPitch = dstPitch * sizeof(uint32_t);
	params.data = &_xbr_data;
	switch (factor) {
	case 2:
		xbr_filter_xbr2x(&params);
		break;
	case 3:
		xbr_filter_xbr3x(&params);
		break;
	case 4:
		xbr_filter_xbr4x(&params);
		break;
	}
}

const Scaler scaler_xbr = {
	SCALER_TAG,
	"xbr",
	2, 4,
	scale_xbr
};
