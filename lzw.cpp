/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "intern.h"

enum {
	kCodeWidth = 9,
	kClearCode = 1 << (kCodeWidth - 1),
	kEndCode = kClearCode + 1,
	kNewCodes = kEndCode + 1,
	kMaxBits = 12
};

struct LzwDecoder {

	uint16_t _prefix[4096];
	uint8_t _stack[8192];
	const uint8_t *_buf;
	uint8_t _currentBits;
	uint8_t _codeSize;
	uint8_t _bitsLeft;

	uint32_t nextCode();
	int decode(uint8_t *dst);
};

static struct LzwDecoder _lzw;

uint32_t LzwDecoder::nextCode() {
	if (_bitsLeft == 0) {
		_currentBits = *_buf++;
		_bitsLeft = 8;
	}
	uint32_t code = _currentBits >> (8 - _bitsLeft);
	while (_bitsLeft < _codeSize) {
		_currentBits = *_buf++;
		code |= _currentBits << _bitsLeft;
		_bitsLeft += 8;
	}
	code &= (1 << _codeSize) - 1;
	_bitsLeft -= _codeSize;
	return code;
}

int LzwDecoder::decode(uint8_t *dst) {
	uint8_t *p = dst;
	uint32_t topSlot = 1 << kCodeWidth;
	uint8_t *stackPtr = &_stack[8191];
	uint32_t currentSlot = kNewCodes;
	uint32_t previousCode = 0;
	uint32_t lastCode = 0;
	uint32_t currentCode;
	_codeSize = kCodeWidth;
	while ((currentCode = nextCode()) != kEndCode) {
		if (currentCode == kClearCode) {
			currentSlot = kNewCodes;
			_codeSize = kCodeWidth;
			topSlot = 1 << kCodeWidth;
			while ((currentCode = nextCode()) == kClearCode) {
			}
			if (currentCode == kEndCode) {
				break;
			} else if (currentCode >= kNewCodes) {
				currentCode = 0;
			}
			previousCode = lastCode = currentCode;
			*p++ = (uint8_t)currentCode;
		} else {
			uint8_t *currentStackPtr = stackPtr;
			uint32_t slot = currentSlot;
			uint32_t code = currentCode;
			if (currentCode >= slot) {
				code = lastCode;
				currentStackPtr = &_stack[8190];
				_stack[8190] = (uint8_t)previousCode;
			}
			while (code >= kNewCodes) {
				--currentStackPtr;
				assert(currentStackPtr >= &_stack[0]);
				*currentStackPtr = _stack[code];
				code = _prefix[code];
			}
			--currentStackPtr;
			*currentStackPtr = (uint8_t)code;
			if (slot < topSlot) {
				_stack[slot] = (uint8_t)code;
				previousCode = code;
				_prefix[slot] = (uint16_t)lastCode;
				lastCode = currentCode;
				++slot;
				currentSlot = slot;
			}
			if (slot >= topSlot && _codeSize < kMaxBits) {
				topSlot <<= 1;
				++_codeSize;
			}
			while (currentStackPtr < stackPtr) {
				*p++ = *currentStackPtr++;
			}
			assert(currentStackPtr == stackPtr);
		}
	}
	return p - dst;
}

int decodeLZW(const uint8_t *src, uint8_t *dst) {
	memset(&_lzw, 0, sizeof(_lzw));
	_lzw._buf = src;
	return _lzw.decode(dst);
}
