/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "random.h"

void Random::setSeed() {
	_rndSeed = 0x101A1010;
}

void Random::initMstTable() {
	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < 32; ++j) {
			_mstRandomTable[i][j] = j;
		}
		for (int j = 0; j < 64; ++j) {
			const int index1 = update() & 31;
			const int index2 = update() & 31;
			SWAP(_mstRandomTable[i][index1], _mstRandomTable[i][index2]);
		}
	}
}

void Random::initTable(int rounds) {
	for (int i = 0; i < 100; ++i) {
		_rndRandomTable[i] = i + 1;
	}
	for (int j = 0; j < rounds; ++j) {
		for (int i = 0; i < 256; ++i) {
			const int a = update() % 100;
			const int b = update() % 100;
			SWAP(_rndRandomTable[a], _rndRandomTable[b]);
		}
	}
	_rndRandomTableIndex = 0;
}

uint32_t Random::update() {
	const bool overflow = (_rndSeed & (1 << 31)) != 0;
	_rndSeed *= 2;
	if (!overflow) {
		_rndSeed ^= 0xDEADBEEF;
	}
	return _rndSeed;
}

uint8_t Random::getNextNumber() {
	const uint8_t num = _rndRandomTable[_rndRandomTableIndex];
	++_rndRandomTableIndex;
	if (_rndRandomTableIndex == 100) {
		_rndRandomTableIndex = 0;
	}
	return num;
}

void Random::resetMst(uint8_t *p) {
	p[0] = update() & 7; // row
	p[1] = update() & 31; // start
	p[2] = 32; // count
}

uint8_t Random::getMstNextNumber(uint8_t *p) {
	const uint8_t num = _mstRandomTable[p[0]][p[1]];
	++p[1];
	if (p[1] >= 32) {
		p[1] = 0;
	}
	--p[2];
	if (p[2] == 0) { // move to next row
		++p[0];
		if (p[0] >= 8) {
			p[0] = 0;
		}
		p[1] = update() & 31;
		p[2] = 32;
	}
	return num;
}
