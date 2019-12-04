/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "game.h"
#include "random.h"
#include "util.h"

// probably rename this to anim.cpp as this updates most LvlObject, not only Andy

static const uint8_t andyOp1LookupTable[] = {
	 0,  0,  0,  0,  1,  1,  2,  2,  3,  4,  3,  4,  2,  2,  1,  1,
	 4,  3,  4,  3,  5,  6,  7,  8,  7,  8,  5,  6,  8,  7,  6,  5,
	 6,  5,  8,  7,  9,  9,  9,  9, 10, 10, 11, 11, 12, 13, 12, 13,
	11, 11, 10, 10, 13, 12, 13, 12, 14, 15, 16, 17, 15, 14, 17, 16,
	16, 17, 14, 15, 17, 16, 15, 14, 18, 19, 18, 19, 19, 18, 19, 18,
	20, 20, 21, 21, 21, 21, 20, 20, 22, 23, 22, 23, 23, 22, 23, 22,
	24, 24, 25, 25, 25, 25, 24, 24, 26, 26, 26, 26, 27, 27, 27, 27
};

int Game::moveAndyObjectOp1(int op) {
	op = andyOp1LookupTable[op];
	switch (op) {
	case 0:
		return 1;
	case 1:
		return _directionKeyMask & 1;
	case 2:
		return (_directionKeyMask >> 2) & 1;
	case 3:
		return (_directionKeyMask >> 1) & 1;
	case 4:
		return (_directionKeyMask >> 3) & 1;
	case 5:
		return ((_directionKeyMask & 3) == 3) ? 1 : 0;
	case 6:
		return ((_directionKeyMask & 9) == 9) ? 1 : 0;
	case 7:
		return ((_directionKeyMask & 6) == 6) ? 1 : 0;
	case 8:
		return ((_directionKeyMask & 12) == 12) ? 1 : 0;
	case 9:
		return (_directionKeyMask == 0) ? 1 : 0;
	case 10:
		return (~_directionKeyMask) & 1;
	case 11:
		return ((~_directionKeyMask) >> 2) & 1;
	case 12:
		return ((~_directionKeyMask) >> 1) & 1;
	case 13:
		return ((~_directionKeyMask) >> 3) & 1;
	case 14:
		return _directionKeyMask & 3;
	case 15:
		return _directionKeyMask & 9;
	case 16:
		return _directionKeyMask & 6;
	case 17:
		return _directionKeyMask & 12;
	case 18:
		return (_directionKeyMask == 2) ? 1 : 0;
	case 19:
		return (_directionKeyMask == 8) ? 1 : 0;
	case 20:
		return (_directionKeyMask == 1) ? 1 : 0;
	case 21:
		return (_directionKeyMask == 4) ? 1 : 0;
	case 22:
		return ((_directionKeyMask & 13) != 0) ? 1 : 0;
	case 23:
		return ((_directionKeyMask & 7) != 0) ? 1 : 0;
	case 24:
		return ((_directionKeyMask & 14) != 0) ? 1 : 0;
	case 25:
		return ((_directionKeyMask & 11) != 0) ? 1 : 0;
	case 26:
		return _directionKeyMask & 5;
	case 27:
		return _directionKeyMask & 10;
	default:
		error("moveAndyObjectOp1 op %d", op);
		break;
	}
	return 0;
}

int Game::moveAndyObjectOp2(int op) {
	switch (op) {
	case 0:
		return 1;
	case 1:
		return _actionKeyMask & 1;
	case 2:
		return (_actionKeyMask >> 1) & 1;
	case 3:
		return (_actionKeyMask >> 2) & 1;
	case 4:
		return ((_actionKeyMask & 3) == 3) ? 1 : 0;
	case 5:
		return ((_actionKeyMask & 6) == 6) ? 1 : 0;
	case 6:
		return ((_actionKeyMask & 5) == 5) ? 1 : 0;
	case 7:
		return ((_actionKeyMask & 7) == 7) ? 1 : 0;
	case 8:
		return (_actionKeyMask >> 3) & 1;
	case 9:
		return (_actionKeyMask == 0) ? 1 : 0;
	case 10:
		return (~_actionKeyMask) & 1;
	case 11:
		return ((~_actionKeyMask) >> 1) & 1;
	case 12:
		return ((~_actionKeyMask) >> 2) & 1;
	case 13:
		return (_actionKeyMask >> 4) & 1;
	case 14:
		return (_actionKeyMask >> 5) & 1;
	case 15:
		return ((_actionKeyMask & 48) == 48) ? 1 : 0;
	case 16:
		return (_actionKeyMask >> 6) & 1;
	case 17:
		return (_actionKeyMask == 1) ? 1 : 0;
	case 18:
		return (_actionKeyMask == 2) ? 1 : 0;
	case 19:
		return (_actionKeyMask == 4) ? 1 : 0;
	case 20:
		return (_actionKeyMask >> 7) & 1;
	case 21:
		return (_actionKeyMask == 8) ? 1 : 0;
	case 22:
		return (_actionKeyMask == 16) ? 1 : 0;
	case 23:
		return (_actionKeyMask == 32) ? 1 : 0;
	case 24:
		return (_actionKeyMask == 64) ? 1 : 0;
	case 25:
		return (_actionKeyMask == 128) ? 1 : 0;
	case 26:
		return ((_actionKeyMask & 24) == 24) ? 1 : 0;
	case 27:
		return ((_actionKeyMask & 96) == 96) ? 1 : 0;
	case 28:
		return ((_actionKeyMask & 192) == 192) ? 1 : 0;
	case 29:
		return ((_actionKeyMask & 3) == 0) ? 1 : 0;
	case 30:
		return ((_actionKeyMask & 6) == 0) ? 1 : 0;
	case 31:
		return ((_actionKeyMask & 5) == 0) ? 1 : 0;
	case 32:
		return ((_actionKeyMask & 7) == 0) ? 1 : 0;
	case 33:
		return ((~_actionKeyMask) >> 3) & 1;
	case 34:
		return ((~_actionKeyMask) >> 4) & 1;
	case 35:
		return ((~_actionKeyMask) >> 5) & 1;
	case 36:
		return ((~_actionKeyMask) >> 6) & 1;
	case 37:
		return ((~_actionKeyMask) >> 7) & 1;
	case 38:
		return ((_actionKeyMask & 3) != 0) ? 1 : 0;
	case 39:
		return ((_actionKeyMask & 6) != 0) ? 1 : 0;
	case 40:
		return ((_actionKeyMask & 5) != 0) ? 1 : 0;
	case 41:
		return ((_actionKeyMask & 7) != 0) ? 1 : 0;
	case 42:
		return ((_actionKeyMask & 3) == 1) ? 1 : 0;
	case 43:
		return ((_actionKeyMask & 5) == 1) ? 1 : 0;
	case 44:
		return ((_actionKeyMask & 3) == 2) ? 1 : 0;
	case 45:
		return ((_actionKeyMask & 6) == 2) ? 1 : 0;
	case 46:
		return ((_actionKeyMask & 5) == 4) ? 1 : 0;
	case 47:
		return ((_actionKeyMask & 6) == 4) ? 1 : 0;
	default:
		error("moveAndyObjectOp2 op %d", op);
		break;
	}
	return 0;
}

static const int _andyMoveTable1[] = { 0, 1, -1, 513, 511, 512, -511, -513, -512 };

static const int _andyMoveTable2[] = { 0, 1, 512, 513, 1024, 1025, 1536, 1537 };

static const int _andyMoveTable3[] = { 0, -1, 512, 511, 1024, 1023, 1536, 1535 };

static const int _andyMoveTable4[] = { 0, 1, 512, 1, -511 };

static const int _andyMoveTable5[] = { 0, -1, 512, -1, -513 };

static const int _andyMoveTable6[] = { -1024, -1025, -1026, -512, -513, -514 };

static const int _andyMoveTable7[] = { -1024, -1023, -1022, -512, -511, -510 };

static const int _andyMoveTable8[] = { 0, 512, 1024, 1536 };

static const int _andyMoveTable9[] = { -1024, 1, -512, -1 };

static const int _andyMoveTable10[] = { -1024, -1, -512, 1 };

static const int _andyMoveTable11[] = { -509, -512 };

static const int _andyMoveTable12[] = { -515, -512 };

static const int _andyMoveTable13[] = { 1, 1, 1, 512, -1, -1 };

static const int _andyMoveTable14[] = { -1, -1, -1, 512, 1, 1 };

static const int _andyMoveTable15[] = { 1536, 512, 1, 512 };

static const int _andyMoveTable16[] = { 1536, 512, -1, 512 };

static const int _andyMoveTable17[] = { 1022, -1, 512, 1 };

static const int _andyMoveTable18[] = { 1026, 1, 512, -1 };

static const int _andyMoveTable19[] = { -2, -1, -1, 512, 1, 1 };

static const int _andyMoveTable20[] = { 2, 1, 1, 512, -1, -1 };

static const int _andyMoveTable21[] = { -1, -512, -1, -1, -511 };

static const int _andyMoveTable22[] = { 1, -512, 1, 1, -513 };

static const int _andyMoveTable23[] = { -1024, -512 };

static const int _andyMoveTable24[] = { -1024, -512 };

static const int _andyMoveTable25[] = { 1, -512, 1, 1, -511 };

static const int _andyMoveTable26[] = { -1, -512, -1, -1, -511 };

static const int _andyMoveTable27[] = { 1, 512, 512, 1, 1, 511, -1 };

static const int _andyMoveTable28[] = { -1, 512, 512, -1, -1, 513, 1 };

static const int _andyMoveTable29[] = { 1536, 512 };

static const int _andyMoveTable30[] = { 1536, 512 };

static const int _andyMoveTable31[] = { -1, 512, 512, -1, -1, 513, 1 };

static const int _andyMoveTable32[] = { 1, 512, 512, 1, 1, 511, -1 };

static const int _andyMoveTable33[] = { -1, -1, -1, 512, 1, 1 };

static const int _andyMoveTable34[] = { 1, 1, 1, 512, -1, -1 };

static const int _andyMoveTable35[] = {
	  0,   0,   8,   0,  -8,   0,   8,   8,  -8,   8,   0,   8,   8,  -8,  -8,  -8,
	  0,  -8,  16,   0, -16,   0,  16,  16, -16,  16,   0,  16,  16, -16, -16, -16,
	 24,   0, -24,   0,  24,  24, -24,  24,   0,  24,  24, -24, -24, -24,   0,   0,
};

static const int _andyMoveTable37[] = { -1021, 1027, 2048, -1027, 1021, 2048, -1021, -2048, -3069, -1027, -2048, -3075 };

static const int _andyMoveTable39[] = { 1027, 2048, 3075 };

static const int _andyMoveTable40[] = { 1021, 2048, 3069 };

static const int _andyMoveTable41[] = { -1025, 2, 1023, 2050 };

static const int _andyMoveTable42[] = { -1023, -2, 1025, 2046 };

static const int _andyMoveTable43[] = { -512, 1536, -515, -509, 1533, 1539 };

int Game::moveAndyObjectOp3(int op) {
	switch (op) {
	case 0:
		return 1;
	case 1: {
			int offset = (_andyMoveMask & 1) != 0 ? 3 : 0;
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[_andyMoveTable37[offset + i] + _andyMaskBufferPos1] & 8) == 0) {
					return 0;
				}
			}
		}
		return 1;
	case 2: {
			int i, offset = _andyMaskBufferPos1;
			for (i = 0; i < 9; ++i) {
				if ((_screenMaskBuffer[_andyMoveTable1[i] + offset] & 8) != 0) {
					offset += _andyMoveTable1[i];
					break;
				}
			}
			if ((_screenMaskBuffer[offset + 2048] & 8) != 0) {
				const int alignDy = (_andyLevelData0x288PosTablePtr[1].y + _andyMoveData.yPos) & 7;
				_andyMoveData.yPos += _andyMoveTable35[i * 2 + 1] - alignDy;
				const int alignDx = (_andyLevelData0x288PosTablePtr[1].x + _andyMoveData.xPos) & 7;
				_andyMoveData.xPos += _andyMoveTable35[i * 2] - alignDx + 4;
				if ((_andyMoveMask & 1) != 0) {
					--_andyMoveData.xPos;
				}
				return 1;
			}
		}
		return 0;
	case 3:
		for (int i = 0; i < 9; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable1[i] + _andyMaskBufferPos1] & 8) != 0) {
				const int alignDx = (_andyLevelData0x288PosTablePtr[1].x + _andyMoveData.xPos) & 7;
				_andyMoveData.xPos += _andyMoveTable35[i * 2] - alignDx + 4;
				break;
			}
		}
		return 1;
	case 4: {
			int offset = _andyMaskBufferPos1 - 512;
			int ret = 0;
			const int *p;
			if (_andyMoveMask & 1) {
				--offset;
				p = &_andyMoveTable37[3];
			} else {
				++offset;
				p = &_andyMoveTable37[0];
			}
			int _al = 0;
			for (int i = 0; i < 9; ++i) {
				if ((_screenMaskBuffer[_andyMoveTable1[i] + offset] & 8) == 0) {
					continue;
				}
				ret = 1;
				_al = i;
				offset += _andyMoveTable1[i];
				break;
			}
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[p[i] + offset] & 8) != 0) {
					continue;
				}
				return 0;
			}
			if (ret) {
				const int alignDy = (_andyLevelData0x288PosTablePtr[1].y + _andyMoveData.yPos) & 7;
				_andyMoveData.yPos += _andyMoveTable35[_al * 2 + 1] - alignDy - 8;
				const int alignDx = (_andyLevelData0x288PosTablePtr[1].x + _andyMoveData.xPos) & 7;
				_andyMoveData.xPos += _andyMoveTable35[_al * 2 + 0] - alignDx + 4;
				if (_andyMoveMask & 1) {
					_andyMoveData.xPos -= 9;
				} else {
					_andyMoveData.xPos += 8;
				}
				return ret;
			}
		}
		return 0;
	case 5:
		return (_andyMoveState[8] >> 3) & 1;
	case 6:
		return (_andyMoveState[20] >> 3) & 1;
	case 7:
		return (_andyMoveState[28] >> 3) & 1;
	case 8:
		return (_andyMoveState[0] >> 3) & 1;
	case 9: {
			int offset = (_andyMoveMask & 1) != 0 ? -1 : 1;
			int mask = _screenMaskBuffer[_andyMaskBufferPos1];
			if ((mask & 1) != 0 && (_screenMaskBuffer[_andyMaskBufferPos1 + offset] & 1) != 0) {
				return (mask & 4) != 0 ? 0 : 1;
			}
			mask = _screenMaskBuffer[_andyMaskBufferPos1 - 512];
			if ((mask & 1) != 0 && (_screenMaskBuffer[_andyMaskBufferPos1 - 512 + offset] & 1) != 0) {
				return (mask & 4) != 0 ? 0 : 1;
			}
		}
		return 0;
	case 10: {
			const int offset = (_andyMoveMask & 1) != 0 ? 1 : -1;
			if (_screenMaskBuffer[_andyMaskBufferPos5 + 512] & 1) {
				if (_screenMaskBuffer[_andyMaskBufferPos5 + 512 + offset] & 1) {
					return 1;
				}
			}
		}
		return 0;
	case 11: {
			const int index = (_andyMoveMask & 1) == 0 ? 5 : 3;
			if ((_andyMoveState[index] & 8) != 0 && (_andyMoveState[16 + index] & 8) != 0) {
				return 1;
			}
		}
		return 0;
	case 12: {
			const int index = (_andyMoveMask & 1) == 0 ? 1 : 7;
			if ((_andyMoveState[8 + index] & 8) != 0 && (_andyMoveState[24 + index] & 8) != 0) {
				return 1;
			}
		}
		return 0;
	case 13:
		return (_andyMoveState[8] & 9) == 0 ? 1 : 0;
	case 14: {
			const int index = (_andyMoveMask & 1) | 2;
			return (_andyMoveState[4 + 8 * index] & 9) == 0 ? 1 : 0;
		}
	case 15: {
			const int index = (_andyMoveMask & 1) == 0 ? 7 : -1;
			return ((_andyMoveState[24 + index] >> 3) & (_andyMoveState[8 + index] >> 3)) & 1;
		}
	case 16: {
			const int index = (_andyMoveMask & 1) == 0 ? 3 : 5;
			return ((_andyMoveState[16 + index] >> 3) & (_andyMoveState[index] >> 3)) & 1;
		}
	case 17: {
			const int index = (_andyMoveMask & 1) == 0 ? 7 : 1;
			return ((_andyMoveState[16 + index] >> 3) & (_andyMoveState[index] >> 3)) & 1;
		}
	case 18: {
			const int index = (_andyMoveMask & 1) == 0 ? 1 : 7;
			return ((_andyMoveState[24 + index] >> 3) & (_andyMoveState[8 + index] >> 3)) & 1;
		}
	case 19: {
			const int index = (_andyMoveMask & 1) == 0 ? 3 : 5;
			return ((_andyMoveState[24 + index] >> 3) & (_andyMoveState[8 + index] >> 3)) & 1;
		}
	case 20: {
			const int index = (_andyMoveMask & 1) == 0 ? 5 : 3;
			return ((_andyMoveState[16 + index] >> 3) & (_andyMoveState[index] >> 3)) & 1;
		}
	case 21: {
			const int index = (_andyMoveMask & 1) == 0 ? 5 : 3;
			return ((_andyMoveState[24 + index] >> 3) & (_andyMoveState[8 + index] >> 3)) & 1;
		}
	case 22:
		return (_andyMoveState[0] & 9) == 0 ? 1 : 0;
	case 23: {
			const int index = (~_andyMoveMask & 1) | 2;
			return (_andyMoveState[4 + 8 * index] & 9) == 0 ? 1 : 0;
		}
	case 24: {
			const int index = (_andyMoveMask & 1) == 0 ? 5 : 2;
			return ((~(_andyMoveState[16 + index] & _andyMoveState[index])) >> 3) & 1;
		}
	case 25: {
			const int index = (_andyMoveMask & 1) == 0 ? 1 : 6;
			return ((~(_andyMoveState[24 + index] & _andyMoveState[8 + index])) >> 3) & 1;
		}
	case 26: {
			const int index = (_andyMoveMask & 1) == 0 ? 3 : 5;
			return ((~(_andyMoveState[16 + index] & _andyMoveState[index])) >> 3) & 1;
		}
	case 27: {
			const int index = (_andyMoveMask & 1) == 0 ? 5 : 3;
			return ((~(_andyMoveState[24 + index] & _andyMoveState[8 + index])) >> 3) & 1;
		}
	case 28: {
			const int index = (_andyMoveMask & 1) == 0 ? 1 : 7;
			return ((~_andyMoveState[index]) >> 3) & 1;
		}
	case 29: {
			const int index = (_andyMoveMask & 1) == 0 ? 7 : 1;
			return ((~_andyMoveState[8 + index]) >> 3) & 1;
		}
	case 30: {
			int index1, index2;
			if (_andyMoveMask & 1) {
				index1 = 2;
				index2 = 3;
			} else {
				index1 = 6;
				index2 = 5;
			}
			if (((_andyMoveState[16 + index1] & _andyMoveState[index1]) & 8) == 0 || (_andyMoveState[index2] & 1) != 0) {
				return 0;
			}
		}
		return 1;
	case 31: {
			int index1, index2;
			if (_andyMoveMask & 1) {
				index1 = 6;
				index2 = 5;
			} else {
				index1 = 2;
				index2 = 3;
			}
			if (((_andyMoveState[24 + index1] & _andyMoveState[8 + index1]) & 8) == 0 || (_andyMoveState[8 + index2] & 1) != 0) {
				return 0;
			}
		}
		return 1;
	case 32: {
			int index1, index2;
			if (_andyMoveMask & 1) {
				index1 = 2;
				index2 = 3;
			} else {
				index1 = 6;
				index2 = 5;
			}
			if (((_andyMoveState[16 + index1] & _andyMoveState[index1]) & 8) != 0 && (_andyMoveState[index2] & 1) == 0) {
				return 0;
			}
		}
		return 1;
	case 33: {
			int index1, index2;
			if (_andyMoveMask & 1) {
				index1 = 6;
				index2 = 5;
			} else {
				index1 = 2;
				index2 = 3;
			}
			if (((_andyMoveState[24 + index1] & _andyMoveState[8 + index1]) & 8) != 0 && (_andyMoveState[index2] & 1) == 0) {
				return 0;
			}
		}
		return 1;
	case 34: {
			const int index = (_andyMoveMask & 1) == 0 ? 1 : 7;
			return ((_andyMoveState[16 + index] >> 3) & (_andyMoveState[index] >> 3)) & 1;
		}
	case 35:
		if ((_screenMaskBuffer[_andyMaskBufferPos3 - 514] | _screenMaskBuffer[_andyMaskBufferPos3 - 513]) & 8) {
			if ((_screenMaskBuffer[_andyMaskBufferPos3 + 514] | _screenMaskBuffer[_andyMaskBufferPos3 + 513]) & 8) {
				return 1;
			} else {
				return 0;
			}
		}
		if ((_screenMaskBuffer[_andyMaskBufferPos3 - 511] | _screenMaskBuffer[_andyMaskBufferPos3 - 510]) & 8) {
			if ((_screenMaskBuffer[_andyMaskBufferPos3 + 511] | _screenMaskBuffer[_andyMaskBufferPos3 + 510]) & 8) {
				return 1;
			} else {
				return 0;
			}
		}
		return 0;
	case 36:
		if (_andyMoveMask & 1) {
			int _al = 0;
			int ret = 1;
			int offset = _andyMaskBufferPos2;
			if (_screenMaskBuffer[_andyMaskBufferPos2 + 1] & 8) {
				++offset;
				_al = 2;
			} else if (_screenMaskBuffer[_andyMaskBufferPos2 + 2] & 8) {
				offset += 2;
				_al = 0;
			} else if (_screenMaskBuffer[_andyMaskBufferPos2 + 3] & 8) {
				offset += 3;
				_al = 1;
			} else if (_screenMaskBuffer[_andyMaskBufferPos2 + 4] & 8) {
				offset += 4;
				_al = 9;
			} else {
				return 0;
			}
			int xPos = _andyMoveTable37[3];
			int i = 0;
			while (_screenMaskBuffer[xPos + offset] & 8) {
				++i;
				if (i >= 2) {
					const int alignDx = (_andyLevelData0x288PosTablePtr[2].x + _andyMoveData.xPos) & 7;
					_andyMoveData.xPos += _andyMoveTable35[_al * 2] - alignDx + 7;
					return ret;
				}
				xPos = _andyMoveTable37[3 + i];
			}
		} else {
			int ret = 1;
			int offset = _andyMaskBufferPos1;
			int _al = 0;
			if (_screenMaskBuffer[_andyMaskBufferPos1 - 1] & 8) {
				--offset;
				_al = 2;
			} else if (_screenMaskBuffer[_andyMaskBufferPos1 - 2] & 8) {
				offset -= 2;
				_al = 10;
			} else if (_screenMaskBuffer[_andyMaskBufferPos1 - 3] & 8) {
				offset -= 3;
				_al = 17;
			}
			int xPos = _andyMoveTable37[0];
			int i = 0;
			while (_screenMaskBuffer[xPos + offset] & 8) {
				++i;
				if (i >= 2) {
					const int alignDx = (_andyLevelData0x288PosTablePtr[1].x - _andyMoveData.xPos) & 7;
					_andyMoveData.xPos += _andyMoveTable35[_al * 2] - alignDx + 4;
					return ret;
				}
				xPos = _andyMoveTable37[i];
			}
		}
		return 0;
	case 37:
		if ((_andyMoveMask & 1) == 0) {
			int ret = 1;
			int _al = 0;
			int offset = _andyMaskBufferPos1;
			if (_screenMaskBuffer[_andyMaskBufferPos1 + 1] & 8) {
				++offset;
				_al = 2;
			} else if (_screenMaskBuffer[_andyMaskBufferPos1 + 2] & 8) {
				offset += 2;
				_al = 0;
			} else if (_screenMaskBuffer[_andyMaskBufferPos1 + 3] & 8) {
				offset += 3;
				_al = 1;
			} else if (_screenMaskBuffer[_andyMaskBufferPos1 + 4] & 8) {
				offset += 4;
				_al = 9;
			} else {
				ret = 0;
			}
			for (int i = 0; i < 2; ++i) {
				if ((_screenMaskBuffer[offset + _andyMoveTable37[3 + i]] & 8) == 0) {
					return 0;
				}
			}
			if (ret) {
				const int alignDx = (_andyLevelData0x288PosTablePtr[1].x + _andyMoveData.xPos) & 7;
				_andyMoveData.xPos += _andyMoveTable35[_al * 2] - alignDx + 7;
			}
			return ret;
		} else {
			int ret = 1;
			int _al = 0;
			int offset = _andyMaskBufferPos2;
			if (_screenMaskBuffer[_andyMaskBufferPos2 - 1] & 8) {
				--offset;
				_al = 2;
			} else if (_screenMaskBuffer[_andyMaskBufferPos2 - 2] & 8) {
				offset -= 2;
				_al = 10;
			} else if (_screenMaskBuffer[_andyMaskBufferPos2 - 3] & 8) {
				offset -= 3;
				_al = 17;
			}
			for (int i = 0; i < 2; ++i) {
				if ((_screenMaskBuffer[offset + _andyMoveTable37[i]] & 8) == 0) {
					return 0;
				}
			}
			const int alignDx = (_andyLevelData0x288PosTablePtr[2].x + _andyMoveData.xPos) & 7;
			_andyMoveData.xPos += _andyMoveTable35[_al * 2] - alignDx + 4;
			return ret;
		}
		return 0;
	case 38:
		if (_screenMaskBuffer[_andyMaskBufferPos3 + 1536] & 8) {
			if ((_screenMaskBuffer[_andyMaskBufferPos3 + 2557] | _screenMaskBuffer[_andyMaskBufferPos3 + 2563]) & 8) {
				return 1;
			} else {
				return 0;
			}
		}
		if ((_screenMaskBuffer[_andyMaskBufferPos3 + 1533] | _screenMaskBuffer[_andyMaskBufferPos3 + 1534] | _screenMaskBuffer[_andyMaskBufferPos3 + 1535]) & 8) {
			if ((_screenMaskBuffer[_andyMaskBufferPos3 + 2562] | _screenMaskBuffer[_andyMaskBufferPos3 + 2561] | _screenMaskBuffer[_andyMaskBufferPos3 + 2560]) & 8) {
				return 1;
			} else {
				return 0;
			}
		}
		if ((_screenMaskBuffer[_andyMaskBufferPos3 + 1539] | _screenMaskBuffer[_andyMaskBufferPos3 + 1538] | _screenMaskBuffer[_andyMaskBufferPos3 + 1537]) & 8) {
			if ((_screenMaskBuffer[_andyMaskBufferPos3 + 2558] | _screenMaskBuffer[_andyMaskBufferPos3 + 2559] | _screenMaskBuffer[_andyMaskBufferPos3 + 2560]) & 8) {
				return 1;
			} else {
				return 0;
			}
		}
		return 0;
	case 39:
		for (int i = 0; i < 3; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos5] & 1) == 0) {
				continue;
			}
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos4] & 1) == 0) {
				continue;
			}
			return 1;
		}
		return 0;
	case 40:
		for (int i = 0; i < 3; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos5] & 1) == 0) {
				continue;
			}
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos4] & 1) == 0) {
				continue;
			}
			return 0;
		}
		return 1;
	case 41: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable2 : _andyMoveTable3;
			for (int i = 0; i < 2; ++i) {
				int mask = _screenMaskBuffer[_andyMaskBufferPos1 + p[i]];
				mask |= _screenMaskBuffer[_andyMaskBufferPos2 + p[i]];
				mask |= _screenMaskBuffer[_andyMaskBufferPos4 + p[i]];
				mask |= _screenMaskBuffer[_andyMaskBufferPos5 + p[i]];
				mask |= _screenMaskBuffer[_andyMaskBufferPos3 + p[i]];
				mask |= _screenMaskBuffer[_andyMaskBufferPos0 + p[i]];
				if ((mask & 6) != 0) {
					return 1;
				}
			}
		}
		return 0;
	case 42: {
			int ret = 1;
			int _al = 0;
			int offset = _andyMaskBufferPos4;
			if ((_andyMoveData.flags1 & 0x10) != 0) {
				if (_screenMaskBuffer[offset + 512] & 8) {
					offset += 512;
					_al = 9;
				} else if (_screenMaskBuffer[offset + 511] & 8) {
					offset += 511;
					_al = 1;
				} else if (_screenMaskBuffer[offset + 510] & 8) {
					offset += 510;
					_al = 0;
				} else if (_screenMaskBuffer[offset + 509] & 8) {
					offset += 509;
					_al = 2;
				} else if (_screenMaskBuffer[offset + 508] & 8) {
					offset += 508;
					_al = 10;
				} else {
					ret = 0;
				}
				for (int i = 0; i < 2; ++i) {
					if ((_screenMaskBuffer[_andyMoveTable39[i] + offset] & 8) == 0) {
						return 0;
					}
				}
				if (ret) {
					const int alignDx = (_andyLevelData0x288PosTablePtr[5].x - _andyMoveData.xPos) & 7;
					_andyMoveData.xPos += _andyMoveTable35[_al * 2] - alignDx + 7;
					return ret;
				}
			} else {
				if (_screenMaskBuffer[offset + 512] & 8) {
					offset += 512;
					_al = 10;
				} else if (_screenMaskBuffer[offset + 513] & 8) {
					offset += 513;
					_al = 2;
				} else if (_screenMaskBuffer[offset + 514] & 8) {
					offset += 514;
					_al = 0;
				} else if (_screenMaskBuffer[offset + 515] & 8) {
					offset += 515;
					_al = 1;
				} else if (_screenMaskBuffer[offset + 516] & 8) {
					offset += 516;
					_al = 9;
				} else {
					ret = 0;
				}
				for (int i = 0; i < 2; ++i) {
					if ((_screenMaskBuffer[_andyMoveTable40[i] + offset] & 8) == 0) {
						return 0;
					}
				}
				if (ret) {
					const int alignDx = (_andyLevelData0x288PosTablePtr[5].x - _andyMoveData.xPos) & 7;
					_andyMoveData.xPos += _andyMoveTable35[_al * 2] - alignDx;
					return ret;
				}
			}
		}
		return 0;
	case 43: {
			int ret = 1;
			int _al = 0;
			int offset = _andyMaskBufferPos5;
			if ((_andyMoveData.flags1 & 0x10) == 0) {
				if (_screenMaskBuffer[offset + 512] & 8) {
					offset += 512;
					_al = 9;
				} else if (_screenMaskBuffer[offset + 511] & 8) {
					offset += 511;
					_al = 1;
				} else if (_screenMaskBuffer[offset + 510] & 8) {
					offset += 510;
					_al = 0;
				} else if (_screenMaskBuffer[offset + 509] & 8) {
					offset += 509;
					_al = 2;
				} else if (_screenMaskBuffer[offset + 508] & 8) {
					offset += 508;
					_al = 10;
				} else {
					ret = 0;
				}
				for (int i = 0; i < 2; ++i) {
					if ((_screenMaskBuffer[_andyMoveTable39[i] + offset] & 8) == 0) {
						return 0;
					}
				}
				if (ret) {
					const int alignDx = (_andyLevelData0x288PosTablePtr[4].x + _andyMoveData.xPos) & 7;
					_andyMoveData.xPos += _andyMoveTable35[_al * 2] - alignDx + 6;
					return ret;
				}
			} else {
				if (_screenMaskBuffer[offset + 512] & 8) {
					offset += 512;
					_al = 10;
				} else if (_screenMaskBuffer[offset + 513] & 8) {
					offset += 513;
					_al = 2;
				} else if (_screenMaskBuffer[offset + 514] & 8) {
					offset += 514;
					_al = 0;
				} else if (_screenMaskBuffer[offset + 515] & 8) {
					offset += 515;
					_al = 1;
				} else if (_screenMaskBuffer[offset + 516] & 8) {
					offset += 516;
					_al = 9;
				} else {
					ret = 0;
				}
				for (int i = 0; i < 2; ++i) {
					if ((_screenMaskBuffer[_andyMoveTable40[i] + offset] & 8) == 0) {
						return 0;
					}
				}
				if (ret) {
					const int alignDx = (_andyLevelData0x288PosTablePtr[4].x + _andyMoveData.xPos) & 7;
					_andyMoveData.xPos += _andyMoveTable35[_al * 2] - alignDx + 1;
					return ret;
				}
			}
		}
		return 0;
	case 44: {
			const int offset = (_andyMoveMask & 1) == 0 ? _andyMaskBufferPos5 - 1 : _andyMaskBufferPos5 + 1;
			for (int i = 0; i < 2; ++i) {
				if ((_screenMaskBuffer[_andyMoveTable8[i] + offset] & 1) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 45: {
			const int *p = (_andyMoveMask & 1) != 0 ? _andyMoveTable4 : _andyMoveTable5;
			for (int i = 0; i < 5; ++i) {
				if ((_screenMaskBuffer[_andyMaskBufferPos1 + p[i]] & 6) != 0) {
					return 1;
				}
			}
		}
		return 0;
	case 46: {
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos3] & 1) != 0) {
					return 1;
				}
			}
		}
		return 0;
	case 47: {
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos3] & 1) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 48: {
			const int *p = (_andyMoveMask & 1) != 0 ? _andyMoveTable4 : _andyMoveTable5;
			for (int i = 0; i < 5; ++i) {
				if ((_screenMaskBuffer[_andyMaskBufferPos2 + p[i]] & 6) != 0) {
					return 1;
				}
			}
		}
		return 0;
	case 49: {
			const int d = (_andyMoveMask & 1) != 0 ? _andyMaskBufferPos4 - 1 : _andyMaskBufferPos4 + 1;
			for (int i = 0; i < 2; ++i) {
				if ((_screenMaskBuffer[_andyMoveTable8[i] + d] & 1) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 50:
		for (int i = 0; i < 3; ++i) {
			if (((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos4] | _screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos5]) & 1) != 0) {
				return 1;
			}
		}
		return 0;
	case 51:
		for (int i = 0; i < 3; ++i) {
			if (((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos5] | _screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos4]) & 1) != 0) {
				return 0;
			}
		}
		return 1;
	case 52:
		for (int i = 0; i < 3; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos5] & 1) != 0) {
				return 1;
			}
		}
		return 0;
	case 53:
		for (int i = 0; i < 3; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos5] & 1) != 0) {
				return 0;
			}
		}
		return 1;
	case 54:
		for (int i = 0; i < 3; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos4] & 1) != 0) {
				return 1;
			}
		}
		return 0;
	case 55:
		for (int i = 0; i < 3; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos4] & 1) != 0) {
				return 0;
			}
		}
		return 1;
	case 56:
		for (int i = 0; i < 2; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos1] & 6) != 0) {
				return 1;
			}
		}
		return 0;
	case 57:
		for (int i = 0; i < 2; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos4] & 2) != 0) {
				return 1;
			}
		}
		return 0;
	case 58:
	case 60:
		for (int i = 0; i < 2; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos2] & 6) != 0) {
				return 1;
			}
		}
		return 0;
	case 59:
	case 61:
		for (int i = 0; i < 2; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable8[i] + _andyMaskBufferPos5] & 2) != 0) {
				return 1;
			}
		}
		return 0;
	case 62: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable4 : _andyMoveTable5;
			for (int i = 0; i < 5; ++i) {
				if ((_screenMaskBuffer[_andyMaskBufferPos1 + p[i]] & 6) != 0) {
					return 1;
				}
			}
		}
		return 0;
	case 63: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable4 : _andyMoveTable5;
			for (int i = 0; i < 5; ++i) {
				if ((_screenMaskBuffer[_andyMaskBufferPos2 + p[i]] & 6) != 0) {
					return 1;
				}
			}
		}
		return 0;
	case 64: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable39 : _andyMoveTable40;
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[_andyMaskBufferPos1 + p[i]] & 8) == 0) {
					return 0;
				}
			}
		}
		return 1;
	case 65: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable40 : _andyMoveTable39;
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[_andyMaskBufferPos1 + p[i]] & 8) == 0) {
					return 0;
				}
			}
		}
		return 1;
	case 66: {
			const int *p = (_andyMoveMask & 1) == 0 ? &_andyMoveTable37[3] : &_andyMoveTable37[0];
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[_andyMaskBufferPos1 + p[i]] & 8) == 0) {
					return 0;
				}
			}
		}
		return 1;
	case 67: {
			const int offset = (_andyMoveMask & 2) == 0 ? -512 : 512;
			int mask = _screenMaskBuffer[_andyMaskBufferPos0] | _screenMaskBuffer[_andyMaskBufferPos1] | _screenMaskBuffer[_andyMaskBufferPos2];
			if (mask & 6) {
				return 1;
			}
			mask = _screenMaskBuffer[_andyMaskBufferPos0 + offset] | _screenMaskBuffer[_andyMaskBufferPos1 + offset] | _screenMaskBuffer[_andyMaskBufferPos2 + offset];
			if (mask & 6) {
				return 1;
			}
		}
		return 0;
	case 68: {
			const int offset = (_andyMoveMask & 1) == 0 ? 1 : -1;
			int mask = _screenMaskBuffer[_andyMaskBufferPos0] | _screenMaskBuffer[_andyMaskBufferPos1] | _screenMaskBuffer[_andyMaskBufferPos2];
			if (mask & 6) {
				return 1;
			}
			mask = _screenMaskBuffer[_andyMaskBufferPos0 + offset] | _screenMaskBuffer[_andyMaskBufferPos1 + offset] | _screenMaskBuffer[_andyMaskBufferPos2 + offset];
			if (mask & 6) {
				return 1;
			}
		}
		return 0;
	case 69: {
			const int offset = (_andyMoveMask & 2) == 0 ? 512 : -512;
			int mask = _screenMaskBuffer[_andyMaskBufferPos0] | _screenMaskBuffer[_andyMaskBufferPos1] | _screenMaskBuffer[_andyMaskBufferPos2];
			if (mask & 6) {
				return 1;
			}
			mask = _screenMaskBuffer[_andyMaskBufferPos0 + offset] | _screenMaskBuffer[_andyMaskBufferPos1 + offset] | _screenMaskBuffer[_andyMaskBufferPos2 + offset];
			if (mask & 6) {
				return 1;
			}
		}
		return 0;
	case 70: {
			const int offset = (_andyMoveMask & 1) == 0 ? -1 : 1;
			int mask = _screenMaskBuffer[_andyMaskBufferPos0] | _screenMaskBuffer[_andyMaskBufferPos1] | _screenMaskBuffer[_andyMaskBufferPos2];
			if (mask & 6) {
				return 1;
			}
			mask = _screenMaskBuffer[_andyMaskBufferPos0 + offset] | _screenMaskBuffer[_andyMaskBufferPos1 + offset] | _screenMaskBuffer[_andyMaskBufferPos2 + offset];
			if (mask & 6) {
				return 1;
			}
		}
		return 0;
	case 71: {
			const int offset = (_andyMoveMask & 2) == 0 ? -512 : 512;
			if (((_screenMaskBuffer[_andyMaskBufferPos6] | _screenMaskBuffer[_andyMaskBufferPos6 + offset]) & 6) != 0) {
				return 1;
			}
		}
		return 0;
	case 72: {
			int offset = (_andyMoveMask & 1) != 0 ? -1 : 1;
			if (((_screenMaskBuffer[_andyMaskBufferPos6] | _screenMaskBuffer[_andyMaskBufferPos6 + offset]) & 6) != 0) {
				return 1;
			}
		}
		return 0;
	case 73: {
			int offset = (_andyMoveMask & 2) != 0 ? -512 : 512;
			int mask = _screenMaskBuffer[_andyMaskBufferPos6] | _screenMaskBuffer[_andyMaskBufferPos6 + offset];
			if ((mask & 6) != 0 || (mask & 1) != 0) {
				return 1;
			}
		}
		return 0;
	case 74: {
			const int offset = (_andyMoveMask & 1) != 0 ? 1 : -1;
			if (((_screenMaskBuffer[_andyMaskBufferPos6] | _screenMaskBuffer[_andyMaskBufferPos6 + offset]) & 6) != 0) {
				return 1;
			}
		}
		return 0;
	case 75: {
			const int offset = (_andyMoveMask & 2) != 0 ? -512 : 512;
			if ((_screenMaskBuffer[_andyMaskBufferPos7 + offset] & 15) != 0) {
				return 1;
			}
		}
		return 0;
	case 76: {
			const int *p = (_andyMoveMask & 1) != 0 ? _andyMoveTable10 : _andyMoveTable9;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 4; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 77: {
			const int *p = (_andyMoveMask & 1) != 0 ? _andyMoveTable12 : _andyMoveTable11;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 2; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 78: {
			const int *p = (_andyMoveMask & 1) != 0 ? _andyMoveTable14 : _andyMoveTable13;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 6; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 79: {
			const int *p = (_andyMoveMask & 1) != 0 ? _andyMoveTable16 : _andyMoveTable15;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 4; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 80: {
			const int *p = (_andyMoveMask & 1) != 0 ? _andyMoveTable18 : _andyMoveTable17;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 4; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 3) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 81: {
			const int *p = (_andyMoveMask & 1) != 0 ? _andyMoveTable20 : _andyMoveTable19;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 6; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 82: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable21 : _andyMoveTable22;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 5; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 83: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable23 : _andyMoveTable24;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 2; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 84: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable25 : _andyMoveTable26;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 5; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 85: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable27 : _andyMoveTable28;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 7; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 86: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable29 : _andyMoveTable30;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 2; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 87: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable31 : _andyMoveTable32;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 7; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 88: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable33 : _andyMoveTable34;
			int offset = _andyMaskBufferPos0;
			for (int i = 0; i < 6; ++i) {
				offset += p[i];
				if ((_screenMaskBuffer[offset] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 89: {
			const int offset = (_andyMoveMask & 1) == 0 ? 7 : 1;
			return ((~(_andyMoveState[16 + offset] & _andyMoveState[offset])) >> 3) & 1;
		}
	case 90: {
			const int offset = (_andyMoveMask & 1) == 0 ? 1 : 7;
			return ((~(_andyMoveState[24 + offset] & _andyMoveState[8 + offset])) >> 3) & 1;
		}
	case 91: {
			const int offset = (_andyMoveMask & 1) != 0 ? 5 : 3;
			return ((~(_andyMoveState[24 + offset] & _andyMoveState[8 + offset])) >> 3) & 1;
		}
	case 92: {
			const int offset = (_andyMoveMask & 1) != 0 ? 3 : 5;
			return ((~(_andyMoveState[16 + offset] & _andyMoveState[offset])) >> 3) & 1;
		}
	case 93: {
			const int *p = (_andyMoveMask & 1) != 0 ? _andyMoveTable5 : _andyMoveTable4;
			for (int i = 0; i < 5; ++i) {
				if (_screenMaskBuffer[p[i] + _andyMaskBufferPos0] & 6) {
					return 1;
				}
			}
		}
		return 0;
	case 94: {
			const int *p = (_andyMoveMask & 1) != 0 ? _andyMoveTable4 : _andyMoveTable5;
			for (int i = 0; i < 5; ++i) {
				if (_screenMaskBuffer[p[i] + _andyMaskBufferPos0] & 6) {
					return 1;
				}
			}
		}
		return 0;
	case 95: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable41 : _andyMoveTable42;
			for (int i = 0; i < 4; ++i) {
				if ((_screenMaskBuffer[p[i] + _andyMaskBufferPos0] & 9) == 0) {
					return 1;
				}
			}
		}
		return 0;
	case 96:
		for (int i = 0; i < 2; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable43[i] + _andyMaskBufferPos0] & 9) == 0) {
				return 1;
			}
		}
		return 0;
	case 97:
		for (int i = 0; i < 4; ++i) {
			if ((_screenMaskBuffer[_andyMoveTable43[2 + i] + _andyMaskBufferPos0] & 9) == 0) {
				return 1;
			}
		}
		return 0;
	case 98:
		return _screenMaskBuffer[_andyMaskBufferPos5 + 512] & 1;
	case 99: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable6 : _andyMoveTable7;
			for (int i = 0; i < 6; ++i) {
				if ((_screenMaskBuffer[p[i] + _andyMaskBufferPos0] & 6) != 0) {
					return 0;
				}
			}
		}
		return 1;
	case 100: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable4 : _andyMoveTable5;
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[p[i] + _andyMaskBufferPos2] & 6) != 0) {
					return 1;
				}
			}
		}
		return 0;
	case 101: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable5 : _andyMoveTable4;
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[p[i] + _andyMaskBufferPos2] & 6) != 0) {
					return 1;
				}
			}
		}
		return 0;
	case 102: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable4 : _andyMoveTable5;
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[p[i] + _andyMaskBufferPos1] & 6) != 0) {
					return 1;
				}
			}
		}
		return 0;
	case 103: {
			const int *p = (_andyMoveMask & 1) == 0 ? _andyMoveTable5 : _andyMoveTable4;
			for (int i = 0; i < 3; ++i) {
				if ((_screenMaskBuffer[p[i] + _andyMaskBufferPos1] & 6) != 0) {
					return 1;
				}
			}
		}
		return 0;
	case 104: {
			int offset = (_andyMoveMask & 2) != 0 ? 512 : -512;
			if ((_screenMaskBuffer[_andyMaskBufferPos4] | _screenMaskBuffer[_andyMaskBufferPos5]) & 6) {
				return 1;
			}
			if ((_screenMaskBuffer[_andyMaskBufferPos4 + offset] | _screenMaskBuffer[_andyMaskBufferPos5 + offset]) & 6) {
				return 1;
			}
		}
		return 0;
	case 105: {
			int offset = (_andyMoveMask & 1) != 0 ? -1 : 1;
			if ((_screenMaskBuffer[_andyMaskBufferPos4] | _screenMaskBuffer[_andyMaskBufferPos5]) & 6) {
				return 1;
			}
			if ((_screenMaskBuffer[_andyMaskBufferPos4 + offset] | _screenMaskBuffer[_andyMaskBufferPos5 + offset]) & 6) {
				return 1;
			}
		}
		return 0;
	case 106: {
			int offset = (_andyMoveMask & 2) != 0 ? -512 : 512;
			if ((_screenMaskBuffer[_andyMaskBufferPos5] | _screenMaskBuffer[_andyMaskBufferPos4]) & 6) {
				return 1;
			}
			if ((_screenMaskBuffer[_andyMaskBufferPos5 + offset] | _screenMaskBuffer[_andyMaskBufferPos4 + offset]) & 6) {
				return 1;
			}
		}
		return 0;
	case 107: {
			int offset = (_andyMoveMask & 2) != 0 ? 1 : -1;
			if ((_screenMaskBuffer[_andyMaskBufferPos5] | _screenMaskBuffer[_andyMaskBufferPos4]) & 6) {
				return 1;
			}
			if ((_screenMaskBuffer[_andyMaskBufferPos5 + offset] | _screenMaskBuffer[_andyMaskBufferPos4 + offset]) & 6) {
				return 1;
			}
		}
		return 0;
	case 108: {
			const int offset = (_andyMoveMask & 2) != 0 ? 512 : -512;
			if ((_screenMaskBuffer[_andyMaskBufferPos0 + offset] & 6) != 0) {
				const int offset2 = (_andyMoveMask & 1) != 0 ? -1 : 1;
				if ((_screenMaskBuffer[_andyMaskBufferPos0 + offset2] & 6) != 0) {
					return 1;
				}
			}
			if ((_screenMaskBuffer[_andyMaskBufferPos0] & 6) != 0) {
				return 1;
			}
		}
		return 0;
	case 109: {
			const int offset = (_andyMoveMask & 2) != 0 ? -512 : 512;
			if ((_screenMaskBuffer[_andyMaskBufferPos0 + offset] & 6) != 0) {
				const int offset2 = (_andyMoveMask & 1) != 0 ? -1 : 1;
				if ((_screenMaskBuffer[_andyMaskBufferPos0 + offset2] & 6) != 0) {
					return 1;
				}
			}
			if ((_screenMaskBuffer[_andyMaskBufferPos0] & 6) != 0) {
				return 1;
			}
		}
		return 0;
	default:
		error("moveAndyObjectOp3 op %d", op);
		break;
	}
	return 0;
}

int Game::moveAndyObjectOp4(int op) {
	switch (op) {
	case 0:
		return 1;
	case 1:
		return _rnd.getNextNumber() >= 25;
	case 2:
		return _rnd.getNextNumber() >= 50;
	case 3:
		return _rnd.getNextNumber() >= 75;
	case 4:
		return _rnd.getNextNumber() >= 90;
	case 5:
		_andyUpdatePositionFlag = true;
		return 1;
	default:
		error("moveAndyObjectOp4 op %d", op);
		break;
	}
	return 0;
}

void Game::setupAndyObjectMoveData(LvlObject *ptr) {
	_andyMoveData.xPos = ptr->xPos;
	_andyMoveData.yPos = ptr->yPos;
	_andyPosX = _res->_screensBasePos[ptr->screenNum].u + _andyMoveData.xPos;
	_andyPosY = _res->_screensBasePos[ptr->screenNum].v + _andyMoveData.yPos;
	_andyMoveData.anim = ptr->anim;
	_andyMoveData.unkA = ptr->currentSprite;
	_andyMoveData.frame = ptr->frame;
	_andyMoveData.flags0 = ptr->flags0;
	_andyMoveData.flags1 = ptr->flags1;
	_andyMoveMask = (ptr->flags1 >> 4) & 3;
	_andyMoveData.unk16 = ptr->width;
	_andyMoveData.unk18 = ptr->height;
	const LvlObjectData *dat = ptr->levelData0x2988;
	_andyMoveData.unkC = dat->hotspotsCount;
	_andyMoveData.unk1C = dat->animsInfoData;
	_andyMoveData.framesData = dat->framesData;
	_andyMoveData.unk24 = dat->movesData;
	_andyMoveData.unk28 = dat->hotspotsData;
	_andyLevelData0x288PosTablePtr = ptr->posTable;
}

void Game::setupAndyObjectMoveState() {
	int yPos, xPos;

	yPos = _andyLevelData0x288PosTablePtr[1].y + _andyPosY;
	xPos = _andyLevelData0x288PosTablePtr[1].x + _andyPosX;
	_andyMaskBufferPos1 = screenMaskOffset(xPos, yPos);
	_andyMoveState[0x0] = _screenMaskBuffer[_andyMaskBufferPos1 - 0x800];
	_andyMoveState[0x1] = _screenMaskBuffer[_andyMaskBufferPos1 - 0x3FD];
	_andyMoveState[0x2] = _screenMaskBuffer[_andyMaskBufferPos1 + 6];
	_andyMoveState[0x3] = _screenMaskBuffer[_andyMaskBufferPos1 + 0x403];
	_andyMoveState[0x4] = _screenMaskBuffer[_andyMaskBufferPos1 + 0x800];
	_andyMoveState[0x5] = _screenMaskBuffer[_andyMaskBufferPos1 + 0x3FD];
	_andyMoveState[0x6] = _screenMaskBuffer[_andyMaskBufferPos1 - 6];
	_andyMoveState[0x7] = _screenMaskBuffer[_andyMaskBufferPos1 - 0x403];

	yPos = _andyLevelData0x288PosTablePtr[2].y + _andyPosY;
	xPos = _andyLevelData0x288PosTablePtr[2].x + _andyPosX;
	_andyMaskBufferPos2 = screenMaskOffset(xPos, yPos);
	_andyMoveState[0x8] = _screenMaskBuffer[_andyMaskBufferPos2 - 0x800];
	_andyMoveState[0x9] = _screenMaskBuffer[_andyMaskBufferPos2 - 0x3FD];
	_andyMoveState[0xA] = _screenMaskBuffer[_andyMaskBufferPos2 + 6];
	_andyMoveState[0xB] = _screenMaskBuffer[_andyMaskBufferPos2 + 0x403];
	_andyMoveState[0xC] = _screenMaskBuffer[_andyMaskBufferPos2 + 0x800];
	_andyMoveState[0xD] = _screenMaskBuffer[_andyMaskBufferPos2 + 0x3FD];
	_andyMoveState[0xE] = _screenMaskBuffer[_andyMaskBufferPos2 - 6];
	_andyMoveState[0xF] = _screenMaskBuffer[_andyMaskBufferPos2 - 0x403];

	yPos = _andyLevelData0x288PosTablePtr[4].y + _andyPosY;
	xPos = _andyLevelData0x288PosTablePtr[4].x + _andyPosX;
	_andyMaskBufferPos4 = screenMaskOffset(xPos, yPos);
	_andyMoveState[0x10] = _screenMaskBuffer[_andyMaskBufferPos4 - 0x600];
	_andyMoveState[0x11] = _screenMaskBuffer[_andyMaskBufferPos4 - 0x1FD];
	_andyMoveState[0x12] = _screenMaskBuffer[_andyMaskBufferPos4 + 0x206];
	_andyMoveState[0x13] = _screenMaskBuffer[_andyMaskBufferPos4 + 0x603];
	_andyMoveState[0x14] = _screenMaskBuffer[_andyMaskBufferPos4 + 0xA00];
	_andyMoveState[0x15] = _screenMaskBuffer[_andyMaskBufferPos4 + 0x5FD];
	_andyMoveState[0x16] = _screenMaskBuffer[_andyMaskBufferPos4 + 0x1FA];
	_andyMoveState[0x17] = _screenMaskBuffer[_andyMaskBufferPos4 - 0x203];

	yPos = _andyLevelData0x288PosTablePtr[5].y + _andyPosY;
	xPos = _andyLevelData0x288PosTablePtr[5].x + _andyPosX;
	_andyMaskBufferPos5 = screenMaskOffset(xPos, yPos);
	_andyMoveState[0x18] = _screenMaskBuffer[_andyMaskBufferPos5 - 0x600];
	_andyMoveState[0x19] = _screenMaskBuffer[_andyMaskBufferPos5 - 0x1FD];
	_andyMoveState[0x1A] = _screenMaskBuffer[_andyMaskBufferPos5 + 0x206];
	_andyMoveState[0x1B] = _screenMaskBuffer[_andyMaskBufferPos5 + 0x603];
	_andyMoveState[0x1C] = _screenMaskBuffer[_andyMaskBufferPos5 + 0xA00];
	_andyMoveState[0x1D] = _screenMaskBuffer[_andyMaskBufferPos5 + 0x5FD];
	_andyMoveState[0x1E] = _screenMaskBuffer[_andyMaskBufferPos5 + 0x1FA];
	_andyMoveState[0x1F] = _screenMaskBuffer[_andyMaskBufferPos5 - 0x203];

	yPos = _andyLevelData0x288PosTablePtr[3].y + _andyPosY;
	xPos = _andyLevelData0x288PosTablePtr[3].x + _andyPosX;
	_andyMaskBufferPos3 = screenMaskOffset(xPos, yPos);

	yPos = _andyLevelData0x288PosTablePtr[0].y + _andyPosY;
	xPos = _andyLevelData0x288PosTablePtr[0].x + _andyPosX;
	_andyMaskBufferPos0 = screenMaskOffset(xPos, yPos);

	yPos = _andyLevelData0x288PosTablePtr[7].y + _andyPosY;
	xPos = _andyLevelData0x288PosTablePtr[7].x + _andyPosX;
	_andyMaskBufferPos7 = screenMaskOffset(xPos, yPos);

	yPos = _andyLevelData0x288PosTablePtr[6].y + _andyPosY;
	xPos = _andyLevelData0x288PosTablePtr[6].x + _andyPosX;
	_andyMaskBufferPos6 = screenMaskOffset(xPos, yPos);
}

void Game::updateAndyObject(LvlObject *ptr) {
	_andyUpdatePositionFlag = false;
	int xPos = 0;
	int yPos = 0;
	int mask = 0;

	_actionKeyMask = ptr->actionKeyMask;
	_directionKeyMask = ptr->directionKeyMask;
	LvlObjectData *dat = ptr->levelData0x2988;

	int currentAnimFrame = ptr->frame;
	int currentAnim = ptr->anim;

	LvlAnimHeader *ah = ((LvlAnimHeader *)(dat->animsInfoData + kLvlAnimHdrOffset)) + ptr->anim;
	assert(ptr->frame < ah->seqCount);
	LvlAnimSeqHeader *ash = ((LvlAnimSeqHeader *)(dat->animsInfoData + ah->seqOffset)) + ptr->frame;
	LvlAnimSeqFrameHeader *asfh = (LvlAnimSeqFrameHeader *)(dat->animsInfoData + ash->offset);

	int count = ash->count;
	if (count == 0) goto sameAnim;

	setupAndyObjectMoveData(ptr);
	if (dat->frame != 0) {
		setupAndyObjectMoveState();
	}
	while (count != 0) {
		--count;
		assert(asfh[count].move < dat->movesCount);
		LvlSprMoveData *m = ((LvlSprMoveData *)dat->movesData) + asfh[count].move;
		int op = m->op1 * 4 + ((ptr->flags1 >> 4) & 3);
		mask = moveAndyObjectOp1(op);
		if (mask == 0) {
			continue;
		}
		mask &= moveAndyObjectOp2(m->op2);
		if (mask == 0) {
			continue;
		}
		mask &= moveAndyObjectOp3(m->op3);
		if (mask == 0) {
			continue;
		}
		mask &= moveAndyObjectOp4(m->op4);
		if (mask == 0) {
			continue;
		}
		break;
	}
	ptr->flags0 = _andyMoveData.flags0;
	ptr->flags1 = _andyMoveData.flags1;
	ptr->xPos = _andyMoveData.xPos;
	ptr->yPos = _andyMoveData.yPos;
	if (_andyUpdatePositionFlag) {
		xPos = ptr->posTable[7].x + ptr->xPos;
		yPos = ptr->posTable[7].y + ptr->yPos;
	}
	if (mask) {

		assert(count < ash->count);
		currentAnimFrame = asfh[count].frame;
		currentAnim = asfh[count].anim;

		ah = (LvlAnimHeader *)(dat->animsInfoData + kLvlAnimHdrOffset) + currentAnim;
		ash = (LvlAnimSeqHeader *)(dat->animsInfoData + ah->seqOffset) + currentAnimFrame;
		asfh = &asfh[count];

		uint16_t w, h;
		_res->getLvlSpriteFramePtr(dat, ash->firstFrame, &w, &h);

		ptr->flags1 = ((ptr->flags1 & 0x30) ^ ((asfh->unk5 & 3) << 4)) | (ptr->flags1 & ~0x30);
		int type = (ptr->flags1 >> 4) & 3;

		switch (type) {
		case 0:
			ptr->xPos += asfh->unk6;
			ptr->yPos += asfh->unk7;
			break;
		case 1:
			ptr->xPos += ptr->width - asfh->unk6 - w;
			ptr->yPos += asfh->unk7;
			break;
		case 2:
			ptr->xPos += asfh->unk6;
			ptr->yPos += ptr->height - asfh->unk7 - h;
			break;
		case 3:
			ptr->xPos += ptr->width - asfh->unk6 - w;
			ptr->yPos += ptr->height - asfh->unk7 - h;
			break;
		}

	} else {
sameAnim:

		uint16_t frame1_w, frame1_h;
		_res->getLvlSpriteFramePtr(dat, ash->firstFrame, &frame1_w, &frame1_h);

		++currentAnimFrame;
		if (currentAnimFrame >= ah->seqCount) {
			currentAnimFrame = 0;
		}

		ash = (LvlAnimSeqHeader *)(dat->animsInfoData + ah->seqOffset) + currentAnimFrame;

		uint16_t frame2_w, frame2_h;
		_res->getLvlSpriteFramePtr(dat, ash->firstFrame, &frame2_w, &frame2_h);

		int dw = frame2_w - frame1_w;
		int dh = frame2_h - frame1_h;

		const int type = (ptr->flags1 >> 4) & 3;
		switch (type) {
		case 0:
			ptr->xPos += ash->dx;
			ptr->yPos += ash->dy;
			break;
		case 1:
			ptr->xPos -= ash->dx + dw;
			ptr->yPos += ash->dy;
			break;
		case 2:
			ptr->xPos += ash->dx;
			ptr->yPos -= ash->dy + dh;
			break;
		case 3:
			ptr->xPos -= ash->dx + dw;
			ptr->yPos -= ash->dy + dh;
			break;
		}
	}

	ptr->anim = currentAnim;
	ptr->frame = currentAnimFrame;

	ptr->currentSound = ash->sound;
	if (ptr->currentSound != 0xFFFF && ptr->type == 8 && ptr->spriteNum < 5) {
		playSound(ptr->currentSound, ptr, 0, 0);
	}

	ptr->flags0 = merge_bits(ptr->flags0, ash->flags0, 0x3FF);
	ptr->flags1 = merge_bits(ptr->flags1, ash->flags1, 6);
	ptr->flags1 = merge_bits(ptr->flags1, ash->flags1, 8);

	ptr->currentSprite = ash->firstFrame;

	ptr->bitmapBits = _res->getLvlSpriteFramePtr(dat, ash->firstFrame, &ptr->width, &ptr->height);

	LvlSprHotspotData *hs = ((LvlSprHotspotData *)dat->hotspotsData) + ash->firstFrame;

	if (_andyUpdatePositionFlag) {
		ptr->flags1 &= ~0x30;
	}
	const int w = ptr->width - 1;
	const int h = ptr->height - 1;
	const int type = (ptr->flags1 >> 4) & 3;
	for (int i = 0; i < 8; ++i) {
		switch (type) {
		case 0:
			ptr->posTable[i].x = hs->pts[i].x;
			ptr->posTable[i].y = hs->pts[i].y;
			break;
		case 1:
			ptr->posTable[i].x = w - hs->pts[i].x;
			ptr->posTable[i].y = hs->pts[i].y;
			break;
		case 2:
			ptr->posTable[i].x = hs->pts[i].x;
			ptr->posTable[i].y = h - hs->pts[i].y;
			break;
		case 3:
			ptr->posTable[i].x = w - hs->pts[i].x;
			ptr->posTable[i].y = h - hs->pts[i].y;
			break;
		}
	}
	if (_andyUpdatePositionFlag) {
		ptr->xPos = xPos - ptr->posTable[7].x;
		ptr->yPos = yPos - ptr->posTable[7].y;
	}
}
