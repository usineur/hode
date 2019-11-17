/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "video.h"
#include "system.h"

static const bool kUseShadowColorLut = false;
static const bool _findBlackColor = false;

Video::Video(System *system)
	: _system(system) {
	_displayShadowLayer = false;
	_drawLine.x1 = 0;
	_drawLine.y1 = 0;
	_drawLine.x2 = W - 1;
	_drawLine.y2 = H - 1;
	_drawLine.pitch = W;
	_shadowLayer = (uint8_t *)malloc(W * H + 1); // projectionData offset can be equal to W * H
	_frontLayer = (uint8_t *)malloc(W * H);
	_backgroundLayer = (uint8_t *)malloc(W * H);
	_spr.pitch = W;
	_spr.x = 0;
	_spr.y = 0;
	_spr.w = W;
	_spr.h = H;
	if (kUseShadowColorLut) {
		_shadowColorLookupTable = (uint8_t *)malloc(256 * 256); // shadowLayer, frontLayer
	} else {
		_shadowColorLookupTable = 0;
	}
	_shadowScreenMaskBuffer = (uint8_t *)malloc(256 * 192 * 2 + 256 * 4);
	_transformShadowBuffer = 0;
	_transformShadowLayerDelta = 0;
	_fillColor = 0xC4;
	_blackColor = 255;
}

Video::~Video() {
	free(_shadowLayer);
	free(_frontLayer);
	free(_backgroundLayer);
	free(_shadowColorLookupTable);
	free(_shadowScreenMaskBuffer);
}

static int colorBrightness(int r, int g, int b) {
	return (r + g * 2) * 19 + b * 7;
}

void Video::refreshGamePalette(const uint16_t *pal) {
	_refreshPalette = true;
	for (int i = 0; i < 256 * 3; ++i) {
		_palette[i] = pal[i] >> 8;
	}
	int blackQuant = INT32_MAX;
	_blackColor = 255;
	if (_findBlackColor) {
		const int r = _palette[255 * 3];
		const int g = _palette[255 * 3 + 1];
		const int b = _palette[255 * 3 + 2];
		for (int i = 1; i < 255; ++i) {
			const int q = colorBrightness(ABS(_palette[i * 3] - r), ABS(_palette[i * 3 + 1] - g), ABS(_palette[i * 3 + 2] - b));
			if (q < blackQuant) {
				blackQuant = q;
				_blackColor = i;
			}
		}
	}
	_system->setPalette(_palette, 256);
}

void Video::updateGameDisplay(uint8_t *buf) {
	if (_findBlackColor) {
		for (int i = 0; i < H * W; ++i) {
			if (buf[i] == 255) {
				buf[i] = _blackColor;
			}
		}
	}
	_system->copyRect(0, 0, W, H, buf, 256);
}

void Video::updateScreen() {
	_system->updateScreen(true);
}

void Video::fillBackBuffer() {
	_system->fillRect(0, 0, W, H, _fillColor);
}

void Video::clearPalette() {
	memset(_palette, 0, sizeof(_palette));
	_refreshPalette = true;
	_system->setPalette(_palette, 256);
}

void Video::decodeSPR(const uint8_t *src, uint8_t *dst, int x, int y, uint8_t flags, uint16_t spr_w, uint16_t spr_h) {
	if (y >= _spr.h) {
		return;
	} else if (y < _spr.y) {
		flags |= kSprClipTop;
	}
	const int y2 = y + spr_h - 1;
	if (y2 < _spr.y) {
		return;
	} else if (y2 >= _spr.h) {
		flags |= kSprClipBottom;
	}

	if (x >= _spr.w) {
		return;
	} else if (x < _spr.x) {
		flags |= kSprClipLeft;
	}
	const int x2 = x + spr_w - 1;
	if (x2 < _spr.x) {
		return;
	} else if (x2 >= _spr.w) {
		flags |= kSprClipRight;
	}

	if (flags & kSprHorizFlip) {
		x = x2;
	}
	if (flags & kSprVertFlip) {
		y = y2;
	}
	const int xOrig = x;
	while (1) {
		uint8_t *p = dst + y * _spr.pitch + x;
		int code = *src++;
		int count = code & 0x3F;
		int clippedCount = count;
		if (y < _spr.y || y >= _spr.h) {
			clippedCount = 0;
		}
		switch (code >> 6) {
		case 0:
			if ((flags & (kSprHorizFlip | kSprClipLeft | kSprClipRight)) == 0) {
				memcpy(p, src, clippedCount);
				x += count;
			} else if (flags & kSprHorizFlip) {
				for (int i = 0; i < clippedCount; ++i) {
					if (x - i >= _spr.x && x - i < _spr.w) {
						p[-i] = src[i];
					}
				}
				x -= count;
			} else {
				for (int i = 0; i < clippedCount; ++i) {
					if (x + i >= _spr.x && x + i < _spr.w) {
						p[i] = src[i];
					}
				}
				x += count;
			}
			src += count;
			break;
		case 1:
			code = *src++;
			if ((flags & (kSprHorizFlip | kSprClipLeft | kSprClipRight)) == 0) {
				memset(p, code, clippedCount);
				x += count;
			} else if (flags & kSprHorizFlip) {
				for (int i = 0; i < clippedCount; ++i) {
					if (x - i >= _spr.x && x - i < _spr.w) {
						p[-i] = code;
					}
				}
				x -= count;
			} else {
				for (int i = 0; i < clippedCount; ++i) {
					if (x + i >= _spr.x && x + i < _spr.w) {
						p[i] = code;
					}
				}
				x += count;
			}
			break;
		case 2:
			if (count == 0) {
				count = *src++;
			}
			if (flags & kSprHorizFlip) {
				x -= count;
			} else {
				x += count;
			}
			break;
		case 3:
			if (count == 0) {
				count = *src++;
				if (count == 0) {
					return;
				}
			}
			if (flags & kSprVertFlip) {
				y -= count;
			} else {
				y += count;
			}
			if (flags & kSprHorizFlip) {
				x = xOrig - *src++;
			} else {
				x = xOrig + *src++;
			}
			break;
		}
	}
}

void Video::decodeRLE(const uint8_t *src, uint8_t *dst, int size) {
	int count;

	while (size > 0) {
		int8_t code = *src++;
		if (code < 0) {
			count = 1 - code;
			const uint8_t color = *src++;
			memset(dst, color, count);
		} else {
			count = code + 1;
			memcpy(dst, src, count);
			src += count;
		}
		dst += count;
		size -= count;
	}
	assert(size == 0);
}

// https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
int Video::computeLineOutCode(int x, int y) {
	int mask = 0;
	if (y > _drawLine.y2) mask |= 1 << 24;
	if (x > _drawLine.x2) mask |= 1 << 16;
	if (y < _drawLine.y1) mask |= 1 <<  8;
	if (x < _drawLine.x1) mask |= 1;
	return mask;
}

bool Video::clipLineCoords(int &x1, int &y1, int &x2, int &y2) {
	int mask1 = computeLineOutCode(x2, y2);
	while (1) {
		const int mask2 = computeLineOutCode(x1, y1);
		if (mask2 == 0 && mask1 == 0) {
			break;
		}
		if ((mask1 & mask2) != 0) {
			return true;
		}
		if (mask2 & 1) { // (x < _drawLine.x1)
			y1 += (y2 - y1) * (_drawLine.x1 - x1) / (x2 - x1);
			x1 = _drawLine.x1;
			continue;
		}
		if (mask2 & 0x100) { // (y < _drawLine.y1)
			x1 += (x2 - x1) * (_drawLine.y1 - y1) / (y2 - y1);
			y1 = _drawLine.y1;
			continue;
		}
		if (mask2 & 0x10000) { // (x > _drawLine.x2)
			y1 += (y2 - y1) * (_drawLine.x2 - x1) / (x2 - x1);
			x1 = _drawLine.x2;
			continue;
		}
		if (mask2 & 0x1000000) { // (y > _drawLine.y2)
			x1 += (x2 - x1) * (_drawLine.y2 - y1) / (y2 - y1);
			y1 = _drawLine.y2;
			continue;
		}
		SWAP(x1, x2);
		SWAP(y1, y2);
		assert(mask2 == 0);
		mask1 = 0;
	}
	return false;
}

void Video::drawLine(int x1, int y1, int x2, int y2) {
	if (clipLineCoords(x1, y1, x2, y2)) {
		return;
	}
	assert(x1 >= _drawLine.x1 && x1 <= _drawLine.x2);
	assert(y1 >= _drawLine.y1 && y1 <= _drawLine.y2);
	assert(x2 >= _drawLine.x1 && x2 <= _drawLine.x2);
	assert(y2 >= _drawLine.y1 && y2 <= _drawLine.y2);
	int dstPitch = _drawLine.pitch;
	int dx = x2 - x1;
	if (dx == 0) {
		int dy = y2 - y1;
		if (dy < 0) {
			y1 += dy;
			dy = -dy;
		}
		uint8_t *dst = _frontLayer + y1 * _drawLine.pitch + x1;
		for (int i = 0; i <= dy; ++i) {
			*dst = _drawLine.color;
			dst += dstPitch;
		}
		return;
	}
	if (dx < 0) {
		x1 += dx;
		dx = -dx;
		SWAP(y1, y2);
	}
	uint8_t *dst = _frontLayer + y1 * _drawLine.pitch + x1;
	int dy = y2 - y1;
	if (dy == 0) {
		memset(dst, _drawLine.color, dx);
		return;
	}
	if (dy < 0) {
		dy = -dy;
		dstPitch = -dstPitch;
	}
	int step = 0;
	if (dx > dy) {
		SWAP(dx, dy);
		dx *= 2;
		const int stepInc = dy * 2;
		step -= stepInc;
		for (int i = 0; i <= dy; ++i) {
			*dst = _drawLine.color;
			step += dx;
			if (step >= 0) {
				step -= stepInc;
				dst += dstPitch;
			}
			++dst;
		}
	} else {
		dx *= 2;
		const int stepInc = dy * 2;
		step -= stepInc;
		for (int i = 0; i <= dy; ++i) {
			*dst = _drawLine.color;
			step += dx;
			if (step >= 0) {
				step -= stepInc;
				++dst;
			}
			dst += dstPitch;
		}
	}
}

static uint8_t lookupColor(uint8_t a, uint8_t b, const uint8_t *lut) {
	return (a >= 144 && b < 144) ? lut[b] : b;
}

void Video::applyShadowColors(int x, int y, int src_w, int src_h, int dst_pitch, int src_pitch, uint8_t *dst1, uint8_t *dst2, uint8_t *src1, uint8_t *src2) {
	assert(dst1 == _shadowLayer);
	assert(dst2 == _frontLayer);
	// src1 == projectionData
	// src2 == shadowPalette

	dst2 += y * dst_pitch + x;
	src2 = _shadowColorLookupTable;
	for (int j = 0; j < src_h; ++j) {
		for (int i = 0; i < src_w; ++i) {
			int offset = READ_LE_UINT16(src1); src1 += 2;
			assert(offset <= W * H);
			if (!src2) { // no LUT
				dst2[i] = lookupColor(_shadowLayer[offset], dst2[i], _shadowColorLut);
			} else {
				// build lookup offset
				//   msb : _shadowLayer[ _projectionData[ (x, y) ] ]
				//   lsb : _frontLayer[ (x, y) ]
				offset = (dst1[offset] << 8) | dst2[i];

				// lookup color matrix
				//   if msb < 144 : _frontLayer.color
				//   if msb >= 144 : if _frontLayer.color < 144 ? shadowPalette[ _frontLayer.color ] : _frontLayer.color
				dst2[i] = src2[offset];
			}
		}
		dst2 += dst_pitch;
	}
}

void Video::buildShadowColorLookupTable(const uint8_t *src, uint8_t *dst) {
	assert(dst == _shadowColorLookupTable);
	if (_shadowColorLookupTable) {
		// 256x256
		//   0..143 : 0..255
		// 144..255 : src[0..143] 144..255
		for (int i = 0; i < 144; ++i) {
			for (int j = 0; j < 256; ++j) {
				*dst++ = j;
			}
		}
		for (int i = 0; i < 112; ++i) {
			memcpy(dst, src, 144);
			dst += 144;
			for (int j = 0; j < 112; ++j) {
				*dst++ = 144 + j;
			}
		}
	}
	memcpy(_shadowColorLut, src, 144);
	for (int i = 144; i < 256; ++i) {
		_shadowColorLut[i] = i;
	}
	if (0) {
		// lookup[a * 256 + b]
		//
		// if (a < 144) return b;
		// else if (b < 144) return src[b]
		// else return b;
		//
		// return (a >= 144 && b < 144) ? src[b] : b;
		for (int a = 0; a < 256; ++a) {
			for (int b = 0; b < 256; ++b) {
				const int res1 = (a >= 144 && b < 144) ? src[b] : b;
				const int res2 = dst[a * 256 + b - 65536];
				if (res1 != res2) {
					fprintf(stdout, "buildShadowColorLookupTable a %d b %d res1 %d res2 %d\n", a, b, res1, res2);
				}
				assert(res1 == res2);
			}
		}
	}
}

// returns the font index
uint8_t Video::findStringCharacterFontIndex(uint8_t chr) const {
	// bugfix: the original code seems to ignore the last 3 entries
	for (int i = 0; i < 39 * 2; i += 2) {
		if (_fontCharactersTable[i] == chr) {
			return _fontCharactersTable[i + 1];
		}
	}
	return 255;
}

void Video::drawStringCharacter(int x, int y, uint8_t chr, uint8_t color, uint8_t *dst) {
	const uint8_t *p = _font + ((chr & 15) + (chr >> 4) * 256) * 16;
	dst += y * W + x;
	for (int j = 0; j < 16; ++j) {
		for (int i = 0; i < 16; ++i) {
			if (p[i] != 0) {
				dst[i] = color;
			}
		}
		p += 16 * 16;
		dst += W;
	}
}

void Video::drawString(const char *s, int x, int y, uint8_t color, uint8_t *dst) {
	for (int i = 0; s[i]; ++i) {
		uint8_t chr = s[i];
		if (chr >= 'a' && chr <= 'z') {
			chr += 'A' - 'a';
		}
		if (chr != ' ') {
			chr = findStringCharacterFontIndex(chr);
			if (chr == 255) {
				continue;
			}
			drawStringCharacter(x, y, chr, color, dst);
		}
		x += 8;
	}
}

uint8_t Video::findWhiteColor() const {
	uint8_t color = 0;
	int whiteQuant = 0;
	for (int i = 0; i < 256; ++i) {
		const int q = colorBrightness(_palette[i * 3], _palette[i * 3 + 1], _palette[i * 3 + 2]);
		if (q > whiteQuant) {
			whiteQuant = q;
			color = i;
		}
	}
	return color;
}
