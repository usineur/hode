
#include "game.h"
#include "lzw.h"
#include "system.h"
#include "util.h"
#include "video.h"

static uint32_t benchmarkLoop(const uint8_t *p, int count) {
	uint32_t accum = 0;
	count >>= 2; // sizeof(uint32_t)
	if (count > 0) {
		count += 1023;
		count >>= 10;
		for (; count > 0; --count) {
			accum += READ_LE_UINT32(p);
			p += (1 << 10) * sizeof(uint32_t);
		}
	}
	return accum;
}

uint32_t Game::benchmarkCpu() {
	const uint32_t t0 = g_system->getTimeStamp();
	uint8_t *p = _video->_shadowLayer;
	int count = 32;
	do {
		decodeLZW(_pwr1_screenTransformData, p);
		benchmarkLoop(p, Video::W * Video::H);
		decodeLZW(_pwr2_screenTransformData, p);
		_video->updateGameDisplay(p);
	} while (--count != 0);
	const uint32_t score = g_system->getTimeStamp() - t0;
	// The original engine flags the CPU as 'slow'
	// if the GetTickCount difference is >= 1100ms
	warning("benchmark CPU %d", score);
	return score;
}
