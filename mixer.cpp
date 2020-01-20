
#include "mixer.h"
#include "util.h"

static void nullMixerLock(int lock) {
}

Mixer::Mixer()
	: _lock(nullMixerLock) {
}

Mixer::~Mixer() {
}

void Mixer::init(int rate) {
	_rate = rate;
	memset(_mixingQueue, 0, sizeof(_mixingQueue));
	_mixingQueueSize = 0;
}

void Mixer::fini() {
}

void Mixer::queue(const int16_t *ptr, const int16_t *end, int panType, int panL, int panR, bool stereo) {
	if (_mixingQueueSize >= kPcmChannels) {
		warning("MixingQueue overflow %d", _mixingQueueSize);
		return;
	}
	MixerChannel *channel = &_mixingQueue[_mixingQueueSize];
	channel->ptr = ptr;
	channel->end = end;
	channel->panL = panL;
	channel->panR = panR;
	channel->panType = panType;
	channel->stereo = stereo;
	++_mixingQueueSize;
}

template <bool stereo, int panning>
static void mixS16(int16_t *dst, const int16_t *src, int len, int panL, int panR) {

	static const int kPanBits = 14; // 0..16384

	for (int j = 0; j < len; j += 2, dst += 2) {
		const int16_t sampleL = *src++;
		const int16_t sampleR = stereo ? *src++ : sampleL;
		if (panning != 1) {
			dst[0] = CLIP(dst[0] + ((panL * sampleL) >> kPanBits), -32768, 32767);
		}
		if (panning != 2) {
			dst[1] = CLIP(dst[1] + ((panR * sampleR) >> kPanBits), -32768, 32767);
		}
	}
}

void Mixer::mix(int16_t *buf, int len) {
	// stereo s16
	assert((len & 1) == 0);
	if (_mixingQueueSize == 0) {
		return;
	}
	for (int i = 0; i < _mixingQueueSize; ++i) {
		const MixerChannel *channel = &_mixingQueue[i];
		const int panL = channel->panL;
		const int panR = channel->panR;
		if (channel->stereo) {
			assert(channel->ptr + len <= channel->end);
			switch (channel->panType) {
			case 1:
				mixS16<true, 1>(buf, channel->ptr, len, panL, panR);
				break;
			case 2:
				mixS16<true, 2>(buf, channel->ptr, len, panL, panR);
				break;
			default:
				mixS16<true, 0>(buf, channel->ptr, len, panL, panR);
				break;
			}
		} else {
			assert(channel->ptr + len / 2 <= channel->end);
			switch (channel->panType) {
			case 1:
				mixS16<false, 1>(buf, channel->ptr, len, panL, panR);
				break;
			case 2:
				mixS16<false, 2>(buf, channel->ptr, len, panL, panR);
				break;
			default:
				mixS16<false, 0>(buf, channel->ptr, len, panL, panR);
				break;
			}
		}
	}
}
