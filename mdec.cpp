
#include <math.h>
#include "intern.h"
#include "mdec.h"
#include "mdec_coeffs.h"

struct BitStream { // most significant 16 bits
	const uint8_t *_src;
	uint32_t _bits;
	int _len;
	const uint8_t *_end;

	BitStream(const uint8_t *src, int size)
		: _src(src), _bits(0), _len(0), _end(src + size) {
	}

	int bitsAvailable() const {
		return (_end - _src) * 8 + _len;
	}

	int getBits(int count) { // 6 to 16 bits
		if (_len < count) {
			_bits <<= 16;
			assert(_src < _end);
			_bits |= READ_LE_UINT16(_src); _src += 2;
			_len += 16;
		}
		assert(_len >= count);
		_len -= count;
		const int value = (_bits >> _len) & ((1 << count) - 1);
		return value;
	}
	int getSignedBits(int len) {
		const int shift = 32 - len;
		const int32_t value = getBits(len);
		return (value << shift) >> shift;
	}
	bool getBit() {
		if (_len == 0) {
			assert(_src < _end);
			_bits = READ_LE_UINT16(_src); _src += 2;
			_len = 16;
		}
		--_len;
		return (_bits & (1 << _len)) != 0;
	}
};

static int readDC(BitStream *bs, int version) {
	assert(version == 2);
	return bs->getSignedBits(10);
}

static void readAC(BitStream *bs, int *coefficients) {
	int count = 0;
	int node = 0;
	while (bs->bitsAvailable() > 0) {
		const uint16_t value = _acHuffTree[node].value;
		switch (value) {
		case kAcHuff_EscapeCode: {
				const int zeroes = bs->getBits(6);
				count += zeroes + 1;
				assert(count < 63);
				coefficients += zeroes;
				*coefficients++ = bs->getSignedBits(10);
			}
			break;
		case kAcHuff_EndOfBlock:
			return;
		case 0:
			if (bs->getBit()) {
				node = _acHuffTree[node].right;
			} else {
				node = _acHuffTree[node].left;
			}
			continue;
		default: {
				const int zeroes = value >> 8;
				count += zeroes + 1;
				assert(count < 63);
				coefficients += zeroes;
				const int nonZeroes = value & 255;
				*coefficients++ = bs->getBit() ? -nonZeroes : nonZeroes;
			}
			break;
		}
		node = 0; // root
	}
}

static const uint8_t _zigZagTable[8 * 8] = {
	 0,  1,  5,  6, 14, 15, 27, 28,
	 2,  4,  7, 13, 16, 26, 29, 42,
	 3,  8, 12, 17, 25, 30, 41, 43,
	 9, 11, 18, 24, 31, 40, 44, 53,
	10, 19, 23, 32, 39, 45, 52, 54,
	20, 22, 33, 38, 46, 51, 55, 60,
	21, 34, 37, 47, 50, 56, 59, 61,
	35, 36, 48, 49, 57, 58, 62, 63
};

static const uint8_t _quantizationTable[8 * 8] = {
	 2, 16, 19, 22, 26, 27, 29, 34,
	16, 16, 22, 24, 27, 29, 34, 37,
	19, 22, 26, 27, 29, 34, 34, 38,
	22, 22, 26, 27, 29, 34, 37, 40,
	22, 26, 27, 29, 32, 35, 40, 48,
	26, 27, 29, 32, 35, 40, 48, 58,
	26, 27, 29, 34, 38, 46, 56, 69,
	27, 29, 35, 38, 46, 56, 69, 83
};

static void dequantizeBlock(int *coefficients, float *block, int scale) {
	block[0] = coefficients[0] * _quantizationTable[0]; // DC
	for (int i = 1; i < 8 * 8; i++) {
		block[i] = coefficients[_zigZagTable[i]] * _quantizationTable[i] * scale / 8.f;
	}
}

static const int16_t _idct8x8[8][8] = {
	{ 23170,  32138,  30273,  27245,  23170,  18204,  12539,   6392 },
	{ 23170,  27245,  12539,  -6393, -23171, -32139, -30274, -18205 },
	{ 23170,  18204, -12540, -32139, -23171,   6392,  30273,  27245 },
	{ 23170,   6392, -30274, -18205,  23170,  27245, -12540, -32139 },
	{ 23170,  -6393, -30274,  18204,  23170, -27246, -12540,  32138 },
	{ 23170, -18205, -12540,  32138, -23171,  -6393,  30273, -27246 },
	{ 23170, -27246,  12539,   6392, -23171,  32138, -30274,  18204 },
	{ 23170, -32139,  30273, -27246,  23170, -18205,  12539,  -6393 }
};

static void idct(float *dequantData, float *result) {
	float tmp[8 * 8];
	// 1D IDCT rows
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			float p = 0;
			for (int i = 0; i < 8; ++i) {
				p += dequantData[i] * _idct8x8[x][i] / 0x10000;
			}
			tmp[y + x * 8] = p;
		}
		dequantData += 8;
	}
	// 1D IDCT columns
	for (int x = 0; x < 8; x++) {
		const float *u = tmp + x * 8;
		for (int y = 0; y < 8; y++) {
			float p = 0;
			for (int i = 0; i < 8; ++i) {
				p += u[i] * _idct8x8[y][i] / 0x10000;
			}
			result[y * 8 + x] = p;
		}
	}
}

static void decodeBlock(BitStream *bs, int x8, int y8, uint8_t *dst, int dstPitch, int scale, int version) {
	int coefficients[8 * 8];
	memset(coefficients, 0, sizeof(coefficients));
	coefficients[0] = readDC(bs, version);
	readAC(bs, &coefficients[1]);

	float dequantData[8 * 8];
	dequantizeBlock(coefficients, dequantData, scale);

	float idctData[8 * 8];
	idct(dequantData, idctData);

	dst += (y8 * dstPitch + x8) * 8;
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			const int val = (int)round(idctData[y * 8 + x]); // (-128,127) range
			dst[x] = (val <= -128) ? 0 : ((val >= 127) ? 255 : (128 + val));
		}
		dst += dstPitch;
	}
}

int decodeMDEC(const uint8_t *src, int len, const uint8_t *mbOrder, int mbLength, int w, int h, MdecOutput *out) {
	BitStream bs(src, len);
	bs.getBits(16);
	const uint16_t vlc = bs.getBits(16);
	assert(vlc == 0x3800);
	const uint16_t qscale = bs.getBits(16);
	const uint16_t version = bs.getBits(16);
	// fprintf(stdout, "mdec qscale %d version %d w %d h %d\n", qscale, version, w, h);
	assert(version == 2);

	const int blockW = (w + 15) / 16;
	const int blockH = (h + 15) / 16;

	const int yPitch = out->planes[kOutputPlaneY].pitch;
	uint8_t *yPtr = out->planes[kOutputPlaneY].ptr + out->y * yPitch + out->x;
	const int cbPitch = out->planes[kOutputPlaneCb].pitch;
	uint8_t *cbPtr = out->planes[kOutputPlaneCb].ptr + (out->y / 2) * cbPitch + (out->x / 2);
	const int crPitch = out->planes[kOutputPlaneCr].pitch;
	uint8_t *crPtr = out->planes[kOutputPlaneCr].ptr + (out->y / 2) * crPitch + (out->x / 2);

	int z = 0;
	for (int x = 0, x2 = 0; x < blockW; ++x, x2 += 2) {
		for (int y = 0, y2 = 0; y < blockH; ++y, y2 += 2) {
			if (z < mbLength) {
				const uint8_t xy = mbOrder[z];
				if ((xy & 15) != x || (xy >> 4) != y) {
					continue;
				}
				++z;
			}
			decodeBlock(&bs, x, y, crPtr, crPitch, qscale, version);
			decodeBlock(&bs, x, y, cbPtr, cbPitch, qscale, version);
			decodeBlock(&bs, x2,     y2,     yPtr, yPitch, qscale, version);
			decodeBlock(&bs, x2 + 1, y2,     yPtr, yPitch, qscale, version);
			decodeBlock(&bs, x2,     y2 + 1, yPtr, yPitch, qscale, version);
			decodeBlock(&bs, x2 + 1, y2 + 1, yPtr, yPitch, qscale, version);
			if (mbOrder && z == mbLength) {
				goto end;
			}
		}
	}
end:
	if (!mbOrder && bs.bitsAvailable() >= 11) {
		const int eof = bs.getBits(11);
		assert(eof == 0x3FE || eof == 0x3FF);
	}

	return bs._src - src;
}
