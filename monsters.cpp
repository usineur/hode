/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "game.h"
#include "level.h"
#include "resource.h"
#include "util.h"

// (lut[x] & 1) indicates a diagonal direction
static const uint8_t _mstLut1[] = {
	// 0
	0x08,
	0x00, // kDirectionKeyMaskUp
	0x02, // kDirectionKeyMaskRight
	0x01, // kDirectionKeyMaskUp | kDirectionKeyMaskRight
	// 4
	0x04, // kDirectionKeyMaskDown
	0x00, // kDirectionKeyMaskDown | kDirectionKeyMaskUp
	0x03, // kDirectionKeyMaskDown | kDirectionKeyMaskRight
	0x01, // kDirectionKeyMaskDown | kDirectionKeyMaskUp | kDirectionKeyMaskRight
	// 8
	0x06, // kDirectionKeyMaskLeft
	0x07, // kDirectionKeyMaskLeft | kDirectionKeyMaskUp
	0x02, // kDirectionKeyMaskLeft | kDirectionKeyMaskRight
	0x01, // kDirectionKeyMaskLeft | kDirectionKeyMaskUp | kDirectionKeyMaskRight
	// 12
	0x05, // kDirectionKeyMaskLeft | kDirectionKeyMaskDown
	0x07, // kDirectionKeyMaskLeft | kDirectionKeyMaskDown | kDirectionKeyMaskRight
	0x03, // kDirectionKeyMaskLeft | kDirectionKeyMaskDown | kDirectionKeyMaskRight
	0x01  // kDirectionKeyMaskLeft | kDirectionKeyMaskDown | kDirectionKeyMaskUp | kDirectionKeyMaskRight
};

// indexes _mstLut1
static const uint8_t _mstLut3[8 * 5] = {
	0x01, 0x03, 0x09, 0x02, 0x08, 0x03, 0x02, 0x01, 0x06, 0x09, 0x02, 0x06, 0x03, 0x04, 0x01, 0x06,
	0x04, 0x02, 0x0C, 0x03, 0x04, 0x0C, 0x06, 0x08, 0x02, 0x0C, 0x08, 0x04, 0x09, 0x06, 0x08, 0x09,
	0x0C, 0x01, 0x04, 0x09, 0x01, 0x08, 0x03, 0x0C
};

// monster frames animation (minus #0, #4, #8, #12, #16, #20, eg. every 4th is skipped)
static const uint8_t _mstLut4[] = {
	0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x11, 0x12, 0x13, 0x15,
	0x16, 0x17
};

// indexes _mstLut4 clipped to [0,17], repeat the frame preceding the skipped ones
static const uint8_t _mstLut5[] = {
	0x00, 0x00, 0x01, 0x02, 0x03, 0x03, 0x04, 0x05, 0x06, 0x06, 0x07, 0x08, 0x09, 0x09, 0x0A, 0x0B,
	0x0C, 0x0C, 0x0D, 0x0E, 0x0F, 0x0F, 0x10, 0x11, 0x12, 0x12, 0x13, 0x14, 0x15, 0x15, 0x16, 0x17
};

static const uint8_t _fireflyPosData[] = {
	0, 1, 2, 3, 4, 4, 4, 0xFF, 0, 1, 2, 2, 4, 4, 3, 0xFF,
	0, 1, 1, 2, 3, 2, 3, 0xFF, 0, 1, 1, 2, 3, 3, 3, 0xFF,
	0, 0, 1, 2, 2, 2, 2, 0xFF, 4, 4, 4, 3, 2, 1, 0, 0xFF,
	3, 4, 4, 2, 2, 1, 0, 0xFF, 3, 2, 3, 2, 1, 1, 0, 0xFF,
	3, 3, 3, 2, 1, 1, 0, 0xFF, 2, 2, 2, 2, 1, 0, 0, 0xFF
};

static const uint8_t kUndefinedMonsterByteCode[] = { 0x12, 0x34 };

static uint8_t mstGetFacingDirectionMask(uint8_t a) {
	uint8_t r = 0;
	if (a & kDirectionKeyMaskLeft) { // Andy left facing monster
		r |= 1;
	}
	if (a & kDirectionKeyMaskDown) { // Andy below the monster
		r |= 2;
	}
	return r;
}

void Game::mstMonster1ResetData(MonsterObject1 *m) {
	m->m46 = 0;
	LvlObject *o = m->o16;
	if (o) {
		o->dataPtr = 0;
	}
	for (int i = 0; i < kMaxMonsterObjects2; ++i) {
		MonsterObject2 *mo = &_monsterObjects2Table[i];
		if (mo->monster2Info && mo->monster1 == m) {
			mo->monster1 = 0;
		}
	}
}

void Game::mstMonster2InitFirefly(MonsterObject2 *m) {
	LvlObject *o = m->o;
	m->x1 = _res->_mstPointOffsets[o->screenNum].xOffset;
	m->y1 = _res->_mstPointOffsets[o->screenNum].yOffset;
	m->x2 = m->x1 + 255;
	m->y2 = m->y1 + 191;
	const uint32_t num = _rnd.update();
	m->hPosIndex = num % 40;
	if (_fireflyPosData[m->hPosIndex] != 0xFF) {
		m->hPosIndex &= ~7;
	}
	m->vPosIndex = m->hPosIndex;
	if (num & 0x80) {
		m->hPosIndex += 40;
	} else {
		m->vPosIndex += 40;
	}
	m->hDir = (num >> 16) & 1;
	m->vDir = (num >> 17) & 1;
}

void Game::mstMonster2ResetData(MonsterObject2 *m) {
	m->monster2Info = 0;
	LvlObject *o = m->o;
	if (o) {
		o->dataPtr = 0;
	}
}

void Game::mstTaskSetMonster2ScreenPosition(Task *t) {
	MonsterObject2 *m = t->monster2;
	LvlObject *o = m->o;
	m->xPos = o->xPos + o->posTable[7].x;
	m->yPos = o->yPos + o->posTable[7].y;
	m->xMstPos = m->xPos + _res->_mstPointOffsets[o->screenNum].xOffset;
	m->yMstPos = m->yPos + _res->_mstPointOffsets[o->screenNum].yOffset;
}

void Game::mstMonster1ResetWalkPath(MonsterObject1 *m) {
	_rnd.resetMst(m->rnd_m35);
	_rnd.resetMst(m->rnd_m49);

	const uint8_t *ptr = m->monsterInfos;
	const int num = (~m->flagsA5) & 1;

	m->levelPosBounds_x2 = m->walkNode->coords[0][num] - (int32_t)READ_LE_UINT32(ptr + 904);
	m->levelPosBounds_x1 = m->walkNode->coords[1][num] + (int32_t)READ_LE_UINT32(ptr + 904);
	m->levelPosBounds_y2 = m->walkNode->coords[2][num] - (int32_t)READ_LE_UINT32(ptr + 908);
	m->levelPosBounds_y1 = m->walkNode->coords[3][num] + (int32_t)READ_LE_UINT32(ptr + 908);

	const uint32_t indexWalkCode = m->walkNode->walkCodeReset[num];
	m->walkCode = (indexWalkCode == kNone) ? 0 : &_res->_mstWalkCodeData[indexWalkCode];
}

bool Game::mstUpdateInRange(MstMonsterAction *m) {
	if (m->unk4 == 0) {
		if (mstHasMonsterInRange(m, 0) && addChasingMonster(m, 0)) {
			return true;
		}
	} else {
		int direction = _rnd.update() & 1;
		if (mstHasMonsterInRange(m, direction) && addChasingMonster(m, direction)) {
			return true;
		}
		direction ^= 1;
		if (mstHasMonsterInRange(m, direction) && addChasingMonster(m, direction)) {
			return true;
		}
	}
	return false;
}

bool Game::addChasingMonster(MstMonsterAction *m48, uint8_t direction) {
	debug(kDebug_MONSTER, "addChasingMonster %d", direction);
	m48->direction = direction;
	if (m48->codeData != kNone) {
		Task *t = createTask(_res->_mstCodeData + m48->codeData * 4);
		if (!t) {
			return false;
		}
		while ((this->*(t->run))(t) == 0);
	}
	_mstActionNum = m48 - &_res->_mstMonsterActionData[0];
	_mstChasingMonstersCount = 0;
	for (int i = 0; i < m48->areaCount; ++i) {
		MstMonsterAreaAction *unk4 = m48->area[i].data;
		const uint8_t num = unk4->monster1Index;
		if (num != 0xFF) {
			assert(num < kMaxMonsterObjects1);
			unk4->direction = direction;
			MonsterObject1 *m = &_monsterObjects1Table[num];
			m->action = unk4;
			m->flags48 |= 0x40;
			m->flagsA5 = (m->flagsA5 & ~0x75) | 0xA;
			mstMonster1ResetWalkPath(m);
			Task *current = _monsterObjects1TasksList;
			Task *t = m->task;
			assert(t);
			while (current) {
				Task *next = current->nextPtr;
				if (current == t) {
					removeTask(&_monsterObjects1TasksList, t);
					appendTask(&_monsterObjects1TasksList, t);
					break;
				}
				current = next;
			}
			const uint32_t codeData = unk4->codeData;
			assert(codeData != kNone);
			resetTask(t, _res->_mstCodeData + codeData * 4);
			++_mstChasingMonstersCount;
		}
	}
	debug(kDebug_MONSTER, "_mstChasingMonstersCount %d", _mstChasingMonstersCount);
	return true;
}

void Game::mstMonster1ClearChasingMonster(MonsterObject1 *m) {
	m->flags48 &= ~0x50;
	m->action->monster1Index = 0xFF;
	m->action = 0;
	--_mstChasingMonstersCount;
	if (_mstChasingMonstersCount <= 0) {
		_mstActionNum = -1;
	}
}

void Game::mstTaskSetMonster1BehaviorState(Task *t, MonsterObject1 *m, int num) {
	MstBehaviorState *behaviorState = &m->m46->data[num];
	m->behaviorState = behaviorState;
	m->monsterInfos = _res->_mstMonsterInfos + behaviorState->indexMonsterInfo * kMonsterInfoDataSize;
	if (behaviorState->indexUnk51 == kNone) {
		m->flags48 &= ~4;
	}
	const uint32_t indexWalkPath = behaviorState->walkPath;
	m->walkNode = _res->_mstWalkPathData[indexWalkPath].data;
	mstTaskUpdateScreenPosition(t);
	if (!mstMonster1UpdateWalkPath(m)) {
		mstMonster1ResetWalkPath(m);
	}
	if (t->run == &Game::mstTask_monsterWait4) {
		t->run = &Game::mstTask_main;
	}
	if ((m->flagsA5 & 8) == 0 && t->run == &Game::mstTask_idle) {
		mstTaskSetNextWalkCode(t);
	}
}

int Game::mstTaskStopMonsterObject1(Task *t) {
	if (_mstActionNum == -1) {
		return 0;
	}
	MonsterObject1 *m = t->monster1;

	// bugfix: original code meant to check bit 3 directly ?

	// const uint8_t r = (m->flagsA5 == 0) ? 1 : 0;
	// if ((r & 8) != 0) {
	//   return 0;
	// }

	const MstMonsterAreaAction *m48 = m->action;
	if (!m48) {
		return 0;
	}
	const uint32_t codeData = m48->codeData2;
	if (codeData != kNone) {
		resetTask(t, _res->_mstCodeData + codeData * 4);
		mstMonster1ClearChasingMonster(m);
		return 0;
	}
	mstMonster1ClearChasingMonster(m);
	if (m->flagsA5 & 0x80) {
		m->flagsA5 &= ~8;
		const uint32_t codeData = m->behaviorState->codeData;
		if (codeData != kNone) {
			resetTask(t, _res->_mstCodeData + codeData * 4);
			return 0;
		}
		m->o16->actionKeyMask = 7;
		m->o16->directionKeyMask = 0;
		t->run = &Game::mstTask_idle;
		return 0;
	}
	m->flagsA5 = (m->flagsA5 & ~9) | 6;
	if (m->flagsA6 & 2) {
		return 0;
	}
	return mstTaskInitMonster1Type2(t, 1);
}

void Game::mstMonster1SetScreenPosition(MonsterObject1 *m) {
	LvlObject *o = m->o16;
	o->xPos = m->xPos - o->posTable[7].x;
	o->yPos = m->yPos - o->posTable[7].y;
	m->xMstPos = m->xPos + _res->_mstPointOffsets[o->screenNum].xOffset;
	m->yMstPos = m->yPos + _res->_mstPointOffsets[o->screenNum].yOffset;
}

bool Game::mstMonster1SetWalkingBounds(MonsterObject1 *m) {
	MstBehaviorState *behaviorState = m->behaviorState;
	const uint32_t indexWalkPath = behaviorState->walkPath;
	MstWalkPath *walkPath = &_res->_mstWalkPathData[indexWalkPath];
	MstWalkNode *walkNode = walkPath->data;

	int x = m->xMstPos; // ve
	int y = m->yMstPos; // vd
	if (m->levelPosBounds_x1 >= 0) {
		if (x < m->levelPosBounds_x1) {
			x = m->levelPosBounds_x1;
		} else if (x > m->levelPosBounds_x2) {
			x = m->levelPosBounds_x2;
		}
		if (y < m->levelPosBounds_y1) {
			y = m->levelPosBounds_y1;
		} else if (y > m->levelPosBounds_y2) {
			y = m->levelPosBounds_y2;
		}
	}

	const uint32_t indexWalkBox = walkPath->data[0].walkBox;
	const MstWalkBox *m34 = &_res->_mstWalkBoxData[indexWalkBox]; // vg
	int xWalkBox = (m34->right - m34->left) / 2 + m34->left; // vc

	int minDistance = 0x1000000; // vf
	int yWalkBox = y;

	uint32_t i = 0;
	for (; i < walkPath->count; ++i) {
		const uint32_t indexWalkBox = walkPath->data[i].walkBox;
		const MstWalkBox *m34 = &_res->_mstWalkBoxData[indexWalkBox]; // vg
		if (!rect_contains(m34->left, m34->top, m34->right, m34->bottom, x, y)) {
			// find the closest box
			const int d1 = ABS(x - m34->left);
			if (d1 < minDistance) {
				minDistance = d1;
				walkNode = &walkPath->data[i];
				xWalkBox = m34->left;
			}
			const int d2 = ABS(x - m34->right);
			if (d2 < minDistance) {
				minDistance = d2;
				walkNode = &walkPath->data[i];
				xWalkBox = m34->right;
			}
		} else {
			// find match, point is in the box
			xWalkBox = x;
			yWalkBox = y;
			walkNode = &walkPath->data[i];
			break;
		}
	}
	if (i == walkPath->count) {
		// calculate the yPos for the walkBox
		const uint32_t indexWalkBox = walkNode->walkBox;
		const MstWalkBox *m34 = &_res->_mstWalkBoxData[indexWalkBox]; // vg
		if (y <= m34->bottom) {
			y = (m34->bottom - m34->top) / 2 + m34->top;
		}
		yWalkBox = y; // vd
	}

	// find screenNum for level position
	int screenNum = -1;
	int xLevelPos;
	int yLevelPos;
	for (int i = 0; i < _res->_mstHdr.screensCount; ++i) {
		xLevelPos = _res->_mstPointOffsets[i].xOffset;
		yLevelPos = _res->_mstPointOffsets[i].yOffset;
		if (rect_contains(xLevelPos, yLevelPos, xLevelPos + 255, yLevelPos + 191, xWalkBox, yWalkBox)) {
			screenNum = i;
			break;
		}
	}
	if (screenNum == -1) {
		screenNum = 0;
		xLevelPos = _res->_mstPointOffsets[0].xOffset + 256 / 2;
		yLevelPos = _res->_mstPointOffsets[0].yOffset + 192 / 2;
	}
	LvlObject *o = m->o16;
	o->screenNum = screenNum;
	m->xPos = xWalkBox - xLevelPos;
	m->yPos = yWalkBox - yLevelPos;
	mstMonster1SetScreenPosition(m);
	m->walkNode = walkNode;
	mstMonster1ResetWalkPath(m);
	return true;
}

bool Game::mstMonster1UpdateWalkPath(MonsterObject1 *m) {
	debug(kDebug_MONSTER, "mstMonster1UpdateWalkPath m %p", m);
	const uint8_t screenNum = m->o16->screenNum;
	MstBehaviorState *behaviorState = m->behaviorState;
	const uint32_t indexWalkPath = behaviorState->walkPath;
	MstWalkPath *walkPath = &_res->_mstWalkPathData[indexWalkPath];
	// start from screen number
	uint32_t indexWalkNode = walkPath->walkNodeData[screenNum];
	if (indexWalkNode != kNone) {
		MstWalkNode *walkNode = &walkPath->data[indexWalkNode];
		uint32_t indexWalkBox = walkNode->walkBox;
		const MstWalkBox *m34 = &_res->_mstWalkBoxData[indexWalkBox];
		while (m34->left <= m->xMstPos) {
			if (m34->right >= m->xMstPos && m34->top <= m->yMstPos && m34->bottom >= m->yMstPos) {
				if (m->walkNode == walkNode) {
					return false;
				}
				m->walkNode = walkNode;
				mstMonster1ResetWalkPath(m);
				return true;
			}

			indexWalkNode = walkNode->nextWalkNode;
			if (indexWalkNode == kNone) {
				break;
			}
			walkNode = &walkPath->data[indexWalkNode];
			indexWalkBox = walkNode->walkBox;
			m34 = &_res->_mstWalkBoxData[indexWalkBox];
		}
	}
	return mstMonster1SetWalkingBounds(m);
}

uint32_t Game::mstMonster1GetNextWalkCode(MonsterObject1 *m) {
	MstWalkCode *walkCode = m->walkCode;
	int num = 0;
	if (walkCode->indexDataCount != 0) {
		num = _rnd.getMstNextNumber(m->rnd_m35);
		num = walkCode->indexData[num];
	}
	const uint32_t codeData = walkCode->codeData[num];
	return codeData;
}

int Game::mstTaskSetNextWalkCode(Task *t) {
	MonsterObject1 *m = t->monster1;
	assert(m);
	const uint32_t codeData = mstMonster1GetNextWalkCode(m);
	assert(codeData != kNone);
	resetTask(t, _res->_mstCodeData + codeData * 4);
	const int counter = m->executeCounter;
	m->executeCounter = _executeMstLogicCounter;
	return m->executeCounter - counter;
}

void Game::mstBoundingBoxClear(MonsterObject1 *m, int dir) {
	assert(dir == 0 || dir == 1);
	uint8_t num = m->bboxNum[dir];
	if (num < _mstBoundingBoxesCount && _mstBoundingBoxesTable[num].monster1Index == m->monster1Index) {
		_mstBoundingBoxesTable[num].monster1Index = 0xFF;
		int i = num;
		for (; i < _mstBoundingBoxesCount; ++i) {
			if (_mstBoundingBoxesTable[i].monster1Index != 0xFF) {
				break;
			}
		}
		if (i == _mstBoundingBoxesCount) {
			_mstBoundingBoxesCount = num;
		}
	}
	m->bboxNum[dir] = 0xFF;
}

int Game::mstBoundingBoxCollides1(int num, int x1, int y1, int x2, int y2) const {
	for (int i = 0; i < _mstBoundingBoxesCount; ++i) {
		const MstBoundingBox *p = &_mstBoundingBoxesTable[i];
		if (p->monster1Index == 0xFF || num == p->monster1Index) {
			continue;
		}
		if (rect_intersects(x1, y1, x2, y2, p->x1, p->y1, p->x2, p->y2)) {
			return i + 1;
		}
	}
	return 0;
}

int Game::mstBoundingBoxUpdate(int num, int monster1Index, int x1, int y1, int x2, int y2) {
	if (num == 0xFF) {
		for (num = 0; num < _mstBoundingBoxesCount; ++num) {
			if (_mstBoundingBoxesTable[num].monster1Index == 0xFF) {
				break;
			}
		}
		assert(num < kMaxBoundingBoxes);
		MstBoundingBox *p = &_mstBoundingBoxesTable[num];
		p->x1 = x1;
		p->y1 = y1;
		p->x2 = x2;
		p->y2 = y2;
		p->monster1Index = monster1Index;
		if (num == _mstBoundingBoxesCount) {
			++_mstBoundingBoxesCount;
		}
	} else if (num < _mstBoundingBoxesCount) {
		MstBoundingBox *p = &_mstBoundingBoxesTable[num];
		if (p->monster1Index == monster1Index) {
			p->x1 = x1;
			p->y1 = y1;
			p->x2 = x2;
			p->y2 = y2;
		}
	}
	return num;
}

int Game::mstBoundingBoxCollides2(int monster1Index, int x1, int y1, int x2, int y2) const {
	for (int i = 0; i < _mstBoundingBoxesCount; ++i) {
		const MstBoundingBox *p = &_mstBoundingBoxesTable[i];
		if (p->monster1Index == 0xFF || p->monster1Index == monster1Index) {
			continue;
		}
		if (p->monster1Index == 0xFE) { // Andy
			if (_monsterObjects1Table[monster1Index].monsterInfos[944] != 15) {
				continue;
			}
		}
		if (rect_intersects(x1, y1, x2, y2, p->x1, p->y1, p->x2, p->y2)) {
			return i + 1;
		}
	}
	return 0;
}

int Game::getMstDistance(int y, const AndyShootData *p) const {
	switch (p->directionMask) {
	case 0:
		if (p->boundingBox.x1 <= _mstTemp_x2 && p->boundingBox.y2 >= _mstTemp_y1 && p->boundingBox.y1 <= _mstTemp_y2) {
			const int dx = _mstTemp_x1 - p->boundingBox.x2;
			if (dx <= 0) {
				return 0;
			}
			return (p->type == 3) ? 0 : dx / p->width;
		}
		break;
	case 1:
		{
			const int dx = p->boundingBox.x2 - _mstTemp_x1;
			if (dx >= 0) {
				const int dy = p->boundingBox.y1 - dx * 2 / 3;
				if (dy <= _mstTemp_y2) {
					const int dx2 = p->boundingBox.x1 - _mstTemp_x2;
					if (dx2 <= 0) {
						return (p->boundingBox.y2 >= _mstTemp_y1) ? 0 : -1;
					}
					const int dy2 = p->boundingBox.y2 + dx2 * 2 / 3;
					if (dy2 > _mstTemp_y1) {
						return (p->type == 3) ? 0 : dx2 / p->width;
					}
				}
			}
		}
		break;
	case 2: {
			const int dx = _mstTemp_x1 - p->boundingBox.x2;
			if (dx >= 0) {
				const int dy = p->boundingBox.y2 + dx * 2 / 3;
				if (dy >= _mstTemp_x1) {
					const int dx2 = p->boundingBox.x1 - _mstTemp_x2;
					if (dx2 <= 0) {
						return (p->boundingBox.y1 <= _mstTemp_y2) ? 0 : -1;
					}
					const int dy2 = p->boundingBox.y1 + dx2 * 2 / 3;
					if (dy2 <= _mstTemp_y2) {
						return (p->type == 3) ? 0 : dx2 / p->width;
					}
				}
			}
		}
		break;
	case 3: {
			const int dx = _mstTemp_x2 - p->boundingBox.x1;
			if (dx >= 0) {
				const int dy = p->boundingBox.y1 - dx * 2 / 3;
				if (dy <= _mstTemp_y2) {
					const int dx2 = _mstTemp_x1 - p->boundingBox.x2;
					if (dx2 <= 0) {
						return (p->boundingBox.y2 >= _mstTemp_y1) ? 0 : -1;
					}
					const int dy2 =  p->boundingBox.y2 - dx2 * 2 / 3;
					if (dy2 >= _mstTemp_y1) {
						return (p->type == 3) ? 0 : dx2 / p->width;
					}
				}
			}
		}
		break;
	case 4:
		{
			const int dx = _mstTemp_x2 - p->boundingBox.x1;
			if (dx >= 0) {
				const int dy = dx * 2 / 3 + p->boundingBox.y2;
				if (dy >= _mstTemp_y1) {
					const int dx2 = _mstTemp_x1 - p->boundingBox.x2;
					if (dx2 <= 0) {
						return (p->boundingBox.y1 <= _mstTemp_y2) ? 0 : -1;
					}
					const int dy2 = dx2 * 2 / 3 + + p->boundingBox.y1;
					if (dy2 <= _mstTemp_y2) {
						return (p->type == 3) ? 0 : dx2 / p->width;
					}
				}
			}
		}
		break;
	case 5:
		if (p->boundingBox.x2 >= _mstTemp_x1 && p->boundingBox.y2 >= _mstTemp_y1 && p->boundingBox.y1 <= _mstTemp_y2) {
			const int dx = p->boundingBox.x1 - _mstTemp_x2;
			if (dx <= 0) {
				return 0;
			}
			return (p->type == 3) ? 0 : dx / p->width;
		}
		break;
	case 6:
		if (p->boundingBox.y2 >= _mstTemp_y1 && p->boundingBox.x2 >= _mstTemp_x1 && p->boundingBox.x1 <= _mstTemp_x2) {
			const int dy = p->boundingBox.y1 - _mstTemp_y2;
			if (dy <= 0) {
				return 0;
			}
			return (p->type == 3) ? 0 : dy / p->height;
		}
		break;
	case 7:
		if (p->boundingBox.y1 <= _mstTemp_y2 && p->boundingBox.x2 >= _mstTemp_x1 && p->boundingBox.x1 <= _mstTemp_x2) {
			const int dy = _mstTemp_y1 - p->boundingBox.y2;
			if (dy <= 0) {
				return 0;
			}
			return (p->type == 3) ? 0 : dy / p->height;
		}
		break;
	case 128: // 8
		if (p->boundingBox.x1 <= _mstTemp_x2 && y >= _mstTemp_y1 && y - _mstTemp_y2 <= 9) {
			const int dx = _mstTemp_x1 - p->boundingBox.x2;
			if (dx <= 0) {
				return 0;
			}
			return dx / p->width;
		}
		break;
	case 133: // 9
		if (p->boundingBox.x2 >= _mstTemp_x1 && y >= _mstTemp_y1 && y - _mstTemp_y2 <= 9) {
			const int dx = _mstTemp_x2 - p->boundingBox.x1;
			if (dx <= 0) {
				return 0;
			}
			return dx / p->width;
		}
		break;
	}
	return -1;
}

void Game::mstTaskUpdateScreenPosition(Task *t) {
	MonsterObject1 *m = t->monster1;
	assert(m);
	LvlObject *o = m->o20;
	if (!o) {
		o = m->o16;
	}
	assert(o);
	m->xPos = o->xPos + o->posTable[7].x;
	m->yPos = o->yPos + o->posTable[7].y;
	m->xMstPos = m->xPos + _res->_mstPointOffsets[o->screenNum].xOffset;
	m->yMstPos = m->yPos + _res->_mstPointOffsets[o->screenNum].yOffset;

	const uint8_t *ptr = m->monsterInfos;
	if (ptr[946] & 4) {
		const uint8_t *ptr1 = ptr + (o->flags0 & 0xFF) * 28; // va
		if (ptr1[0xE] != 0) {
			_mstTemp_x1 = m->xMstPos + (int8_t)ptr1[0xC];
			_mstTemp_y1 = m->yMstPos + (int8_t)ptr1[0xD];
			_mstTemp_x2 = _mstTemp_x1 + ptr1[0xE] - 1;
			_mstTemp_y2 = _mstTemp_y1 + ptr1[0xF] - 1;
			m->bboxNum[0] = mstBoundingBoxUpdate(m->bboxNum[0], m->monster1Index, _mstTemp_x1, _mstTemp_y1, _mstTemp_x2, _mstTemp_y2);
		} else {
			mstBoundingBoxClear(m, 0);
		}
	}
	m->xDelta = _mstAndyLevelPosX - m->xMstPos;
	if (m->xDelta < 0) {
		m->xDelta = -m->xDelta;
		m->facingDirectionMask = kDirectionKeyMaskLeft;
	} else {
		m->facingDirectionMask = kDirectionKeyMaskRight;
	}
	m->yDelta = _mstAndyLevelPosY - m->yMstPos;
	if (m->yDelta < 0) {
		m->yDelta = -m->yDelta;
		m->facingDirectionMask |= kDirectionKeyMaskUp;
	} else {
		m->facingDirectionMask |= kDirectionKeyMaskDown;
	}
	m->collideDistance = -1;
	m->shootData = 0;
	if (_andyShootsCount != 0 && !_specialAnimFlag && (o->flags1 & 6) != 6 && (m->localVars[7] > 0 || m->localVars[7] < -1) && (m->flagsA5 & 0x80) == 0) {
		for (int i = 0; i < _andyShootsCount; ++i) {
			AndyShootData *p = &_andyShootsTable[i];
			if (m->xDelta > 256 || m->yDelta > 192) {
				continue;
			}
			_mstTemp_x1 = o->xPos;
			_mstTemp_y1 = o->yPos;
			_mstTemp_x2 = o->xPos + o->width - 1;
			_mstTemp_y2 = o->yPos + o->height - 1;
			const uint8_t _al = p->type;
			if (_al == 1 || _al == 2) {
				if (p->monsterDistance >= m->xDelta + m->yDelta) {
					if (o->screenNum != _currentScreen) {
						const int dx = _res->_mstPointOffsets[o->screenNum].xOffset - _res->_mstPointOffsets[_currentScreen].xOffset;
						const int dy = _res->_mstPointOffsets[o->screenNum].yOffset - _res->_mstPointOffsets[_currentScreen].yOffset;
						_mstTemp_x1 += dx;
						_mstTemp_x2 += dx;
						_mstTemp_y1 += dy;
						_mstTemp_y2 += dy;
					}
					if (rect_intersects(0, 0, 255, 191, _mstTemp_x1, _mstTemp_y1, _mstTemp_x2, _mstTemp_y2)) {
						const uint8_t type = m->monsterInfos[944];
						if ((type == 9 && clipLvlObjectsSmall(p->o, o, 132)) || (type != 9 && clipLvlObjectsSmall(p->o, o, 20))) {
							p->m = m;
							p->monsterDistance = m->xDelta + m->yDelta;
							p->clipX = _clipBoxOffsetX;
							p->clipY = _clipBoxOffsetY;
						}
					}
					if (o->screenNum != _currentScreen) {
						const int dx = _res->_mstPointOffsets[_currentScreen].xOffset - _res->_mstPointOffsets[o->screenNum].xOffset;
						const int dy = _res->_mstPointOffsets[_currentScreen].yOffset - _res->_mstPointOffsets[o->screenNum].yOffset;
						_mstTemp_x1 += dx;
						_mstTemp_x2 += dx;
						_mstTemp_y1 += dy;
						_mstTemp_y2 += dy;
					}
				}
			} else if (_al == 3 && p->monsterDistance > m->xDelta + m->yDelta) {
				if (o->screenNum != _currentScreen) {
					const int dx = _res->_mstPointOffsets[o->screenNum].xOffset - _res->_mstPointOffsets[_currentScreen].xOffset;
					const int dy = _res->_mstPointOffsets[o->screenNum].yOffset - _res->_mstPointOffsets[_currentScreen].yOffset;
					_mstTemp_x1 += dx;
					_mstTemp_x2 += dx;
					_mstTemp_y1 += dy;
					_mstTemp_y2 += dy;
				}
				if (rect_intersects(0, 0, 255, 191, _mstTemp_x1, _mstTemp_y1, _mstTemp_x2, _mstTemp_y2)) {
					if (testPlasmaCannonPointsDirection(_mstTemp_x1, _mstTemp_y1, _mstTemp_x2, _mstTemp_y2)) {
						p->monsterDistance = m->xDelta + m->yDelta;
						p->m = m;
						p->plasmaCannonPointsCount = _plasmaCannonLastIndex1;
					}
				}
				const int dx = _res->_mstPointOffsets[_currentScreen].xOffset - _res->_mstPointOffsets[o->screenNum].xOffset;
				const int dy = _res->_mstPointOffsets[_currentScreen].yOffset - _res->_mstPointOffsets[o->screenNum].yOffset;
				_mstTemp_x1 += dx;
				_mstTemp_x2 += dx;
				_mstTemp_y1 += dy;
				_mstTemp_y2 += dy;
			}
			_mstTemp_x1 += _res->_mstPointOffsets[o->screenNum].xOffset;
			_mstTemp_y1 += _res->_mstPointOffsets[o->screenNum].yOffset;
			_mstTemp_x2 += _res->_mstPointOffsets[o->screenNum].xOffset;
			_mstTemp_y2 += _res->_mstPointOffsets[o->screenNum].yOffset;
			int res = getMstDistance((m->monsterInfos[946] & 2) != 0 ? p->boundingBox.y2 : m->yMstPos, p);
			if (res < 0) {
				continue;
			}
			if (m->collideDistance == -1 || m->collideDistance > res || (m->collideDistance == 0 && res == 0 && (m->shootData->type & 1) == 0 && p->type == 2)) {
				m->shootData = p;
				m->collideDistance = res;
			}
		}
	}
	if (m->facingDirectionMask & kDirectionKeyMaskLeft) {
		m->goalPosBounds_x1 = READ_LE_UINT32(ptr + 920);
		m->goalPosBounds_x2 = READ_LE_UINT32(ptr + 924);
	} else {
		m->goalPosBounds_x1 = READ_LE_UINT32(ptr + 912);
		m->goalPosBounds_x2 = READ_LE_UINT32(ptr + 916);
	}
	// m->unk90 =
	// m->unk8C =
	if (_andyShootsTable[0].type == 3 && m->shootSource != 0 && _andyShootsTable[0].directionMask == m->shootDirection && m->directionKeyMask == _andyObject->directionKeyMask) {
		m->flags48 |= 0x80;
	} else {
		m->shootDirection = -1;
		m->flags48 &= ~0x80;
	}
}

void Game::shuffleMstMonsterActionIndex(MstMonsterActionIndex *p) {
	for (uint32_t i = 0; i < p->dataCount; ++i) {
		p->data[i] &= 0x7F;
	}
	shuffleArray(p->data, p->dataCount);
}

void Game::initMstCode() {
	memset(_mstVars, 0, sizeof(_mstVars));
	if (_mstDisabled) {
		return;
	}
	// _mstLut initialization
	resetMstCode();
}

void Game::resetMstCode() {
	if (_mstDisabled) {
		return;
	}
	_mstFlags = 0;
	for (int i = 0; i < kMaxMonsterObjects1; ++i) {
		mstMonster1ResetData(&_monsterObjects1Table[i]);
	}
	for (int i = 0; i < kMaxMonsterObjects2; ++i) {
		mstMonster2ResetData(&_monsterObjects2Table[i]);
	}
	clearLvlObjectsList1();
	for (int i = 0; i < _res->_mstHdr.screenAreaDataCount; ++i) {
		_res->_mstScreenAreaData[i].unk0x1D = 1;
	}
	_rnd.initMstTable();
	_rnd.initTable();
	for (int i = 0; i < _res->_mstHdr.movingBoundsDataCount; ++i) {
		const int count = _res->_mstMovingBoundsData[i].indexDataCount;
		if (count != 0) {
			shuffleArray(_res->_mstMovingBoundsData[i].indexData, count);
		}
	}
	for (int i = 0; i < _res->_mstHdr.walkCodeDataCount; ++i) {
		const int count = _res->_mstWalkCodeData[i].indexDataCount;
		if (count != 0) {
			shuffleArray(_res->_mstWalkCodeData[i].indexData, count);
		}
	}
	for (int i = 0; i < _res->_mstHdr.monsterActionIndexDataCount; ++i) {
		shuffleMstMonsterActionIndex(&_res->_mstMonsterActionIndexData[i]);
	}
	_mstOp67_x1 = -256;
	_mstOp67_x2 = -256;
	memset(_monsterObjects1Table, 0, sizeof(_monsterObjects1Table));
	memset(_monsterObjects2Table, 0, sizeof(_monsterObjects2Table));
	memset(_mstVars, 0, sizeof(_mstVars));
	memset(_tasksTable, 0, sizeof(_tasksTable));
	_m43Num3 = _m43Num1 = _m43Num2 = _mstActionNum = -1;
	_mstOp54Counter = 0; // bugfix: not reset in the original, causes uninitialized reads at the beginning of 'fort'
	_executeMstLogicPrevCounter = _executeMstLogicCounter = 0;
	// _mstUnk8 = 0;
	_specialAnimFlag = false;
	_mstAndyRectNum = 0xFF;
	_mstBoundingBoxesCount = 0;
	_mstOp67_y1 = 0;
	_mstOp67_y2 = 0;
	_mstOp67_screenNum = -1;
	_mstOp68_x1 = 256;
	_mstOp68_x2 = 256;
	_mstOp68_y1 = 0;
	_mstOp68_y2 = 0;
	_mstOp68_screenNum = -1;
	_mstLevelGatesMask = 0;
	// _mstLevelGatesTestMask = 0xFFFFFFFF;
	_mstAndyVarMask = 0;
	_tasksList = 0;
	_monsterObjects1TasksList = 0;
	_monsterObjects2TasksList = 0;
	// _mstTasksList3 = 0;
	// _mstTasksList4 = 0;
	if (_res->_mstTickCodeData != kNone) {
		_mstVars[31] = _mstTickDelay = _res->_mstTickDelay;
	} else {
		_mstVars[31] = -1;
	}
	_mstVars[30] = 0x20;
	for (int i = 0; i < kMaxMonsterObjects1; ++i) {
		_monsterObjects1Table[i].monster1Index = i;
	}
	for (int i = 0; i < kMaxMonsterObjects2; ++i) {
		_monsterObjects2Table[i].monster2Index = i;
	}
	mstUpdateRefPos();
	_mstAndyLevelPrevPosX = _mstAndyLevelPosX;
	_mstAndyLevelPrevPosY = _mstAndyLevelPosY;
}

void Game::startMstCode() {
	mstUpdateRefPos();
	_mstAndyLevelPrevPosX = _mstAndyLevelPosX;
	_mstAndyLevelPrevPosY = _mstAndyLevelPosY;
	int offset = 0;
	for (int i = 0; i < _res->_mstHdr.infoMonster1Count; ++i) {
		offset += kMonsterInfoDataSize;
		const uint32_t unk30 = READ_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x30]); // 900
		const uint32_t unk34 = READ_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x34]); // 896

		const uint32_t unk20 = _mstAndyLevelPosX - unk30;
		const uint32_t unk1C = _mstAndyLevelPosX + unk30;
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x20], unk20);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x1C], unk1C);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x24], unk20 - unk34);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x18], unk1C + unk34);

		const uint32_t unk10 = _mstAndyLevelPosY - unk30;
		const uint32_t unk0C = _mstAndyLevelPosY + unk30;
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x10], unk10);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x0C], unk0C);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x14], unk10 - unk34);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x08], unk0C + unk34);
	}
	if (_level->_checkpoint < _res->_mstHdr.levelCheckpointCodeDataCount) {
		const uint32_t codeData = _res->_mstLevelCheckpointCodeData[_level->_checkpoint];
		if (codeData != kNone) {
			Task *t = createTask(_res->_mstCodeData + codeData * 4);
			if (t) {
				_runTaskOpcodesCount = 0;
				while ((this->*(t->run))(t) == 0);
			}
		}
	}
}

static bool compareOp(int op, int num1, int num2) {
	switch (op) {
	case 0:
		return num1 != num2;
	case 1:
		return num1 == num2;
	case 2:
		return num1 >= num2;
	case 3:
		return num1 > num2;
	case 4:
		return num1 <= num2;
	case 5:
		return num1 < num2;
	case 6:
		return (num1 & num2) == 0;
	case 7:
		return (num1 | num2) == 0;
	case 8:
		return (num1 ^ num2) == 0;
	default:
		error("compareOp unhandled op %d", op);
		break;
	}
	return false;
}

static void arithOp(int op, int *p, int num) {
	switch (op) {
	case 0:
		*p = num;
		break;
	case 1:
		*p += num;
		break;
	case 2:
		*p -= num;
		break;
	case 3:
		*p *= num;
		break;
	case 4:
		if (num != 0) {
			*p /= num;
		}
		break;
	case 5:
		*p <<= num;
		break;
	case 6:
		*p >>= num;
		break;
	case 7:
		*p &= num;
		break;
	case 8:
		*p |= num;
		break;
	case 9:
		*p ^= num;
		break;
	default:
		error("arithOp unhandled op %d", op);
		break;
	}
}

void Game::executeMstCode() {
	_andyActionKeyMaskAnd = 0xFF;
	_andyDirectionKeyMaskAnd = 0xFF;
	_andyActionKeyMaskOr = 0;
	_andyDirectionKeyMaskOr = 0;
	if (_mstDisabled) {
		return;
	}
	++_executeMstLogicCounter;
	if (_mstLevelGatesMask != 0) {
		_mstHelper1Count = 0;
		executeMstCodeHelper1();
		_mstLevelGatesMask = 0;
	}
	for (int i = 0; i < kMaxAndyShoots; ++i) {
		_andyShootsTable[i].shootObjectData = 0;
		_andyShootsTable[i].m = 0;
		_andyShootsTable[i].monsterDistance = 0x1000000;
	}
	mstUpdateMonster1ObjectsPosition();
	if (_mstVars[31] > 0) {
		--_mstVars[31];
		if (_mstVars[31] == 0) {
			uint32_t codeData = _res->_mstTickCodeData;
			assert(codeData != kNone);
			Task *t = createTask(_res->_mstCodeData + codeData * 4);
			if (t) {
				t->state = 1;
			}
		}
	}
	const MstScreenArea *msac;
	while ((msac = _res->findMstCodeForPos(_currentScreen, _mstAndyLevelPosX, _mstAndyLevelPosY)) != 0) {
		debug(kDebug_MONSTER, "trigger for %d,%d", _mstAndyLevelPosX, _mstAndyLevelPosY);
		_res->flagMstCodeForPos(msac->unk0x1C, 0);
		assert(msac->codeData != kNone);
		createTask(_res->_mstCodeData + msac->codeData * 4);
	}
	if (_mstAndyCurrentScreenNum != _currentScreen) {
		_mstAndyCurrentScreenNum = _currentScreen;
	}
	for (Task *t = _tasksList; t; t = t->nextPtr) {
		_runTaskOpcodesCount = 0;
		while ((this->*(t->run))(t) == 0);
	}
	for (int i = 0; i < _andyShootsCount; ++i) {
		AndyShootData *p = &_andyShootsTable[i];
		MonsterObject1 *m = p->m;
		if (!m) {
			continue;
		}
		const int energy = m->localVars[7];
		ShootLvlObjectData *dat = p->shootObjectData;
		if (dat) {
			if (energy > 0) {
				if (_cheats & kCheatOneHitSpecialPowers) {
					m->localVars[7] = 0;
				} else if (p->type == 2) {
					m->localVars[7] -= 4;
					if (m->localVars[7] < 0) {
						m->localVars[7] = 0;
					}
				} else {
					--m->localVars[7];
				}
				dat->unk3 = 0x80;
				dat->xPosShoot = p->clipX;
				dat->yPosShoot = p->clipY;
				dat->o = m->o16;
			} else if (energy == -2) {
				dat->unk3 = 0x80;
				dat->xPosShoot = p->clipX;
				dat->yPosShoot = p->clipY;
			}
			continue;
		}
		if (energy > 0) {
			if (_cheats & kCheatOneHitPlasmaCannon) {
				m->localVars[7] = 0;
			} else {
				m->localVars[7] = energy - 1;
			}
			_plasmaCannonLastIndex1 = p->plasmaCannonPointsCount;
		} else if (energy == -2) {
			_plasmaCannonLastIndex1 = p->plasmaCannonPointsCount;
		} else {
			_plasmaCannonLastIndex1 = 0;
		}
	}
	for (Task *t = _monsterObjects1TasksList; t; t = t->nextPtr) {
		_runTaskOpcodesCount = 0;
		if (mstUpdateTaskMonsterObject1(t) == 0) {
			while ((this->*(t->run))(t) == 0);
		}
	}
	for (Task *t = _monsterObjects2TasksList; t; t = t->nextPtr) {
		_runTaskOpcodesCount = 0;
		if (mstUpdateTaskMonsterObject2(t) == 0) {
			while ((this->*(t->run))(t) == 0);
		}
	}
}

void Game::mstWalkPathUpdateIndex(MstWalkPath *walkPath, int index) {
	uint32_t _walkNodesTable[32];
	int _walkNodesFirstIndex, _walkNodesLastIndex;
	int32_t buffer[64];
	for (uint32_t i = 0; i < walkPath->count; ++i) {
		MstWalkNode *walkNode = &walkPath->data[i];
		memset(buffer, 0xFF, sizeof(buffer));
		memset(walkNode->unk60[index], 0, walkPath->count);
		_walkNodesTable[0] = i;
		_walkNodesLastIndex = 1;
		buffer[i] = 0;
		_walkNodesFirstIndex = 0;
		while (_walkNodesFirstIndex != _walkNodesLastIndex) {
			uint32_t vf = _walkNodesTable[_walkNodesFirstIndex];
			++_walkNodesFirstIndex;
			if (_walkNodesFirstIndex >= 32) {
				_walkNodesFirstIndex = 0;
			}
			const uint32_t indexWalkBox = walkPath->data[vf].walkBox;
			const MstWalkBox *m34 = &_res->_mstWalkBoxData[indexWalkBox];
			for (int j = 0; j < 4; ++j) {
				const uint32_t indexWalkNode = walkPath->data[vf].neighborWalkNode[j];
				if (indexWalkNode == kNone) {
					continue;
				}
				assert(indexWalkNode < walkPath->count);
				uint32_t mask;
				const uint8_t flags = m34->flags[j];
				if (flags & 0x80) {
					mask = _mstAndyVarMask & (1 << (flags & 0x7F));
				} else {
					mask = flags & (1 << index);
				}
				if (mask != 0) {
					continue;
				}
				int delta;
				if (j == 0 || j == 1) {
					delta = m34->right - m34->left + buffer[vf];
				} else {
					delta = m34->bottom - m34->top + buffer[vf];
				}
				if (delta >= buffer[i]) {
					continue;
				}
				if (buffer[indexWalkNode] == -1) {
					_walkNodesTable[_walkNodesLastIndex] = indexWalkNode;
					++_walkNodesLastIndex;
					if (_walkNodesLastIndex >= 32) {
						_walkNodesLastIndex = 0;
					}
				}
				buffer[i] = delta;
				uint8_t value;
				if (vf == i) {
					static const uint8_t directions[] = { 2, 8, 4, 1 };
					value = directions[j];
				} else {
					value = walkNode->unk60[index][vf];
				}
				walkNode->unk60[index][i] = value;
			}
		}
	}
}

int Game::mstWalkPathUpdateWalkNode(MstWalkPath *walkPath, MstWalkNode *walkNode, int num, int index) {
	if (walkNode->coords[num][index] < 0) {
		const uint32_t indexWalkBox = walkNode->walkBox;
		const MstWalkBox *m34 = &_res->_mstWalkBoxData[indexWalkBox];
		uint32_t mask;
		const uint8_t flags = m34->flags[num];
		if (flags & 0x80) {
			mask = _mstAndyVarMask & (1 << (flags & 0x7F));
		} else {
			mask = flags & (1 << index);
		}
		if (mask) {
			switch (num) { // ((int *)&m34)[num]
			case 0:
				walkNode->coords[num][index] = m34->right;
				break;
			case 1:
				walkNode->coords[num][index] = m34->left;
				break;
			case 2:
				walkNode->coords[num][index] = m34->bottom;
				break;
			case 3:
				walkNode->coords[num][index] = m34->top;
				break;
			}
		} else {
			const uint32_t indexWalkNode = walkNode->neighborWalkNode[num];
			assert(indexWalkNode != kNone && indexWalkNode < walkPath->count);
			walkNode->coords[num][index] = mstWalkPathUpdateWalkNode(walkPath, &walkPath->data[indexWalkNode], num, index);
		}
	}
	return walkNode->coords[num][index];
}

void Game::executeMstCodeHelper1() {
	for (int i = 0; i < _res->_mstHdr.walkPathDataCount; ++i) {
		MstWalkPath *walkPath = &_res->_mstWalkPathData[i];
		if (walkPath->mask & _mstLevelGatesMask) {
			++_mstHelper1Count;
			for (uint32_t j = 0; j < walkPath->count; ++j) {
				for (int k = 0; k < 2; ++k) {
					walkPath->data[j].coords[0][k] = -1;
					walkPath->data[j].coords[1][k] = -1;
					walkPath->data[j].coords[2][k] = -1;
					walkPath->data[j].coords[3][k] = -1;
				}
			}
			for (uint32_t j = 0; j < walkPath->count; ++j) {
				MstWalkNode *walkNode = &walkPath->data[j];
				for (int k = 0; k < 4; ++k) {
					mstWalkPathUpdateWalkNode(walkPath, walkNode, k, 0);
					mstWalkPathUpdateWalkNode(walkPath, walkNode, k, 1);
				}
			}
			mstWalkPathUpdateIndex(walkPath, 0);
			mstWalkPathUpdateIndex(walkPath, 1);
		}
	}
	if (_mstHelper1Count != 0) {
		for (int i = 0; i < kMaxMonsterObjects1; ++i) {
			MonsterObject1 *m = &_monsterObjects1Table[i];
			if (!m->m46) {
				continue;
			}
			MstWalkNode *walkNode = m->walkNode;
			const int num = (~m->flagsA5) & 1;
			m->levelPosBounds_x2 = walkNode->coords[0][num] - (int32_t)READ_LE_UINT32(m->monsterInfos + 904);
			m->levelPosBounds_x1 = walkNode->coords[1][num] + (int32_t)READ_LE_UINT32(m->monsterInfos + 904);
			m->levelPosBounds_y2 = walkNode->coords[2][num] - (int32_t)READ_LE_UINT32(m->monsterInfos + 908);
			m->levelPosBounds_y1 = walkNode->coords[3][num] + (int32_t)READ_LE_UINT32(m->monsterInfos + 908);
		}
	}
}

void Game::mstUpdateMonster1ObjectsPosition() {
	mstUpdateRefPos();
	mstUpdateMonstersRect();
	for (Task *t = _monsterObjects1TasksList; t; t = t->nextPtr) {
		mstTaskUpdateScreenPosition(t);
	}
}

void Game::mstLvlObjectSetActionDirection(LvlObject *o, const uint8_t *ptr, uint8_t mask1, uint8_t dirMask2) {
	o->actionKeyMask = ptr[1];
	o->directionKeyMask = mask1 & 15;
	MonsterObject1 *m = (MonsterObject1 *)getLvlObjectDataPtr(o, kObjectDataTypeMonster1);
	if ((mask1 & 0x10) == 0) {
		const int vf = mask1 & 0xE0;
		switch (vf) {
		case 32:
		case 96:
		case 160:
		case 192:
			if (vf == 192) {
				o->directionKeyMask |= (m->facingDirectionMask & ~kDirectionKeyMaskVertical);
			} else {
				o->directionKeyMask |= m->facingDirectionMask;
				if (m->monsterInfos[946] & 2) {
					if (vf == 160 && (_mstLut1[o->directionKeyMask] & 1) != 0) {
						if (m->xDelta >= m->yDelta) {
							o->directionKeyMask &= ~kDirectionKeyMaskVertical;
						} else {
							o->directionKeyMask &= ~kDirectionKeyMaskHorizontal;
						}
					} else {
						if (m->xDelta >= m->yDelta * 2) {
							o->directionKeyMask &= ~kDirectionKeyMaskVertical;
						} else if (m->yDelta >= m->xDelta * 2) {
							o->directionKeyMask &= ~kDirectionKeyMaskHorizontal;
						}
					}
				}
			}
			break;
		case 128:
			o->directionKeyMask |= dirMask2;
			if ((m->monsterInfos[946] & 2) != 0 && (_mstLut1[o->directionKeyMask] & 1) != 0) {
				if (m->xDelta >= m->yDelta) {
					o->directionKeyMask &= ~kDirectionKeyMaskVertical;
				} else {
					o->directionKeyMask &= ~kDirectionKeyMaskHorizontal;
				}
			}
			break;
		case 224:
			o->directionKeyMask |= m->unkF8;
			break;
		default:
			o->directionKeyMask |= dirMask2;
			break;
		}
	}
	o->directionKeyMask &= ptr[2];
	if ((mask1 & 0xE0) == 0x40) {
		o->directionKeyMask ^= kDirectionKeyMaskHorizontal;
	}
}

void Game::mstMonster1UpdateGoalPosition(MonsterObject1 *m) {
	int var1C = 0;
	int var18 = 0;
	debug(kDebug_MONSTER, "mstMonster1UpdateGoalPosition m %p goalScreen %d levelBounds [%d,%d,%d,%d]", m, m->goalScreenNum, m->levelPosBounds_x1, m->levelPosBounds_y1, m->levelPosBounds_x2, m->levelPosBounds_y2);
	if (m->goalScreenNum == 0xFD) {
		int ve, vf, vg, va;
		const MstMovingBounds *m49 = m->m49;
		const int xPos_1 = _mstAndyLevelPosX + m49->unk14 - m->goalDistance_x2;
		const int xPos_2 = _mstAndyLevelPosX - m49->unk14 + m->goalDistance_x2;
		const int yPos_1 = _mstAndyLevelPosY + m49->unk15 - m->goalDistance_y2;
		const int yPos_2 = _mstAndyLevelPosY + m49->unk15 + m->goalDistance_y2;
		if (m->levelPosBounds_x2 > xPos_1 && m->levelPosBounds_x1 < xPos_2 && m->levelPosBounds_y2 > yPos_1 && m->levelPosBounds_y1 < yPos_2) {
			const int xPos_3 = _mstAndyLevelPosX + m49->unk14 + m->goalDistance_x1;
			const int xPos_4 = _mstAndyLevelPosX - m49->unk14 - m->goalDistance_x1;
			const int yPos_3 = _mstAndyLevelPosY + m49->unk15 + m->goalDistance_y1;
			const int yPos_4 = _mstAndyLevelPosY - m49->unk15 - m->goalDistance_y1;
			if (m->levelPosBounds_x2 < xPos_3) {
				if (m->levelPosBounds_x1 > xPos_4 && m->levelPosBounds_y2 < yPos_3 && m->levelPosBounds_y1 > yPos_4) {
					goto l41B2DC;
				}
			}
			var18 = 0;
			ve = 0x40000000;
			if (m->levelPosBounds_x2 >= _mstAndyLevelPosX + m->goalDistance_x1 + m49->unk14) {
				if (m->xMstPos > _mstAndyLevelPosX + m->goalDistance_x2) {
					ve = m->xMstPos - m->goalDistance_x1 - _mstAndyLevelPosX;
					if (ve >= 0 && ve < m49->unk14) {
						ve = 0x40000000;
					} else {
						var18 = 1;
					}
				} else if (m->xMstPos >= _mstAndyLevelPosX + m->goalDistance_x1) {
					ve = -1;
					var18 = 1;
				} else {
					ve = m->goalDistance_x2 - m->xMstPos + _mstAndyLevelPosX;
					if (ve >= 0 && ve < m49->unk14) {
						ve = 0x40000000;
					} else {
						var18 = 1;
					}
				}
			}
			vf = 0x40000000;
			if (m->levelPosBounds_x1 <= _mstAndyLevelPosX - m->goalDistance_x1 - m49->unk14) {
				if (m->xMstPos > _mstAndyLevelPosX - m->goalDistance_x1) {
					vf = m->xMstPos - _mstAndyLevelPosX + m->goalDistance_x2;
					if (vf >= 0 && vf < m49->unk14) {
						vf = 0x40000000;
					} else {
						var18 |= 2;
					}
				} else if (m->xMstPos >= _mstAndyLevelPosX - m->goalDistance_x2) {
					vf = -1;
					var18 |= 2;
				} else {
					vf = _mstAndyLevelPosX - m->goalDistance_x1 - m->xMstPos;
					if (vf >= 0 && vf < m49->unk14) {
						vf = 0x40000000;
					} else {
						var18 |= 2;
					}
				}
			}
			vg = 0x40000000;
			if (m->levelPosBounds_y2 >= _mstAndyLevelPosY + m->goalDistance_y1 + m49->unk15) {
				if (m->yMstPos > _mstAndyLevelPosY + m->goalDistance_y2) {
					vg = m->yMstPos - m->goalDistance_y1 - _mstAndyLevelPosY;
					if (vg >= 0 && vg < m49->unk15) {
						vg = 0x40000000;
					} else {
						var18 |= 4;
					}
				} else if (m->yMstPos >= _mstAndyLevelPosY + m->goalDistance_y1) {
					vg = -1;
					var18 |= 4;
				} else {
					vg = m->goalDistance_y2 - m->yMstPos + _mstAndyLevelPosY;
					if (vg >= 0 && vg < m49->unk15) {
						vg = 0x40000000;
					} else {
						var18 |= 4;
					}
				}
			}
			va = 0x40000000;
			if (m->levelPosBounds_y1 <= _mstAndyLevelPosY - m->goalDistance_y1 - m49->unk15) {
				if (m->yMstPos > _mstAndyLevelPosY - m->goalDistance_y1) {
					va = m->yMstPos - _mstAndyLevelPosY + m->goalDistance_y2;
					if (va >= 0 && va < m49->unk15) {
						va = 0x40000000;
					} else {
						var18 |= 8;
					}
				} else if (m->yMstPos >= _mstAndyLevelPosY - m->goalDistance_y2) {
					va = -1;
					var18 |= 8;
				} else {
					va = _mstAndyLevelPosY - m->goalDistance_y1 - m->yMstPos;
					if (va >= 0 && va < m49->unk15) {
						va = 0x40000000;
					} else {
						var18 |= 8;
					}
				}
			}
			if (var18 == 0) {
				var18 = 15;
			}
		} else {
l41B2DC:
			ve = ABS(m->xMstPos - _mstAndyLevelPosX + m49->unk14 - m->goalDistance_x2);
			vf = ABS(m->xMstPos - _mstAndyLevelPosX - m49->unk14 + m->goalDistance_x2);
			vg = ABS(m->yMstPos - _mstAndyLevelPosY + m49->unk15 - m->goalDistance_y2);
			va = ABS(m->yMstPos - _mstAndyLevelPosY - m49->unk15 + m->goalDistance_y2);
			var18 = 15;
		}
		if (var18 == m->unkE4) {
			var1C = m->unkE5;
		} else {
			switch (var18 & 3) {
			case 0:
				var1C = (vg > va) ? 0x21 : 0x20;
				break;
			case 2:
				if (vf <= va && vf <= vg) {
					var1C = 0x12;
				} else {
					var1C = (vg >= va) ? 0x21 : 0x20;
				}
				break;
			case 3:
				if (ve > vf) {
					if (vf <= va && vf <= vg) {
						var1C = 0x12;
					} else {
						var1C = (vg >= va) ? 0x21 : 0x20;
					}
					break;
				}
				// fall-through
			case 1:
				if (ve <= va && ve <= vg) {
					var1C = 0x02;
				} else {
					var1C = (vg >= va) ? 0x21 : 0x20;
				}
				break;
			}
			m->unkE4 = var18;
			m->unkE5 = var1C;
		}
	} else {
		var1C = 0;
	}
	switch (var1C & 0xF0) {
	case 0x00:
		m->goalPos_x1 = m->goalDistance_x1;
		m->goalPos_x2 = m->goalDistance_x2;
		break;
	case 0x10:
		m->goalPos_x1 = -m->goalDistance_x2;
		m->goalPos_x2 = -m->goalDistance_x1;
		break;
	case 0x20:
		m->goalPos_x1 = -m->goalDistance_x2;
		m->goalPos_x2 =  m->goalDistance_x2;
		break;
	}
	switch (var1C & 0xF) {
	case 0:
		m->goalPos_y1 = m->goalDistance_y1;
		m->goalPos_y2 = m->goalDistance_y2;
		break;
	case 1:
		m->goalPos_y1 = -m->goalDistance_y2;
		m->goalPos_y2 = -m->goalDistance_y1;
		break;
	case 2:
		m->goalPos_y1 = -m->goalDistance_y2;
		m->goalPos_y2 =  m->goalDistance_y2;
		break;
	}
	m->goalPos_x1 += _mstAndyLevelPosX;
	m->goalPos_x2 += _mstAndyLevelPosX;
	m->goalPos_y1 += _mstAndyLevelPosY;
	m->goalPos_y2 += _mstAndyLevelPosY;
	const uint8_t *ptr = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
	if ((ptr[2] & kDirectionKeyMaskVertical) == 0) {
		m->goalDistance_y1 = m->goalPos_y1 = m->goalDistance_y2 = m->goalPos_y2 = m->yMstPos;
	}
	if ((ptr[2] & kDirectionKeyMaskHorizontal) == 0) {
		m->goalDistance_x1 = m->goalPos_x1 = m->goalDistance_x2 = m->goalPos_x2 = m->xMstPos;
	}
	debug(kDebug_MONSTER, "mstMonster1UpdateGoalPosition m %p mask 0x%x goal [%d,%d,%d,%d]", m, var1C, m->goalPos_x1, m->goalPos_y1, m->goalPos_x2, m->goalPos_y2);
}

void Game::mstMonster1MoveTowardsGoal1(MonsterObject1 *m) {
	MstWalkNode *walkNode = m->walkNode;
	const uint8_t *p = m->monsterInfos;
	const uint32_t indexWalkBox = walkNode->walkBox;
	const MstWalkBox *m34 = &_res->_mstWalkBoxData[indexWalkBox];
	const int w = (int32_t)READ_LE_UINT32(p + 904);
	const int h = (int32_t)READ_LE_UINT32(p + 908);
	debug(kDebug_MONSTER, "mstMonster1MoveTowardsGoal1 m %p pos %d,%d [%d,%d,%d,%d]", m, m->xMstPos, m->yMstPos, m34->left, m34->top, m34->right, m34->bottom);
	if (!rect_contains(m34->left - w, m34->top - h, m34->right + w, m34->bottom + h, m->xMstPos, m->yMstPos)) {
		mstMonster1UpdateWalkPath(m);
		m->unkC0 = -1;
		m->unkBC = -1;
	}
	m->task->flags &= ~0x80;
	if (m->xMstPos < m->goalPos_x1) {
		int x = m->goalPos_x2;
		_xMstPos1 = x;
		if (x > m->levelPosBounds_x2) {
			x = m->levelPosBounds_x2;
			m->task->flags |= 0x80;
		}
		m->goalDirectionMask = kDirectionKeyMaskRight;
		_xMstPos2 = x - m->xMstPos;
	} else if (m->xMstPos > m->goalPos_x2) {
		int x = m->goalPos_x1;
		_xMstPos1 = x;
		if (x < m->levelPosBounds_x1) {
			x = m->levelPosBounds_x1;
			m->task->flags |= 0x80;
		}
		m->goalDirectionMask = kDirectionKeyMaskLeft;
		_xMstPos2 = m->xMstPos - x;
	} else {
		_xMstPos1 = m->xMstPos;
		m->goalDirectionMask = 0;
		_xMstPos2 = 0;
	}
	if (m->yMstPos < m->goalPos_y1) {
		int y = m->goalPos_y2;
		_yMstPos1 = y;
		if (y > m->levelPosBounds_y2) {
			y = m->levelPosBounds_y2;
			m->task->flags |= 0x80;
		}
		_yMstPos2 = y - m->yMstPos;
		m->goalDirectionMask |= kDirectionKeyMaskDown;
	} else if (m->yMstPos > m->goalPos_y2) {
		int y = m->goalPos_y1;
		_yMstPos1 = y;
		if (y < m->levelPosBounds_y1) {
			y = m->levelPosBounds_y1;
			m->task->flags |= 0x80;
		}
		_yMstPos2 = m->yMstPos - y;
		m->goalDirectionMask |= kDirectionKeyMaskUp;
	} else {
		_yMstPos1 = m->yMstPos;
		_yMstPos2 = 0;
	}
	debug(kDebug_MONSTER, "mstMonster1MoveTowardsGoal1 m %p pos %d,%d dist %d,%d direction 0x%x", m, _xMstPos1, _yMstPos1, _xMstPos2, _yMstPos2, m->goalDirectionMask);
	if (m->goalDirectionMask == 0) {
		return;
	}
	MstBehaviorState *behaviorState = m->behaviorState;
	MstWalkPath *walkPath = &_res->_mstWalkPathData[behaviorState->walkPath];
	uint8_t _cl = m->walkBoxNum;
	if (m->unkBC != _xMstPos1 || m->unkC0 != _yMstPos1) {
		bool inWalkBox = false;
		if (_cl < walkPath->count) {
			MstWalkNode *walkNode = &walkPath->data[_cl];
			const MstWalkBox *m34 = &_res->_mstWalkBoxData[walkNode->walkBox];
			if (rect_contains(m34->left, m34->top, m34->right, m34->bottom, _xMstPos1, _yMstPos1)) {
				inWalkBox = true;
				
			}
		}
		if (!inWalkBox) {
			const int _al = mstMonster1FindWalkPathRect(m, walkPath, _xMstPos1, _yMstPos1);
			if (_al < 0) {
				_xMstPos1 = _xMstPos3;
				_yMstPos1 = _yMstPos3;
				_cl = -1 - _al;
				m->goalPos_x1 = m->goalPos_x2 = _xMstPos1;
				m->goalPos_y1 = m->goalPos_y2 = _yMstPos1;
				_xMstPos2 = ABS(m->xMstPos - _xMstPos1);
				_yMstPos2 = ABS(m->yMstPos - _yMstPos1);
				if (m->xMstPos < m->goalPos_x1) {
					m->goalDirectionMask = kDirectionKeyMaskRight;
				} else if (m->xMstPos > m->goalPos_x2) {
					m->goalDirectionMask = kDirectionKeyMaskLeft;
				}
				if (m->yMstPos < m->goalPos_y1) {
					m->goalDirectionMask |= kDirectionKeyMaskDown;
				} else if (m->yMstPos > m->goalPos_y2) {
					m->goalDirectionMask |= kDirectionKeyMaskUp;
				}
			} else {
				_cl = _al;
			}
			m->walkBoxNum = _cl;
			m->unkBC = -1;
			m->unkC0 = -1;
		}
	}
	if (_cl >= walkPath->count || m->walkNode == &walkPath->data[_cl]) {
		m->targetDirectionMask = 0xFF;
		return;
	}
	const int vf = (~m->flagsA5) & 1;
	if (m->unkBC == _xMstPos1 && m->unkC0 == _yMstPos1) {
		if (m->targetDirectionMask == 0xFF) {
			return;
		}
		m->goalDirectionMask = m->targetDirectionMask;
	} else {
		m->unkBC = _xMstPos1;
		m->unkC0 = _yMstPos1;
		const uint32_t indexWalkPath = m->behaviorState->walkPath;
		MstWalkPath *walkPath = &_res->_mstWalkPathData[indexWalkPath];
		const MstWalkNode *walkNode = m->walkNode;
		const uint8_t dirMask = walkNode->unk60[vf][_cl];
		if (dirMask != 0) {
			const uint8_t *p = m->monsterInfos;
			const int w = (int32_t)READ_LE_UINT32(p + 904);
			const int h = (int32_t)READ_LE_UINT32(p + 908);
			while (1) {
				const int x1 = walkNode->coords[1][vf] + w; // left
				const int x2 = walkNode->coords[0][vf] - w; // right
				const int y1 = walkNode->coords[3][vf] + h; // top
				const int y2 = walkNode->coords[2][vf] - h; // bottom
				if (!rect_contains(x1, y1, x2, y2, _xMstPos1, _yMstPos1)) {
					break;
				}
				int var1C;
				switch (walkNode->unk60[vf][_cl]) {
				case 2:
					var1C = 0;
					break;
				case 4:
					var1C = 2;
					break;
				case 8:
					var1C = 1;
					break;
				default: // 1
					var1C = 3;
					break;
				}
				const uint32_t indexWalkNode = walkNode->neighborWalkNode[var1C];
				assert(indexWalkNode != kNone && indexWalkNode < walkPath->count);
				walkNode = &walkPath->data[indexWalkNode];
				if (walkNode == &walkPath->data[_cl]) {
					m->targetDirectionMask = 0xFF;
					return;
				}
			}
			m->goalDirectionMask = dirMask;
		}
		m->targetDirectionMask = m->goalDirectionMask;
	}
	if (m->goalDirectionMask & kDirectionKeyMaskLeft) {
		_xMstPos2 = m->xMstPos - m->walkNode->coords[1][vf] - (int32_t)READ_LE_UINT32(m->monsterInfos + 904);
	} else if (m->goalDirectionMask & kDirectionKeyMaskRight) {
		_xMstPos2 = m->walkNode->coords[0][vf] - (int32_t)READ_LE_UINT32(m->monsterInfos + 904) - m->xMstPos;
	} else {
		_xMstPos2 = 0;
	}
	if (m->goalDirectionMask & kDirectionKeyMaskUp) {
		_yMstPos2 = m->yMstPos - m->walkNode->coords[3][vf] - (int32_t)READ_LE_UINT32(m->monsterInfos + 908);
	} else if (m->goalDirectionMask & kDirectionKeyMaskDown) {
		_yMstPos2 = m->walkNode->coords[2][vf] - (int32_t)READ_LE_UINT32(m->monsterInfos + 908) - m->yMstPos;
	} else {
		_yMstPos2 = 0;
	}
	debug(kDebug_MONSTER, "mstMonster1MoveTowardsGoal1 m %p dist %d,%d", m, _xMstPos2, _yMstPos2);
}

bool Game::mstMonster1TestGoalDirection(MonsterObject1 *m) {
	if (_mstLut1[m->goalDirectionMask] & 1) {
		if (_xMstPos2 < m->m49->unk14) {
			m->goalDirectionMask &= ~kDirectionKeyMaskHorizontal;
		}
		if (_yMstPos2 < m->m49->unk15) {
			m->goalDirectionMask &= ~kDirectionKeyMaskVertical;
		}
	}
	if (_mstLut1[m->goalDirectionMask] & 1) {
		while (--m->indexUnk49Unk1 >= 0) {
			m->m49Unk1 = &m->m49->data1[m->indexUnk49Unk1];
			if (_xMstPos2 >= m->m49Unk1->unkE && _yMstPos2 >= m->m49Unk1->unkF) {
				return true;
			}
		}
	} else {
		while (--m->indexUnk49Unk1 >= 0) {
			m->m49Unk1 = &m->m49->data1[m->indexUnk49Unk1];
			if (((m->goalDirectionMask & kDirectionKeyMaskHorizontal) == 0 || _xMstPos2 >= m->m49Unk1->unkC) && ((m->goalDirectionMask & kDirectionKeyMaskVertical) == 0 || _yMstPos2 >= m->m49Unk1->unkD)) {
				return true;
			}
		}
	}
	return false;
}

void Game::mstMonster1MoveTowardsGoal2(MonsterObject1 *m) {
	if (m->targetLevelPos_x != -1) {
		if (m->xMstPos != m->targetLevelPos_x || m->yMstPos != m->targetLevelPos_y) {
			_xMstPos2 = m->previousDxPos;
			_yMstPos2 = m->previousDyPos;
			return;
		}
		mstBoundingBoxClear(m, 1);
	}
	mstMonster1MoveTowardsGoal1(m);
	if (m->goalDirectionMask == 0) {
		return;
	}
	m->previousDxPos = _xMstPos2;
	m->previousDyPos = _yMstPos2;
	const MstMovingBoundsUnk1 *m49Unk1 = m->m49Unk1;
	uint8_t xDist, yDist;
	if (_mstLut1[m->goalDirectionMask] & 1) {
		xDist = m49Unk1->unkA;
		yDist = m49Unk1->unkB;
	} else {
		xDist = m49Unk1->unk8;
		yDist = m49Unk1->unk9;
	}
	if (_xMstPos2 < xDist && _yMstPos2 < yDist) {
		if ((_xMstPos2 <= 0 && _yMstPos2 <= 0) || !mstMonster1TestGoalDirection(m)) {
			return;
		}
	}
	if (_mstLut1[m->goalDirectionMask] & 1) {
		if (_xMstPos2 < _yMstPos2) {
			if (_xMstPos2 < m->m49Unk1->unkA) {
				m->goalDirectionMask &= ~kDirectionKeyMaskHorizontal;
				if (m->targetDirectionMask != 0xFF) {
					_xMstPos2 = 0;
				}
			}
		} else {
			if (_yMstPos2 < m->m49Unk1->unkB) {
				m->goalDirectionMask &= ~kDirectionKeyMaskVertical;
				if (m->targetDirectionMask != 0xFF) {
					_yMstPos2 = 0;
				}
			}
		}
	}
	int var10 = (~m->flagsA5) & 1;
	const uint32_t indexWalkBox = m->walkNode->walkBox;
	const MstWalkBox *m34 = &_res->_mstWalkBoxData[indexWalkBox];
	int bboxIndex = 0;
	int var8 = _mstLut1[m->goalDirectionMask];
	for (int var20 = 0; var20 < 5; ++var20) {
		const uint8_t *p = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
		if (var20 != 0) {
			if (p[0xE] == 0 || bboxIndex == 0) {
				break;
			}
		}
		const int num = var20 + var8 * 5;
		const int dirMask = _mstLut3[num];
		if (_mstLut1[dirMask] == m->unkAB) {
			continue;
		}
		int vc, va;
		if (_mstLut1[dirMask] & 1) {
			vc = (int8_t)p[0xA];
			va = (int8_t)p[0xB];
		} else {
			vc = (int8_t)p[0x8];
			va = (int8_t)p[0x9];
		}
		int xPos = m->xMstPos; // vf
		if (dirMask & kDirectionKeyMaskLeft) {
			xPos -= vc;
			if (xPos < m->levelPosBounds_x1) {
				continue;
			}
		} else if (dirMask & kDirectionKeyMaskRight) {
			xPos += vc;
			if (xPos > m->levelPosBounds_x2) {
				continue;
			}
		}
		int yPos = m->yMstPos; // ve
		if (dirMask & kDirectionKeyMaskUp) {
			yPos -= va;
			if (yPos < m->levelPosBounds_y1) {
				continue;
			}
		} else if (dirMask & kDirectionKeyMaskDown) {
			yPos += va;
			if (yPos > m->levelPosBounds_y2) {
				continue;
			}
		}
		if (var10 == 1 && (m->flagsA5 & 4) == 0 && (m->flags48 & 8) != 0) {
			if (!mstSetCurrentPos(m, xPos, yPos)) {
				continue;
			}
		}
		const int w = (int32_t)READ_LE_UINT32(m->monsterInfos + 904);
		const int h = (int32_t)READ_LE_UINT32(m->monsterInfos + 908);
		if (!rect_contains(m34->left - w, m34->top - h, m34->right + w, m34->bottom + h, xPos, yPos)) {
			const uint32_t indexWalkPath = m->behaviorState->walkPath;
			MstWalkPath *walkPath = &_res->_mstWalkPathData[indexWalkPath];
			const int num = mstMonster1FindWalkPathRect(m, walkPath, xPos, yPos);
			if (num < 0) {
				continue;
			}
			if (m->walkNode->unk60[var10][num] == 0) {
				continue;
			}
		}
		m->targetLevelPos_x = xPos;
		m->targetLevelPos_y = yPos;
		p = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
		if (p[0xE] != 0) {
			bboxIndex = m->monster1Index;
			const int x1 = m->xMstPos + (int8_t)p[0xC];
			const int y1 = m->yMstPos + (int8_t)p[0xD];
			const int x2 = x1 + p[0xE] - 1;
			const int y2 = y1 + p[0xF] - 1;
			const int r = mstBoundingBoxCollides2(m->monster1Index, x1, y1, x2, y2);
			if (r != 0) {
				bboxIndex = r - 1;
				const MstBoundingBox *b = &_mstBoundingBoxesTable[bboxIndex];
				if (rect_intersects(b->x1, b->y1, b->x2, b->y2, m->goalPos_x1, m->goalPos_y1, m->goalPos_x2, m->goalPos_y2)) {
					break;
				}
				continue;
			}
			m->bboxNum[1] = mstBoundingBoxUpdate(m->bboxNum[1], m->monster1Index, x1, y1, x2, y2);
		} else { // (p[0xE] == 0)
			mstBoundingBoxClear(m, 1);
		}
		m->goalDirectionMask = dirMask;
		if (var20 == 0) {
			m->unkAB = 0xFF;
		} else {
			const uint8_t n = _mstLut1[dirMask];
			m->unkAB = (n < 4) ? n + 4 : n - 4;
		}
		return;
	}
	m->task->flags |= 0x80;
	m->goalDirectionMask = 0;
	_yMstPos2 = 0;
	_xMstPos2 = 0;
}

int Game::mstTaskStopMonster1(Task *t, MonsterObject1 *m) {
	if (m->monsterInfos[946] & 4) {
		mstBoundingBoxClear(m, 1);
	}
	if (m->goalScreenNum != 0xFC && (m->flagsA5 & 8) != 0 && (t->flags & 0x20) != 0 && m->action) {
		LvlObject *o = m->o16;
		const int xPosScreen = _res->_mstPointOffsets[_currentScreen].xOffset;
		const int yPosScreen = _res->_mstPointOffsets[_currentScreen].yOffset;
		const int xPosObj = o->xPos + _res->_mstPointOffsets[o->screenNum].xOffset;
		const int yPosObj = o->yPos + _res->_mstPointOffsets[o->screenNum].yOffset;
		// this matches the original code but rect_intersects() could probably be used
		if (xPosObj < xPosScreen || xPosObj + o->width - 1 > xPosScreen + 255 || yPosObj < yPosScreen || yPosObj + o->height - 1 > yPosScreen + 191) {
			return mstTaskStopMonsterObject1(t);
		}
	}
	mstTaskResetMonster1WalkPath(t);
	return 0;
}

int Game::mstTaskUpdatePositionActionDirection(Task *t, MonsterObject1 *m) {
	if ((m->monsterInfos[946] & 4) == 0 && (_mstLut1[m->goalDirectionMask] & 1) != 0) {
		if (_xMstPos2 < m->m49->unk14) {
			m->goalDirectionMask &= ~kDirectionKeyMaskHorizontal;
		}
		if (_yMstPos2 < m->m49->unk15) {
			m->goalDirectionMask &= ~kDirectionKeyMaskVertical;
		}
	}
	const uint8_t *ptr = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
	if ((m->monsterInfos[946] & 4) == 0 && (m->flagsA5 & 4) == 0 && (m->flagsA5 & 2) != 0 && (m->flags48 & 8) != 0) {
		int vf, ve;
		if (_mstLut1[m->goalDirectionMask] & 1) {
			vf = (int8_t)ptr[0xA];
			ve = (int8_t)ptr[0xB];
		} else {
			vf = (int8_t)ptr[0x8];
			ve = (int8_t)ptr[0x9];
		}
		int x = m->xMstPos;
		int y = m->yMstPos;
		if (m->goalDirectionMask & kDirectionKeyMaskLeft) {
			x -= vf;
		} else if (m->goalDirectionMask & kDirectionKeyMaskRight) {
			x += vf;
		}
		if (m->goalDirectionMask & kDirectionKeyMaskUp) {
			y -= ve;
		} else if (m->goalDirectionMask & kDirectionKeyMaskDown) {
			y += ve;
		}
		if (!mstSetCurrentPos(m, x, y)) {
			_xMstPos2 = ABS(m->xMstPos - _mstCurrentPosX);
			_yMstPos2 = ABS(m->yMstPos - _mstCurrentPosY);
		}
	}
	const MstMovingBoundsUnk1 *m49Unk1 = m->m49Unk1;
	uint8_t xDist, yDist;
	if (_mstLut1[m->goalDirectionMask] & 1) {
		xDist = m49Unk1->unkA;
		yDist = m49Unk1->unkB;
	} else {
		xDist = m49Unk1->unk8;
		yDist = m49Unk1->unk9;
	}
	if (m->goalDirectionMask == 0) {
		return mstTaskStopMonster1(t, m);
	}
	if (_xMstPos2 < xDist && _yMstPos2 < yDist) {
		if ((_xMstPos2 <= 0 && _yMstPos2 <= 0) || !mstMonster1TestGoalDirection(m)) {
			return mstTaskStopMonster1(t, m);
		}
	}
	if ((m->monsterInfos[946] & 4) == 0 && (_mstLut1[m->goalDirectionMask] & 1) != 0) {
		if (_xMstPos2 < _yMstPos2) {
			if (_xMstPos2 < m->m49Unk1->unkA) {
				m->goalDirectionMask &= ~kDirectionKeyMaskHorizontal;
				if (m->targetDirectionMask != 0xFF) {
					_xMstPos2 = 0;
				}
			}
		} else {
			if (_yMstPos2 < m->m49Unk1->unkB) {
				m->goalDirectionMask &= ~kDirectionKeyMaskVertical;
				if (m->targetDirectionMask != 0xFF) {
					_yMstPos2 = 0;
				}
			}
		}
	}
	ptr = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
	mstLvlObjectSetActionDirection(m->o16, ptr, ptr[3], m->goalDirectionMask);
	return 1;
}

// ret >0: found a rect matching
// ret <0: return the closest match (setting _xMstPos3 and _yMstPos3)
int Game::mstMonster1FindWalkPathRect(MonsterObject1 *m, MstWalkPath *walkPath, int x, int y) {
	_xMstPos3 = x;
	_yMstPos3 = y;
	const int num = (~m->flagsA5) & 1;
	int minDistance = 0x40000000; // vg
	int ret = -1;
	int currentIndex = -1;
	int xDist = 0;
	int yDist = 0;
	for (uint32_t i = 0; i < walkPath->count; ++i, --currentIndex) {
		MstWalkNode *walkNode = &walkPath->data[i];
		if (m->walkNode->unk60[num][i] == 0 && m->walkNode != walkNode) {
			continue;
		}
		const uint32_t indexWalkBox = walkNode->walkBox;
		const MstWalkBox *m34 = &_res->_mstWalkBoxData[indexWalkBox];
		if (rect_contains(m34->left, m34->top, m34->right, m34->bottom, x, y)) {
			return i;
		}
		int dist, curX = x, curY = y;

		if (x >= m34->left && x <= m34->right) {
			const int dy1 = ABS(y - m34->bottom);
			const int dy2 = ABS(y - m34->top);
			curY = (dy2 >= dy1) ? m34->bottom : m34->top;

			yDist = y - curY;
			yDist *= yDist;

			dist = yDist;
			if (minDistance >= dist) {
				minDistance = dist;
				_xMstPos3 = curX;
				_yMstPos3 = curY;
				ret = currentIndex;
			}
		} else if (y >= m34->top && y <= m34->bottom) {
			const int dx1 = ABS(x - m34->right);
			const int dx2 = ABS(x - m34->left);
			curX = (dx2 >= dx1) ? m34->right : m34->left;

			xDist = x - curX;
			xDist *= xDist;

			dist = xDist;
			if (minDistance >= dist) {
				minDistance = dist;
				_xMstPos3 = curX;
				_yMstPos3 = curY;
				ret = currentIndex;
			}
		} else {
			curX = m34->left;
			xDist = x - curX;
			xDist *= xDist;
			curY = m34->top;
			yDist = y - curY;
			yDist *= yDist;

			dist = xDist + yDist;
			if (minDistance >= dist) {
				minDistance = dist;
				_xMstPos3 = curX;
				_yMstPos3 = curY;
				ret = currentIndex;
			}
			curX = m34->right;
			xDist = x - curX;
			xDist *= xDist;
			dist = xDist + yDist;
			if (minDistance >= dist) {
				minDistance = dist;
				_xMstPos3 = curX;
				_yMstPos3 = curY;
				ret = currentIndex;
			}
			curY = m34->bottom;
			yDist = y - curY;
			yDist *= yDist;
			dist = xDist + yDist;
			if (minDistance >= dist) {
				minDistance = dist;
				_xMstPos3 = curX;
				_yMstPos3 = curY;
				ret = currentIndex;
			}
			curX = m34->left;
			xDist = x - curX;
			xDist *= xDist;
			dist = xDist + yDist;
			if (minDistance >= dist) {
				minDistance = dist;
				_xMstPos3 = curX;
				_yMstPos3 = curY;
				ret = currentIndex;
			}
		}
		dist = xDist + yDist;
		if (minDistance >= dist) {
			minDistance = dist;
			_xMstPos3 = curX;
			_yMstPos3 = curY;
			ret = currentIndex;
		}
	}
	return ret;
}

bool Game::mstTestActionDirection(MonsterObject1 *m, int num) {
	LvlObject *o = m->o16;
	const uint8_t _al = _res->_mstActionDirectionData[num].unk0;
	const uint8_t _bl = _res->_mstActionDirectionData[num].unk2;
	const uint8_t *var4 = m->monsterInfos + _al * 28;
	const uint8_t _dl = (o->flags1 >> 4) & 3;
	uint8_t var8 = ((_dl & 1) != 0) ? 8 : 2;
	if (_dl & 2) {
		var8 |= 4;
	} else {
		var8 |= 1;
	}
	uint8_t directionKeyMask = _bl & 15;
	if ((_bl & 0x10) == 0) {
		const uint32_t ve = _bl & 0xE0;
		switch (ve) {
		case 32:
		case 96:
		case 160:
		case 192: // 0
			if (ve == 192) {
				directionKeyMask |= m->facingDirectionMask & ~kDirectionKeyMaskVertical;
			} else {
				directionKeyMask |= m->facingDirectionMask;
				if (m->monsterInfos[946] & 2) {
					if (ve == 160 && (_mstLut1[directionKeyMask] & 1) != 0) {
						if (m->xDelta >= m->yDelta) {
							directionKeyMask &= ~kDirectionKeyMaskVertical;
						} else {
							directionKeyMask &= ~kDirectionKeyMaskHorizontal;
						}
					} else {
						if (m->xDelta >= 2 * m->yDelta) {
							directionKeyMask &= ~kDirectionKeyMaskVertical;
						} else if (m->yDelta >= 2 * m->xDelta) {
							directionKeyMask &= ~kDirectionKeyMaskHorizontal;
						}
					}
				}
			}
			break;
		case 128: // 1
			directionKeyMask |= var8;
			if ((m->monsterInfos[946] & 2) != 0 && (_mstLut1[directionKeyMask] & 1) != 0) {
				if (m->xDelta >= m->yDelta) {
					directionKeyMask &= ~kDirectionKeyMaskVertical;
				} else {
					directionKeyMask &= ~kDirectionKeyMaskHorizontal;
				}
			}
			break;
		default: // 2
			directionKeyMask |= var8;
			break;
		}
	}
	directionKeyMask &= var4[2];
	if ((_bl & 0xE0) == 0x40) {
		directionKeyMask ^= kDirectionKeyMaskHorizontal;
	}
	return ((var8 & directionKeyMask) != 0) ? 0 : 1;
}

bool Game::lvlObjectCollidesAndy1(LvlObject *o, int flags) const {
	int x1, y1, x2, y2;
	if (flags != 1 && flags != 0x4000) {
		x1 = o->xPos;
		y1 = o->yPos;
		x2 = x1 + o->width  - 1;
		y2 = y1 + o->height - 1;
	} else {
		x1 = o->xPos + o->posTable[0].x;
		x2 = o->xPos + o->posTable[1].x;
		y1 = o->yPos + o->posTable[0].y;
		y2 = o->yPos + o->posTable[1].y;
		if (x1 > x2) {
			SWAP(x1, x2);
		}
		if (y1 > y2) {
			SWAP(y1, y2);
		}
		if (flags == 0x4000 && _andyObject->screenNum != o->screenNum) {
			const int dx = _res->_mstPointOffsets[o->screenNum].xOffset - _res->_mstPointOffsets[_andyObject->screenNum].xOffset;
			x1 += dx;
			x2 += dx;
			const int dy = _res->_mstPointOffsets[o->screenNum].yOffset - _res->_mstPointOffsets[_andyObject->screenNum].yOffset;
			y1 += dy;
			y2 += dy;
		}
	}
	const int x = _andyObject->xPos + _andyObject->width / 2;
	const int y = _andyObject->yPos + _andyObject->height / 2;
	return rect_contains(x1, y1, x2, y2, x, y);
}

bool Game::lvlObjectCollidesAndy2(LvlObject *o, int type) const {
	int x1, y1, x2, y2;
	if (type != 1 && type != 0x1000) {
		x1 = o->xPos; // vg
		y1 = o->yPos; // vf
		x2 = x1 + o->width  - 1; // vb
		y2 = y1 + o->height - 1; // ve
	} else {
		x1 = o->xPos + o->posTable[0].x; // vg
		x2 = o->xPos + o->posTable[1].x; // vb
		y1 = o->yPos + o->posTable[0].y; // vf
		y2 = o->yPos + o->posTable[1].y; // ve
		if (x1 > x2) {
			SWAP(x1, x2);
		}
		if (y1 > y2) {
			SWAP(y1, y2);
		}
		if (type == 0x1000 && _andyObject->screenNum != o->screenNum) {
			const int dx = _res->_mstPointOffsets[_andyObject->screenNum].xOffset - _res->_mstPointOffsets[o->screenNum].xOffset;
			x1 += dx;
			x2 += dx;
			const int dy = _res->_mstPointOffsets[_andyObject->screenNum].yOffset - _res->_mstPointOffsets[o->screenNum].yOffset;
			y1 += dy;
			y2 += dy;
		}
	}
	return rect_intersects(x1, y1, x2, y2, _andyObject->xPos, _andyObject->yPos, _andyObject->xPos + _andyObject->width - 1, _andyObject->yPos + _andyObject->height - 1);
}

bool Game::lvlObjectCollidesAndy3(LvlObject *o, int type) const {
	int x1, y1, x2, y2;
	if (type != 1) {
		x1 = o->xPos; // va
		y1 = o->yPos; // vd
		x2 = o->xPos + o->width  - 1; // vg
		y2 = o->yPos + o->height - 1; // vc
	} else {
		x1 = o->xPos + o->posTable[0].x; // va
		y1 = o->yPos + o->posTable[0].y; // vd
		x2 = o->xPos + o->posTable[1].x; // vg
		y2 = o->yPos + o->posTable[1].y; // vc
		if (x1 > x2) {
			SWAP(x1, x2);
		}
		if (y1 > y2) {
			SWAP(y1, y2);
		}
		const int xPos = _andyObject->xPos + _andyObject->posTable[3].x;
		const int yPos = _andyObject->yPos + _andyObject->posTable[3].y;
		if (rect_contains(x1, y1, x2, y2, xPos, yPos)) {
			return true;
		}
	}
	return false;
}

bool Game::lvlObjectCollidesAndy4(LvlObject *o, int type) const {
	int x1, y1, x2, y2;
	if (type != 1) {
		x1 = o->xPos; // vd
		y1 = o->yPos; // vf
		x2 = o->xPos + o->width  - 1; // vb
		y2 = o->yPos + o->height - 1;
	} else {
		x1 = o->xPos + o->posTable[0].x; // vd
		y1 = o->yPos + o->posTable[0].y; // vf
		x2 = o->xPos + o->posTable[1].x; // vb
		y2 = o->yPos + o->posTable[1].y;
		if (x1 > x2) {
			SWAP(x1, x2);
		}
		if (y1 > y2) {
			SWAP(y1, y2);
		}
		static const uint8_t indexes[] = { 1, 2, 4, 5 };
		for (int i = 0; i < 4; ++i) {
			const int xPos = _andyObject->xPos + _andyObject->posTable[indexes[i]].x;
			const int yPos = _andyObject->yPos + _andyObject->posTable[indexes[i]].y;
			if (rect_contains(x1, y1, x2, y2, xPos, yPos)) {
				return true;
			}
		}
	}
	return false;
}

bool Game::mstCollidesByFlags(MonsterObject1 *m, uint32_t flags) {
	if ((flags & 1) != 0 && (m->o16->flags0 & 0x200) == 0) {
		return false;
	} else if ((flags & 8) != 0 && (m->flags48 & 0x20) == 0) {
		return false;
	} else if ((flags & 0x100) != 0 && (_mstFlags & 0x80000000) != 0) {
		return false;
	} else if ((flags & 0x200) != 0 && (_mstFlags & 0x40000000) != 0) {
		return false;
	} else if ((flags & 0x40) != 0 && (mstGetFacingDirectionMask(m->facingDirectionMask) & 1) != ((m->o16->flags1 >> 4) & 1) && (m->monsterInfos[946] & 4) == 0) {
		return false;
	} else if ((flags & 0x80) != 0 && (mstGetFacingDirectionMask(m->facingDirectionMask) & 1) != ((m->o16->flags1 >> 4) & 1) && (m->monsterInfos[946] & 4) != 0) {
		return false;
	} else if ((flags & 0x400) != 0 && (m->o16->screenNum != _andyObject->screenNum || !lvlObjectCollidesAndy1(m->o16, 0))) {
		return false;
	} else if ((flags & 0x800) != 0 && (m->o16->screenNum != _andyObject->screenNum || !lvlObjectCollidesAndy1(m->o16, 1))) {
		return false;
	} else if ((flags & 0x100000) != 0 && (m->o16->screenNum != _andyObject->screenNum || !lvlObjectCollidesAndy3(m->o16, 0))) {
		return false;
	} else if ((flags & 0x200000) != 0 && (m->o16->screenNum != _andyObject->screenNum || !lvlObjectCollidesAndy3(m->o16, 1))) {
		return false;
	} else if ((flags & 4) != 0 && (m->o16->screenNum != _andyObject->screenNum || !lvlObjectCollidesAndy2(m->o16, 0))) {
		return false;
	} else if ((flags & 2) != 0 && (m->o16->screenNum != _andyObject->screenNum || !lvlObjectCollidesAndy2(m->o16, 1))) {
		return false;
	} else if ((flags & 0x4000) != 0 && !lvlObjectCollidesAndy1(m->o16, 0x4000)) {
		return false;
	} else if ((flags & 0x1000) != 0 && !lvlObjectCollidesAndy2(m->o16, 0x1000)) {
		return false;
	} else if ((flags & 0x20) != 0 && (m->o16->flags0 & 0x100) == 0) {
		return false;
	} else if ((flags & 0x10000) != 0 && (m->o16->screenNum != _andyObject->screenNum || !lvlObjectCollidesAndy4(m->o16, 1))) {
		return false;
	} else if ((flags & 0x20000) != 0 && (m->o16->screenNum != _andyObject->screenNum || !lvlObjectCollidesAndy4(m->o16, 0))) {
		return false;
	} else if ((flags & 0x40000) != 0 && (m->o16->screenNum != _andyObject->screenNum || !clipLvlObjectsBoundingBox(_andyObject, m->o16, 36))) {
		return false;
	} else if ((flags & 0x80000) != 0 && (m->o16->screenNum != _andyObject->screenNum || !clipLvlObjectsBoundingBox(_andyObject, m->o16, 20))) {
		return false;
	}
	return true;
}

bool Game::mstMonster1Collide(MonsterObject1 *m, const uint8_t *p) {
	const uint32_t a = READ_LE_UINT32(p + 0x10);
	debug(kDebug_MONSTER, "mstMonster1Collide mask 0x%x flagsA6 0x%x", a, m->flagsA6);
	if (a == 0 || !mstCollidesByFlags(m, a)) {
		return false;
	}
	if ((a & 0x8000) != 0 && (m->flagsA6 & 4) == 0) {
		Task t;
		memcpy(&t, _mstCurrentTask, sizeof(Task));
		_mstCurrentTask->child = 0;
		const uint32_t codeData = READ_LE_UINT32(p + 0x18);
		assert(codeData != kNone);
		_mstCurrentTask->codeData = _res->_mstCodeData + codeData * 4;
		_mstCurrentTask->run = &Game::mstTask_main;
		_mstCurrentTask->monster1->flagsA6 |= 4;
		Task *currentTask = _mstCurrentTask;
		mstTask_main(_mstCurrentTask);
		_mstCurrentTask = currentTask;
		_mstCurrentTask->monster1->flagsA6 &= ~4;
		t.nextPtr = _mstCurrentTask->nextPtr;
		t.prevPtr = _mstCurrentTask->prevPtr;
		memcpy(_mstCurrentTask, &t, sizeof(Task));
		if (_mstCurrentTask->run == &Game::mstTask_idle && (_mstCurrentTask->monster1->flagsA6 & 2) == 0) {
			_mstCurrentTask->run = &Game::mstTask_main;
		}
		return false;
	} else {
		mstTaskAttack(_mstCurrentTask, READ_LE_UINT32(p + 0x18), 0x10);
		return true;
	}
}

int Game::mstUpdateTaskMonsterObject1(Task *t) {
	debug(kDebug_MONSTER, "mstUpdateTaskMonsterObject1 t %p", t);
	_mstCurrentTask = t;
	MonsterObject1 *m = t->monster1;
	MonsterObject1 *_mstCurrentMonster1 = m;
	LvlObject *o = m->o16;
	const int num = o->flags0 & 0xFF;
	const uint8_t *ptr = m->monsterInfos + num * 28; // vb
	int8_t a = ptr[6];
	if (a != 0) {
		const int num = CLIP(m->lut4Index + a, 0, 17);
		o->flags2 = (o->flags2 & ~0x1F) | _mstLut4[num];
	} else {
		o->flags2 = m->o_flags2;
	}
	if (num == 0x1F) {
		mstRemoveMonsterObject1(_mstCurrentTask, &_monsterObjects1TasksList);
		return 1;
	}
	const uint32_t vf = READ_LE_UINT32(ptr + 20);
	if (vf != kNone) {
		MstBehavior *m46 = _mstCurrentMonster1->m46;
		for (uint32_t i = 0; i < m46->count; ++i) {
			if (m46->data[i].indexMonsterInfo == vf) {
				mstTaskSetMonster1BehaviorState(_mstCurrentTask, _mstCurrentMonster1, i);
				return 0;
			}
		}
	}
	if ((m->flagsA5 & 0x80) != 0) {
		return 0;
	}
	if (m->localVars[7] == 0 && !_specialAnimFlag) {
		// monster is dead
		m->flagsA5 |= 0x80;
		if (m->monsterInfos[946] & 4) {
			mstBoundingBoxClear(m, 1);
		}
		if (t->child) {
			t->child->codeData = 0;
			t->child = 0;
		}
		if ((m->flagsA5 & 8) != 0 && m->action && _mstActionNum != -1) {
			mstTaskStopMonsterObject1(_mstCurrentTask);
			return 0;
		}
		const uint32_t codeData = m->behaviorState->codeData;
		if (codeData != kNone) {
			resetTask(t, _res->_mstCodeData + codeData * 4);
			return 0;
		}
		o->actionKeyMask = 7;
		o->directionKeyMask = 0;
		t->run = &Game::mstTask_idle;
		return 0;
	}
	if (t->run == &Game::mstTask_monsterWait4) {
		return 0;
	}
	if (ptr[0] != 0) {
		mstMonster1Collide(_mstCurrentMonster1, ptr);
		return 0;
	}
	if ((m->flagsA5 & 0x40) != 0) {
		return 0;
	}
	assert(_mstCurrentMonster1 == m);
	int dir = 0;
	for (int i = 0; i < _andyShootsCount; ++i) {
		AndyShootData *p = &_andyShootsTable[i];
		if (p->m && p->m != m) {
			continue;
		}
		if (m->collideDistance < 0) {
			continue;
		}
		if ((m->flags48 & 4) == 0) {
			continue;
		}
		if (m->behaviorState->indexUnk51 == kNone) {
			continue;
		}
		if (_rnd.getNextNumber() > m->behaviorState->unk10) {
			continue;
		}
		m->shootActionIndex = 8;
		int var28 = 0;
		AndyShootData *var14 = m->shootData;
		if (t->run != &Game::mstTask_monsterWait5 && t->run != &Game::mstTask_monsterWait6 && t->run != &Game::mstTask_monsterWait7 && t->run != &Game::mstTask_monsterWait8 && t->run != &Game::mstTask_monsterWait9 && t->run != &Game::mstTask_monsterWait10) {
			if (m->monsterInfos[946] & 2) {
				_mstCurrentMonster1->goalPos_x1 = _mstCurrentMonster1->goalPos_y1 = INT32_MIN;
				_mstCurrentMonster1->goalPos_x2 = _mstCurrentMonster1->goalPos_y2 = INT32_MAX;
				switch (var14->directionMask) {
				case 0:
					var28 = 2;
					break;
				case 1:
				case 133:
					var28 = 9;
					break;
				case 2:
					var28 = 12;
					break;
				case 3:
				case 128:
					var28 = 3;
					break;
				case 4:
					var28 = 6;
					break;
				case 5:
					var28 = 8;
					break;
				case 6:
					var28 = 1;
					break;
				case 7:
					var28 = 4;
					break;
				}
				_mstCurrentMonster1->shootActionIndex = _mstLut1[var28];
			}
		} else {
			m->shootActionIndex = _mstLut1[m->goalDirectionMask];
			var28 = 0;
		}
		uint32_t var24 = 0;
		MstBehaviorState *vg = m->behaviorState;
		if (vg->count != 0) {
			var24 = _rnd.update() % (vg->count + 1);
		}
		int var20 = -1;
		int indexUnk51;
		if ((m->flagsA5 & 8) != 0 && m->action && m->action->indexUnk51 != kNone && m->monsterInfos == &_res->_mstMonsterInfos[m->action->unk0 * 948]) {
			indexUnk51 = m->action->indexUnk51;
		} else {
			indexUnk51 = vg->indexUnk51;
		}
		int vb = -1;
		const MstShootIndex *var18 = &_res->_mstShootIndexData[indexUnk51]; // vg
		for (; var24 < var18->count; ++var24) {
			assert(m->shootActionIndex >= 0 && m->shootActionIndex < 9);
			const uint32_t indexUnk50Unk1 = var18->indexUnk50Unk1[var24 * 9 + m->shootActionIndex];
			const MstShootAction *m50Unk1 = &_res->_mstShootData[var18->indexUnk50].data[indexUnk50Unk1];
			const int32_t hSize = m50Unk1->hSize;
			const int32_t vSize = m50Unk1->vSize;
			if (hSize != 0 || vSize != 0) {
				int dirMask = 0;
				int ve = 0;
				if ((m50Unk1->unk4 & kDirectionKeyMaskHorizontal) != kDirectionKeyMaskHorizontal && (m->o16->flags1 & 0x10) != 0) {
					_mstTemp_x1 = m->xMstPos - hSize;
				} else {
					_mstTemp_x1 = m->xMstPos + hSize;
				}
				if (_mstTemp_x1 < m->xMstPos) {
					if (_mstTemp_x1 < m->goalPos_x1) {
						dirMask = kDirectionKeyMaskLeft;
					}
					ve = kDirectionKeyMaskLeft;
				} else if (_mstTemp_x1 > m->xMstPos) {
					if (_mstTemp_x1 > m->goalPos_x2) {
						dirMask = kDirectionKeyMaskRight;
					}
					ve = kDirectionKeyMaskRight;
				} else {
					ve = 0;
				}
				_mstTemp_y1 = m->yMstPos + vSize;
				if (_mstTemp_y1 < m->yMstPos) {
					if (_mstTemp_y1 < m->goalPos_y1 && (m->monsterInfos[946] & 2) != 0) {
						dirMask |= kDirectionKeyMaskUp;
					}
					ve |= kDirectionKeyMaskUp;
				} else if (_mstTemp_y1 > m->yMstPos) {
					if (_mstTemp_y1 > m->goalPos_y2 && (m->monsterInfos[946] & 2) != 0) {
						dirMask |= kDirectionKeyMaskDown;
					}
					ve |= kDirectionKeyMaskDown;
				}
				if (var28 == dirMask) {
					continue;
				}
				if (mstMonster1CheckLevelBounds(m, _mstTemp_x1, _mstTemp_y1, dirMask)) {
					continue;
				}
			}
			_mstTemp_x1 = m->xMstPos;
			if ((m50Unk1->unk4 & kDirectionKeyMaskHorizontal) != kDirectionKeyMaskHorizontal && (m->o16->flags1 & 0x10) != 0) {
				_mstTemp_x1 -= m50Unk1->width;
				_mstTemp_x1 -= m50Unk1->xPos;
			} else {
				_mstTemp_x1 += m50Unk1->xPos;
			}
			_mstTemp_y1 = m->yMstPos + m50Unk1->yPos;
			_mstTemp_x2 = _mstTemp_x1 + m50Unk1->width - 1;
			_mstTemp_y2 = _mstTemp_y1 + m50Unk1->height - 1;
			if ((m->monsterInfos[946] & 4) != 0 && mstBoundingBoxCollides1(m->monster1Index, _mstTemp_x1, _mstTemp_y1, _mstTemp_x2, _mstTemp_y2) != 0) {
				continue;
			}
			if (m50Unk1->width != 0 && getMstDistance((m->monsterInfos[946] & 2) != 0 ? var14->boundingBox.y2 : m->yMstPos, var14) >= 0) {
				continue;
			}
			if (m->collideDistance >= m50Unk1->unk24) {
				MstBehaviorState *behaviorState = m->behaviorState;
				vb = var24;
				int vf = m50Unk1->unk24;
				if (behaviorState->unk18 != 0) {
					vf += (_rnd.update() % (behaviorState->unk18 * 2 + 1)) - behaviorState->unk18;
					if (vf < 0) {
						vf = 0;
					}
				}
				dir = vf;
				break;
			}
			if (var20 == -1) {
				dir = m50Unk1->unk24;
				var20 = var24;
			}
			vb = var20;
		}
		if (vb >= 0) {
			const uint32_t indexUnk50Unk1 = var18->indexUnk50Unk1[vb * 9 + m->shootActionIndex];
			MstShootAction *m50Unk1 = &_res->_mstShootData[var18->indexUnk50].data[indexUnk50Unk1];
			mstTaskAttack(_mstCurrentTask, m50Unk1->codeData, 0x40);
			_mstCurrentMonster1->unkF8 = m50Unk1->unk8;
			_mstCurrentMonster1->shootSource = dir;
			_mstCurrentMonster1->shootDirection = var14->directionMask;
			_mstCurrentMonster1->directionKeyMask = _andyObject->directionKeyMask;
			return 0;
		}
	}
	if (o->screenNum == _currentScreen && (m->flagsA5 & 0x20) == 0 && (m->flags48 & 0x10) != 0) {
		MstBehaviorState *behaviorState = m->behaviorState;
		if (behaviorState->attackBox != kNone) {
			const MstAttackBox *m47 = &_res->_mstAttackBoxData[behaviorState->attackBox];
			if (m47->count > 0) {
				const uint8_t dir = (o->flags1 >> 4) & 3;
				const uint8_t *p = m47->data;
				for (uint32_t i = 0; i < m47->count; ++i) {
					int32_t a = READ_LE_UINT32(p); // x1
					int32_t b = READ_LE_UINT32(p + 4); // y1
					int32_t c = READ_LE_UINT32(p + 8); // x2
					int32_t d = READ_LE_UINT32(p + 12); // y2
					int x1, x2, y1, y2;
					switch (dir) {
					case 1:
						x1 = m->xMstPos - c; // vd
						x2 = m->xMstPos - a; // vf
						y1 = m->yMstPos + b; // vg
						y2 = m->yMstPos + d; // ve;
						break;
					case 2:
						x1 = m->xMstPos + a; // vd
						x2 = m->xMstPos + c; // vf
						y1 = m->yMstPos - d; // vg
						y2 = m->yMstPos - b; // ve
						break;
					case 3:
						x1 = m->xMstPos - c;
						x2 = m->xMstPos - a;
						y1 = m->yMstPos - d;
						y2 = m->yMstPos - b;
						break;
					default:
						x1 = m->xMstPos + a; // vd
						x2 = m->xMstPos + c; // vf
						y1 = m->yMstPos + b; // vg
						y2 = m->yMstPos + d; // ve
						break;
					}

					if (rect_contains(x1, y1, x2, y2, _mstAndyLevelPosX, _mstAndyLevelPosY)) {
						mstTaskAttack(_mstCurrentTask, READ_LE_UINT32(p + 16), 0x20);
						mstMonster1Collide(_mstCurrentMonster1, ptr);
						return 0;
					}

					p += 20;
				}
			}
		}
	}
	if (mstMonster1Collide(_mstCurrentMonster1, ptr)) {
		return 0;
	}
	uint8_t _al = _mstCurrentMonster1->flagsA6;
	if (_al & 2) {
		return 0;
	}
	uint8_t _dl = _mstCurrentMonster1->flagsA5;
	if (_dl & 0x30) {
		return 0;
	}
	dir = _dl & 3;
	if (dir == 1) {
		MstWalkNode *walkNode = _mstCurrentMonster1->walkNode;
		if (walkNode->walkCodeStage2 == kNone) {
			return 0;
		}
		int ve = 0;
		if (_mstAndyLevelPosY >= m->yMstPos - walkNode->y1 && _mstAndyLevelPosY < m->yMstPos + walkNode->y2) {
			if (walkNode->x1 != -2 || walkNode->x1 != walkNode->x2) {
				if (m->xDelta <= walkNode->x1) {
					if (_al & 1) {
						ve = 1;
					} else {
						_al = mstGetFacingDirectionMask(_mstCurrentMonster1->facingDirectionMask) & 1;
						_dl = (_mstCurrentMonster1->o16->flags1 >> 4) & 1;
						if (_dl == _al || (_mstCurrentMonster1->monsterInfos[946] & 4) != 0) {
							ve = 1;
						} else if (m->xDelta <= walkNode->x2) {
							ve = 2;
						}
					}
				}
			} else if (o->screenNum == _currentScreen) {
				if (_al & 1) {
					ve = 1;
				} else {
					_al = mstGetFacingDirectionMask(_mstCurrentMonster1->facingDirectionMask) & 1;
					_dl = (_mstCurrentMonster1->o16->flags1 >> 4) & 1;
					if (_al != _dl && (_mstCurrentMonster1->monsterInfos[946] & 4) == 0) {
						ve = 2;
					} else {
						ve = 1;
					}
				}
			}
		}
		if (ve == 0) {
			m->flagsA6 &= ~1;
			if ((m->flagsA5 & 4) == 0) {
				const uint32_t indexWalkCode = m->walkNode->walkCodeReset[0];
				m->walkCode = &_res->_mstWalkCodeData[indexWalkCode];
			}
			return 0;
		} else if (ve == 1) {
			m->flagsA6 |= 1;
			assert(_mstCurrentMonster1 == m);
			if (!mstSetCurrentPos(m, m->xMstPos, m->yMstPos) && (m->monsterInfos[946] & 2) == 0) {
				if ((_mstCurrentPosX > m->xMstPos && _mstCurrentPosX > m->walkNode->coords[0][1]) || (_mstCurrentPosX < m->xMstPos && _mstCurrentPosX < m->walkNode->coords[1][1])) {
					uint32_t indexWalkCode = m->walkNode->walkCodeStage1;
					if (indexWalkCode != kNone) {
						m->walkCode = &_res->_mstWalkCodeData[indexWalkCode];
					}
					if (m->flagsA5 & 4) {
						m->flagsA5 &= ~4;
						if (!mstMonster1UpdateWalkPath(m)) {
							mstMonster1ResetWalkPath(m);
						}
						indexWalkCode = m->walkNode->walkCodeStage1;
						if (indexWalkCode != kNone) {
							m->walkCode = &_res->_mstWalkCodeData[indexWalkCode];
						}
						mstTaskSetNextWalkCode(_mstCurrentTask);
					}
					return 0;
				}
			}
			if ((m->monsterInfos[946] & 2) == 0) {
				MstWalkNode *walkPath = m->walkNode;
				int vf = (int32_t)READ_LE_UINT32(m->monsterInfos + 904);
				int vb = MAX(m->goalPosBounds_x1, walkPath->coords[1][1] + vf);
				int va = MIN(m->goalPosBounds_x2, walkPath->coords[0][1] - vf);
				const uint32_t indexUnk36 = walkPath->movingBoundsIndex2;
				const uint32_t indexUnk49 = _res->_mstMovingBoundsIndexData[indexUnk36].indexUnk49;
				uint8_t _bl = _res->_mstMovingBoundsData[indexUnk49].unk14;
				if (ABS(va - vb) <= _bl) {
					uint32_t indexWalkCode = walkPath->walkCodeStage1;
					if (indexWalkCode != kNone) {
						m->walkCode = &_res->_mstWalkCodeData[indexWalkCode];
					}
					if (m->flagsA5 & 4) {
						m->flagsA5 &= ~4;
						if (!mstMonster1UpdateWalkPath(m)) {
							mstMonster1ResetWalkPath(m);
						}
						indexWalkCode = m->walkNode->walkCodeStage1;
						if (indexWalkCode != kNone) {
							m->walkCode = &_res->_mstWalkCodeData[indexWalkCode];
						}
						mstTaskSetNextWalkCode(_mstCurrentTask);
					}
					return 0;
				}
			}
			mstTaskInitMonster1Type2(_mstCurrentTask, 0);
			return 0;
		}
		assert(ve == 2);
		if (m->flagsA6 & 1) {
			return 0;
		}
		const uint32_t indexWalkCode = m->walkNode->walkCodeStage2;
		MstWalkCode *m35 = &_res->_mstWalkCodeData[indexWalkCode];
		if (m->walkCode != m35) {
			_mstCurrentMonster1->walkCode = m35;
			_rnd.resetMst(_mstCurrentMonster1->rnd_m35);
			mstTaskSetNextWalkCode(_mstCurrentTask);
			return 0;
		}
		if (m->flagsA5 & 4) {
			m->flagsA5 &= ~4;
			if (!mstMonster1UpdateWalkPath(_mstCurrentMonster1)) {
				mstMonster1ResetWalkPath(_mstCurrentMonster1);
			}
			const uint32_t indexWalkCode = m->walkNode->walkCodeStage2;
			_mstCurrentMonster1->walkCode = &_res->_mstWalkCodeData[indexWalkCode];
			mstTaskSetNextWalkCode(_mstCurrentTask);
		}
		return 0;
	} else if (dir != 2) {
		return 0;
	}
	if ((m->flagsA5 & 4) != 0 || (m->flags48 & 8) == 0) {
		return 0;
	}
	if ((m->flagsA5 & 8) == 0 && (m->monsterInfos[946] & 2) == 0) {
		const uint8_t _dl = m->facingDirectionMask;
		if (_dl & kDirectionKeyMaskRight) {
			if ((int32_t)READ_LE_UINT32(m->monsterInfos + 916) <= m->walkNode->coords[1][1] || (int32_t)READ_LE_UINT32(m->monsterInfos + 912) >= m->walkNode->coords[0][1]) {
				m->flagsA6 |= 1;
				assert(m == _mstCurrentMonster1);
				m->flagsA5 = 1;
				mstMonster1ResetWalkPath(m);
				const uint32_t indexWalkCode = m->walkNode->walkCodeStage1;
				if (indexWalkCode != kNone) {
					m->walkCode = &_res->_mstWalkCodeData[indexWalkCode];
				}
				return 0;
			}
		} else if (_dl & kDirectionKeyMaskLeft) {
			if ((int32_t)READ_LE_UINT32(m->monsterInfos + 920) >= m->walkNode->coords[0][1] || (int32_t)READ_LE_UINT32(m->monsterInfos + 924) <= m->walkNode->coords[1][1]) {
				m->flagsA6 |= 1;
				assert(m == _mstCurrentMonster1);
				m->flagsA5 = 1;
				mstMonster1ResetWalkPath(m);
				const uint32_t indexWalkCode = m->walkNode->walkCodeStage1;
				if (indexWalkCode != kNone) {
					m->walkCode = &_res->_mstWalkCodeData[indexWalkCode];
				}
				return 0;
			}
		}
	}
	if (!mstSetCurrentPos(m, m->xMstPos, m->yMstPos)) {
		mstTaskInitMonster1Type2(t, 1);
	}
	return 0;
}

int Game::mstUpdateTaskMonsterObject2(Task *t) {
	debug(kDebug_MONSTER, "mstUpdateTaskMonsterObject2 t %p", t);
	mstTaskSetMonster2ScreenPosition(t);
	MonsterObject2 *m = t->monster2;
	if (_currentLevel == kLvl_fort && m->monster2Info->type == 27) {
		if (_fireflyPosData[m->hPosIndex] == 0xFF) {
			uint32_t r = _rnd.update();
			uint8_t _dl = (r % 5) << 3;
			m->vPosIndex = _dl;
			if (m->hPosIndex >= 40) {
				m->hPosIndex = _dl;
				m->vPosIndex = _dl + 40;
				m->hDir = (r >> 16) & 1;
			} else {
				m->hPosIndex = m->vPosIndex + 40;
				m->vDir = (r >> 16) & 1;
			}
		}
		int dx = _fireflyPosData[m->hPosIndex];
		if (m->hDir == 0) {
			dx = -dx;
		}
		int dy = _fireflyPosData[m->vPosIndex];
		if (m->vDir == 0) {
			dy = -dy;
		}
		++m->vPosIndex;
		++m->hPosIndex;
		m->o->xPos += dx;
		m->o->yPos += dy;
		m->xMstPos += dx;
		m->yMstPos += dy;
		if (m->xMstPos > m->x2) {
			m->hDir = 0;
		} else if (m->xMstPos < m->x1) {
			m->hDir = 1;
		}
		if (m->yMstPos > m->y2) {
			m->vDir = 0;
		} else if (m->yMstPos < m->y1) {
			m->vDir = 1;
		}
	}
	uint8_t _dl = 0;
	for (int i = 0; i < _andyShootsCount; ++i) {
		AndyShootData *p = &_andyShootsTable[i];
		if (p->type == 2) {
			_dl |= 1;
		} else if (p->type == 1) {
			_dl |= 2;
		}
	}
	LvlObject *o = m->o;
	MstInfoMonster2 *monster2Info = m->monster2Info;
	const uint8_t _bl = monster2Info->shootMask;
	if ((_bl & _dl) != 0) {
		for (int i = 0; i < _andyShootsCount; ++i) {
			AndyShootData *p = &_andyShootsTable[i];
			if (p->type == 2 && (_bl & 1) == 0) {
				continue;
			} else if (p->type == 1 && (_bl & 2) == 0) {
				continue;
			}
			if (o->screenNum != _currentScreen || p->o->screenNum != _currentScreen) {
				continue;
			}
			if (!clipLvlObjectsBoundingBox(p->o, o, 20)) {
				continue;
			}
			ShootLvlObjectData *s = p->shootObjectData;
			s->unk3 = 0x80;
			s->xPosShoot = o->xPos + o->width / 2;
			s->yPosShoot = o->yPos + o->height / 2;
			if (p->type != 2 || (_bl & 4) != 0) {
				continue;
			}
			const uint32_t codeData = monster2Info->codeData2;
			if (codeData != kNone) {
				resetTask(t, _res->_mstCodeData + codeData * 4);
			} else {
				o->actionKeyMask = 7;
				o->directionKeyMask = 0;
				t->run = &Game::mstTask_idle;
			}
		}
	}
	if ((m->o->flags0 & 0xFF) == 0x1F) {
		mstRemoveMonsterObject2(t, &_monsterObjects2TasksList);
		return 1;
	}
	return 0;
}

void Game::mstUpdateRefPos() {
	if (_andyObject) {
		_mstAndyScreenPosX = _andyObject->xPos;
		_mstAndyScreenPosY = _andyObject->yPos;
		_mstAndyLevelPosX = _mstAndyScreenPosX + _res->_mstPointOffsets[_currentScreen].xOffset;
		_mstAndyLevelPosY = _mstAndyScreenPosY + _res->_mstPointOffsets[_currentScreen].yOffset;
		if (!_specialAnimFlag) {
			_mstAndyRectNum = mstBoundingBoxUpdate(_mstAndyRectNum, 0xFE, _mstAndyLevelPosX, _mstAndyLevelPosY, _mstAndyLevelPosX + _andyObject->width - 1, _mstAndyLevelPosY + _andyObject->height - 1) & 0xFF;
		}
		_mstAndyScreenPosX += _andyObject->posTable[3].x;
		_mstAndyScreenPosY += _andyObject->posTable[3].y;
		_mstAndyLevelPosX += _andyObject->posTable[3].x;
		_mstAndyLevelPosY += _andyObject->posTable[3].y;
	} else {
		_mstAndyScreenPosX = 128;
		_mstAndyScreenPosY = 96;
		_mstAndyLevelPosX = _mstAndyScreenPosX + _res->_mstPointOffsets[0].xOffset;
		_mstAndyLevelPosY = _mstAndyScreenPosY + _res->_mstPointOffsets[0].yOffset;
        }
	_andyShootsCount = 0;
	_andyShootsTable[0].type = 0;
	if (!_lvlObjectsList0) {
		if (_plasmaCannonDirection == 0) {
			_executeMstLogicPrevCounter = _executeMstLogicCounter;
			return;
		}
		_andyShootsTable[0].width  = 512;
		_andyShootsTable[0].height = 512;
		_andyShootsTable[0].shootObjectData = 0;
		_andyShootsTable[0].type = 3;
		_andyShootsTable[0].size = 4;
		_andyShootsTable[0].xPos = _plasmaCannonPosX[_plasmaCannonFirstIndex] + _res->_mstPointOffsets[_currentScreen].xOffset;
		_andyShootsTable[0].yPos = _plasmaCannonPosY[_plasmaCannonFirstIndex] + _res->_mstPointOffsets[_currentScreen].yOffset;
		switch (_plasmaCannonDirection - 1) {
		case 0:
			_andyShootsTable[0].directionMask = 6;
			_andyShootsCount = 1;
			break;
		case 2:
			_andyShootsTable[0].directionMask = 3;
			_andyShootsCount = 1;
			break;
		case 1:
			_andyShootsTable[0].directionMask = 0;
			_andyShootsCount = 1;
			break;
		case 5:
			_andyShootsTable[0].directionMask = 4;
			_andyShootsCount = 1;
			break;
		case 3:
			_andyShootsTable[0].directionMask = 7;
			_andyShootsCount = 1;
			break;
		case 11:
			_andyShootsTable[0].directionMask = 2;
			_andyShootsCount = 1;
			break;
		case 7:
			_andyShootsTable[0].directionMask = 5;
			_andyShootsCount = 1;
			break;
		case 8:
			_andyShootsTable[0].directionMask = 1;
			_andyShootsCount = 1;
			break;
		default:
			_andyShootsCount = 1;
			break;
		}
	} else {
		AndyShootData *p = _andyShootsTable;
		for (LvlObject *o = _lvlObjectsList0; o; o = o->nextPtr) {
			p->o = o;
			assert(o->dataPtr);
			ShootLvlObjectData *ptr = (ShootLvlObjectData *)getLvlObjectDataPtr(o, kObjectDataTypeShoot);
			p->shootObjectData = ptr;
			if (ptr->unk3 == 0x80) {
				continue;
			}
			if (ptr->dxPos == 0 && ptr->dyPos == 0) {
				continue;
			}
			p->width  = ptr->dxPos;
			p->height = ptr->dyPos;
			p->directionMask = ptr->state;
			switch (ptr->type) {
			case 0:
				p->type = 1;
				p->xPos = o->xPos + _res->_mstPointOffsets[o->screenNum].xOffset + o->posTable[7].x;
				p->size = 3;
				p->yPos = o->yPos + _res->_mstPointOffsets[o->screenNum].yOffset + o->posTable[7].y;
				break;
			case 5:
				p->directionMask |= 0x80;
				// fall-through
			case 4:
				p->type = 2;
				p->xPos = o->xPos + _res->_mstPointOffsets[o->screenNum].xOffset + o->posTable[7].x;
				p->size = 7;
				p->yPos = o->yPos + _res->_mstPointOffsets[o->screenNum].yOffset + o->posTable[7].y;
				break;
			default:
				--p;
				--_andyShootsCount;
				break;
			}
			++p;
			++_andyShootsCount;
			if (_andyShootsCount >= kMaxAndyShoots) {
				break;
			}
		}
		if (_andyShootsCount == 0) {
			_executeMstLogicPrevCounter = _executeMstLogicCounter;
			return;
		}
	}
	for (int i = 0; i < _andyShootsCount; ++i) {
		AndyShootData *p = &_andyShootsTable[i];
		p->boundingBox.x2 = p->xPos + p->size;
		p->boundingBox.x1 = p->xPos - p->size;
		p->boundingBox.y2 = p->yPos + p->size;
		p->boundingBox.y1 = p->yPos - p->size;
	}
}

void Game::mstUpdateMonstersRect() {
	const int _mstAndyLevelPosDx = _mstAndyLevelPosX - _mstAndyLevelPrevPosX;
	const int _mstAndyLevelPosDy = _mstAndyLevelPosY - _mstAndyLevelPrevPosY;
	_mstAndyLevelPrevPosX = _mstAndyLevelPosX;
	_mstAndyLevelPrevPosY = _mstAndyLevelPosY;
	if (_mstAndyLevelPosDx == 0 && _mstAndyLevelPosDy == 0) {
		return;
	}
	int offset = 0;
	for (int i = 0; i < _res->_mstHdr.infoMonster1Count; ++i) {
		offset += kMonsterInfoDataSize;
		const uint32_t unk30 = READ_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x30]); // 900
		const uint32_t unk34 = READ_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x34]); // 896

		const uint32_t unk20 = _mstAndyLevelPosX - unk30;
		const uint32_t unk1C = _mstAndyLevelPosX + unk30;
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x20], unk20);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x1C], unk1C);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x24], unk20 - unk34);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x18], unk1C + unk34);

		const uint32_t unk10 = _mstAndyLevelPosY - unk30;
		const uint32_t unk0C = _mstAndyLevelPosY + unk30;
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x10], unk10);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x0C], unk0C);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x14], unk10 - unk34);
		WRITE_LE_UINT32(&_res->_mstMonsterInfos[offset - 0x08], unk0C + unk34);
	}
}

void Game::mstRemoveMonsterObject2(Task *t, Task **tasksList) {
	MonsterObject2 *m = t->monster2;
	m->monster2Info = 0;
	LvlObject *o = m->o;
	if (o) {
		o->dataPtr = 0;
		removeLvlObject2(o);
	}
	removeTask(tasksList, t);
}

void Game::mstRemoveMonsterObject1(Task *t, Task **tasksList) {
	MonsterObject1 *m = t->monster1;
	if (_mstActionNum != -1) {
		if ((m->flagsA5 & 8) != 0 && m->action) {
			mstMonster1ClearChasingMonster(m);
		}
	}
	if (m->monsterInfos[946] & 4) {
		mstBoundingBoxClear(m, 0);
		mstBoundingBoxClear(m, 1);
	}
	m->m46 = 0;
	LvlObject *o = m->o16;
	if (o) {
		o->dataPtr = 0;
	}
	for (int i = 0; i < kMaxMonsterObjects2; ++i) {
		if (_monsterObjects2Table[i].monster2Info != 0 && _monsterObjects2Table[i].monster1 == m) {
			_monsterObjects2Table[i].monster1 = 0;
		}
	}
	removeLvlObject2(o);
	removeTask(tasksList, t);
}

void Game::mstTaskAttack(Task *t, uint32_t codeData, uint8_t flags) {
	MonsterObject1 *m = t->monster1;
	m->flagsA5 = (m->flagsA5 & ~0x70) | flags;
	Task *c = t->child;
	if (c) {
		t->child = 0;
		c->codeData = 0;
	}
	if (m->flagsA5 & 8) {
		Task *n = findFreeTask();
		if (n) {
			memcpy(n, t, sizeof(Task));
			t->child = n;
			if (t->run != &Game::mstTask_wait2) {
				const uint8_t *p = n->codeData - 4;
				if ((t->flags & 0x40) != 0 || p[0] == 203 || ((flags & 0x10) != 0 && (t->run == &Game::mstTask_monsterWait1 || t->run == &Game::mstTask_monsterWait2 || t->run == &Game::mstTask_monsterWait3 || t->run == &Game::mstTask_monsterWait4))) {
					p += 4;
				}
				n->codeData = p;
				n->run = &Game::mstTask_main;
			}
		}
	}
	assert(codeData != kNone);
	resetTask(t, _res->_mstCodeData + codeData * 4);
}

int Game::mstTaskSetActionDirection(Task *t, int num, int delay) {
	MonsterObject1 *m = t->monster1;
	LvlObject *o = m->o16;
	uint8_t var4 = _res->_mstActionDirectionData[num].unk0;
	uint8_t var8 = _res->_mstActionDirectionData[num].unk2;
	const uint8_t *p = m->monsterInfos + var4 * 28;
	uint8_t _al = (o->flags1 >> 4) & 3;
	uint8_t _cl = ((_al & 1) != 0) ? 8 : 2;
	if (_al & 2) {
		_cl |= 4;
	} else {
		_cl |= 1;
	}
	mstLvlObjectSetActionDirection(o, p, var8, _cl);
	const uint8_t am = _res->_mstActionDirectionData[num].unk1;
	o->actionKeyMask |= am;

	t->flags &= ~0x80;
	int vf = (int8_t)p[4];
	int ve = (int8_t)p[5];
	debug(kDebug_MONSTER, "mstTaskSetActionDirection m %p action 0x%x direction 0x%x (%d,%d)", m, o->actionKeyMask, o->directionKeyMask, vf, ve);
	int va = 0;
	if (vf != 0 || ve != 0) {
		int dirMask = 0;
		uint8_t var11 = p[2];
		if (((var11 & kDirectionKeyMaskHorizontal) == kDirectionKeyMaskHorizontal && (o->directionKeyMask & 8) != 0) || ((var11 & kDirectionKeyMaskHorizontal) != kDirectionKeyMaskHorizontal && (o->flags1 & 0x10) != 0)) {
			vf = m->xMstPos - vf;
		} else {
			vf = m->xMstPos + vf;
		}
		if (vf < m->xMstPos) {
			dirMask = kDirectionKeyMaskLeft;
		} else if (vf > m->xMstPos) {
			dirMask = kDirectionKeyMaskRight;
		}
		if ((var11 & kDirectionKeyMaskVertical) == kDirectionKeyMaskVertical && (o->directionKeyMask & 1) != 0) {
			ve = m->yMstPos - ve;
		} else {
			ve = m->yMstPos + ve;
		}
		if (ve < m->yMstPos) {
			dirMask |= kDirectionKeyMaskUp;
		} else if (ve > m->yMstPos) {
			dirMask |= kDirectionKeyMaskDown;
		}
		if (mstMonster1CheckLevelBounds(m, vf, ve, dirMask)) {
			t->flags |= 0x80;
			return 0;
		}
		va = dirMask;
	}
	if ((m->monsterInfos[946] & 4) != 0 && p[0xE] != 0) {
		if (va == 0) {
			ve = 0;
		} else if (_mstLut1[va] & 1) {
			ve = (int8_t)p[0xA];
			va = (int8_t)p[0xB];
		} else {
			ve = (int8_t)p[0x8];
			va = (int8_t)p[0x9];
		}
		if (o->directionKeyMask & kDirectionKeyMaskLeft) {
			ve = -ve;
		} else if ((o->directionKeyMask & kDirectionKeyMaskRight) == 0) {
			ve = 0;
		}
		if (o->directionKeyMask & kDirectionKeyMaskUp) {
			va = -va;
		} else if ((o->directionKeyMask & kDirectionKeyMaskDown) == 0) {
			va = 0;
		}
		const int x1 = m->xMstPos + (int8_t)p[0xC];
		const int y1 = m->yMstPos + (int8_t)p[0xD];
		const int x2 = x1 + p[0xE] - 1;
		const int y2 = y1 + p[0xF] - 1;
		if ((var8 & 0xE0) != 0x60 && mstBoundingBoxCollides2(m->monster1Index, x1, y1, x2, y2) != 0) {
			t->flags |= 0x80;
			return 0;
		}
		m->bboxNum[0] = mstBoundingBoxUpdate(m->bboxNum[0], m->monster1Index, x1, y1, x2, y2);
	}
	m->o_flags0 = var4;
	if (delay == -1) {
		const uint32_t offset = m->monsterInfos - _res->_mstMonsterInfos;
		assert((offset % kMonsterInfoDataSize) == 0);
		t->arg2 = offset / kMonsterInfoDataSize;
		t->run = &Game::mstTask_monsterWait4;
		debug(kDebug_MONSTER, "mstTaskSetActionDirection arg2 %d", t->arg2);
	} else {
		t->arg1 = delay;
		t->run = &Game::mstTask_monsterWait3;
		debug(kDebug_MONSTER, "mstTaskSetActionDirection arg1 %d", t->arg1);
	}
	return 1;
}

Task *Game::findFreeTask() {
	for (int i = 0; i < kMaxTasks; ++i) {
		Task *t = &_tasksTable[i];
		if (!t->codeData) {
			memset(t, 0, sizeof(Task));
			return t;
		}
	}
	warning("findFreeTask() no free task");
	return 0;
}

Task *Game::createTask(const uint8_t *codeData) {
	Task *t = findFreeTask();
	if (t) {
		memset(t, 0, sizeof(Task));
		resetTask(t, codeData);
		t->prevPtr = 0;
		t->nextPtr = _tasksList;
		if (_tasksList) {
			_tasksList->prevPtr = t;
		}
		_tasksList = t;
		return t;
	}
	return 0;
}

void Game::updateTask(Task *t, int num, const uint8_t *codeData) {
	debug(kDebug_MONSTER, "updateTask t %p offset 0x%04x", t, codeData - _res->_mstCodeData);
	Task *current = _tasksList;
	bool found = false;
	while (current) {
		Task *nextPtr = current->nextPtr;
		if (current->localVars[7] == num) {
			found = true;
			if (current != t) {
				if (!codeData) {
					if (current->child) {
						current->child->codeData = 0;
						current->child = 0;
					}
					Task *prev = current->prevPtr;
					current->codeData = 0;
					Task *next = current->nextPtr;
					if (next) {
						next->prevPtr = prev;
					}
					if (prev) {
						prev->nextPtr = next;
					} else {
						_tasksList = next;
					}
				} else {
					t->codeData = codeData;
					t->run = &Game::mstTask_main;
				}
			}
		}
		current = nextPtr;
	}
	if (found) {
		return;
	}
	if (codeData) {
		t = findFreeTask();
		if (t) {
			memset(t, 0, sizeof(Task));
			resetTask(t, codeData);
			t->prevPtr = 0;
			t->nextPtr = _tasksList;
			if (_tasksList) {
				_tasksList->prevPtr = t;
			}
			_tasksList = t;
			t->localVars[7] = num;
		}
	}
}

void Game::resetTask(Task *t, const uint8_t *codeData) {
	debug(kDebug_MONSTER, "resetTask t %p offset 0x%04x", t, codeData - _res->_mstCodeData);
	assert(codeData);
	t->state |= 2;
	t->codeData = codeData;
	t->run = &Game::mstTask_main;
	t->localVars[7] = 0;
	MonsterObject1 *m = t->monster1;
	if (m) {
		const uint8_t mask = m->flagsA5;
		if ((mask & 0x88) == 0 || (mask & 0xF0) == 0) {
			if ((mask & 8) != 0) {
				t->flags = (t->flags & ~0x40) | 0x20;
				m->flags48 &= ~0x1C;
			} else if ((mask & 2) != 0) {
				m->flags48 |= 8;
				const MstBehaviorState *behaviorState = m->behaviorState;
				if (behaviorState->indexUnk51 != kNone) {
					m->flags48 |= 4;
				}
				if (behaviorState->attackBox != kNone) {
					m->flags48 |= 0x10;
				}
			}
		}
	}
}

void Game::removeTask(Task **tasksList, Task *t) {
	Task *c = t->child;
	if (c) {
		c->codeData = 0;
		t->child = 0;
	}
	Task *prev = t->prevPtr;
	t->codeData = 0;
	Task *next = t->nextPtr;
	if (next) {
		next->prevPtr = prev;
	}
	if (prev) {
		prev->nextPtr = next;
	} else {
		*tasksList = next;
	}
}

void Game::appendTask(Task **tasksList, Task *t) {
	Task *current = *tasksList;
	if (!current) {
		*tasksList = t;
		t->nextPtr = t->prevPtr = 0;
	} else {
		// go to last element
		Task *next = current->nextPtr;
		while (next) {
			current = next;
			next = current->nextPtr;
		}
		assert(!current->nextPtr);
		current->nextPtr = t;
		t->nextPtr = 0;
		t->prevPtr = current;
	}
}

int Game::getTaskVar(Task *t, int index, int type) const {
	switch (type) {
	case 1:
		return index;
	case 2:
		assert(index < kMaxLocals);
		return t->localVars[index];
	case 3:
		assert(index < kMaxVars);
		return _mstVars[index];
	case 4:
		return getTaskOtherVar(index, t);
	case 5:
		{
			MonsterObject1 *m = 0;
			if (t->monster2) {
				m = t->monster2->monster1;
			} else {
				m = t->monster1;
			}
			if (m) {
				assert(index < kMaxLocals);
				return m->localVars[index];
			}
		}
		break;
	default:
		warning("getTaskVar unhandled index %d type %d", index, type);
		break;
	}
	return 0;
}

void Game::setTaskVar(Task *t, int index, int type, int value) {
	switch (type) {
	case 2:
		assert(index < kMaxLocals);
		t->localVars[index] = value;
		break;
	case 3:
		assert(index < kMaxVars);
		_mstVars[index] = value;
		break;
	case 5: {
			MonsterObject1 *m = 0;
			if (t->monster2) {
				m = t->monster2->monster1;
			} else {
				m = t->monster1;
			}
			if (m) {
				assert(index < kMaxLocals);
				m->localVars[index] = value;
			}
		}
		break;
	default:
		warning("setTaskVar unhandled index %d type %d", index, type);
		break;
	}
}

int Game::getTaskAndyVar(int index, Task *t) const {
	if (index & 0x80) {
		const int mask = 1 << (index & 0x7F);
		return ((mask & _mstAndyVarMask) != 0) ? 1 : 0;
	}
	switch (index) {
	case 0:
		return (_andyObject->flags1 >> 4) & 1;
	case 1:
		return (_andyObject->flags1 >> 5) & 1;
	case 2: {
			MonsterObject1 *m = t->monster1;
			if (m) {
				return ((m->o16->flags1 & 0x10) != 0) ? 1 : 0;
			} else if (t->monster2) {
				return ((t->monster2->o->flags1 & 0x10) != 0) ? 1 : 0;
			}
		}
		break;
	case 3: {
			MonsterObject1 *m = t->monster1;
			if (m) {
				return ((m->o16->flags1 & 0x20) != 0) ? 1 : 0;
			} else if (t->monster2) {
				return ((t->monster2->o->flags1 & 0x20) != 0) ? 1 : 0;
			}
		}
		break;
	case 4: {
			MonsterObject1 *m = t->monster1;
			if (m) {
				return ((m->o16->flags0 & 0x200) != 0) ? 1 : 0;
			} else if (t->monster2) {
				return ((t->monster2->o->flags0 & 0x200) != 0) ? 1 : 0;
			}
		}
		break;
	case 5:
		return ((_andyObject->flags0 & 0x1F) == 7) ? 1 : 0;
	case 6:
		return (_andyObject->spriteNum == 0) ? 1 : 0;
	case 7:
		if ((_andyObject->flags0 & 0x1F) == 7) {
			AndyLvlObjectData *andyData = (AndyLvlObjectData *)getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			if (andyData) {
				LvlObject *o = andyData->shootLvlObject;
				if (o) {
					ShootLvlObjectData *data = (ShootLvlObjectData *)getLvlObjectDataPtr(o, kObjectDataTypeShoot);
					if (data) {
						return (data->type == 4) ? 1 : 0;
					}
				}
			}
		}
		break;
	case 8:
		if ((_andyObject->flags0 & 0x1F) == 7) {
			AndyLvlObjectData *andyData = (AndyLvlObjectData *)getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			if (andyData) {
				LvlObject *o = andyData->shootLvlObject;
				if (o) {
					ShootLvlObjectData *data = (ShootLvlObjectData *)getLvlObjectDataPtr(o, kObjectDataTypeShoot);
					if (data) {
						return (data->type == 0) ? 1 : 0;
					}
				}
			}
		}
	default:
		warning("getTaskAndyVar unhandled index %d", index);
		break;
	}
	return 0;
}

int Game::getTaskOtherVar(int index, Task *t) const {
	switch (index) {
	case 0:
		return _mstAndyScreenPosX;
	case 1:
		return _mstAndyScreenPosY;
	case 2:
		return _mstAndyLevelPosX;
	case 3:
		return _mstAndyLevelPosY;
	case 4:
		return _currentScreen;
	case 5:
		return _res->_screensState[_currentScreen].s0;
	case 6:
		return _difficulty;
	case 7:
		if (t->monster1 && t->monster1->shootData) {
			return t->monster1->shootData->type;
		}
		return _andyShootsTable[0].type;
	case 8:
		if (t->monster1 && t->monster1->shootData) {
			return t->monster1->shootData->directionMask & 0x7F;
		}
		return _andyShootsTable[0].directionMask & 0x7F;
	case 9:
		if (t->monster1 && t->monster1->action) {
			return t->monster1->action->xPos;
		}
		break;
	case 10:
		if (t->monster1 && t->monster1->action) {
			return t->monster1->action->yPos;
		}
		break;
	case 11:
		if (t->monster1) {
			return t->monster1->shootSource;
		}
		break;
	case 12:
		if (t->monster1) {
			return t->monster1->xPos;
		} else if (t->monster2) {
			return t->monster2->xPos;
		}
		break;
	case 13:
		if (t->monster1) {
			return t->monster1->yPos;
		} else if (t->monster2) {
			return t->monster2->yPos;
		}
		break;
	case 14:
		if (t->monster1) {
			return t->monster1->o16->screenNum;
		} else if (t->monster2) {
			return t->monster2->o->screenNum;
		}
		break;
	case 15:
		if (t->monster1) {
			return t->monster1->xDelta;
		} else if (t->monster2) {
			return ABS(_mstAndyLevelPosX - t->monster2->xMstPos);
		}
		break;
	case 16:
		if (t->monster1) {
			return t->monster1->yDelta;
		} else if (t->monster2) {
			return ABS(_mstAndyLevelPosY - t->monster2->yMstPos);
		}
		break;
	case 17:
		if (t->monster1) {
			return t->monster1->collideDistance;
		}
		break;
	case 18:
		if (t->monster1) {
			return ABS(t->monster1->levelPosBounds_x1 - t->monster1->xMstPos);
		}
		break;
	case 19:
		if (t->monster1) {
			return ABS(t->monster1->levelPosBounds_x2 - t->monster1->xMstPos);
		}
		break;
	case 20:
		if (t->monster1) {
			return ABS(t->monster1->levelPosBounds_y1 - t->monster1->yMstPos);
		}
		break;
	case 21:
		if (t->monster1) {
			return ABS(t->monster1->levelPosBounds_y2 - t->monster1->yMstPos);
		}
		break;
	case 22:
		return _mstOp54Counter;
	case 23:
		return _andyObject->actionKeyMask;
	case 24:
		return _andyObject->directionKeyMask;
	case 25:
		if (t->monster1) {
			return t->monster1->xMstPos;
		} else if (t->monster2) {
			return t->monster2->xMstPos;
		}
		break;
	case 26:
		if (t->monster1) {
			return t->monster1->yMstPos;
		} else if (t->monster2) {
			return t->monster2->yMstPos;
		}
		break;
	case 27:
		if (t->monster1) {
			return t->monster1->o16->flags0 & 0xFF;
		} else if (t->monster2) {
			return t->monster2->o->flags0 & 0xFF;
		}
		break;
	case 28:
		if (t->monster1) {
			return t->monster1->o16->anim;
		} else if (t->monster2) {
			return t->monster2->o->anim;
		}
		break;
	case 29:
		if (t->monster1) {
			return t->monster1->o16->frame;
		} else if (t->monster2) {
			return t->monster2->o->frame;
		}
		break;
	case 30:
		return _mstOp56Counter;
	case 31:
		return _executeMstLogicCounter;
	case 32:
		return _executeMstLogicCounter - _executeMstLogicPrevCounter;
	case 33: {
			LvlObject *o = 0;
			if (t->monster1) {
				o = t->monster1->o16;
			} else if (t->monster2) {
				o = t->monster2->o;
			}
			if (o) {
				return _res->_screensState[o->screenNum].s0;
			}
		}
		break;
	case 34:
		return _level->_checkpoint;
	case 35:
		return _mstAndyCurrentScreenNum;
	default:
		warning("getTaskOtherVar unhandled index %d", index);
		break;
	}
	return 0;
}

int Game::getTaskFlag(Task *t, int num, int type) const {
	switch (type) {
	case 1:
		return num;
	case 2:
		return ((t->flags & (1 << num)) != 0) ? 1 : 0;
	case 3:
		return ((_mstFlags & (1 << num)) != 0) ? 1 : 0;
	case 4:
		return getTaskAndyVar(num, t);
	case 5: {
			MonsterObject1 *m = 0;
			if (t->monster2) {
				m = t->monster2->monster1;
			} else {
				m = t->monster1;
			}
			if (m) {
				return ((m->flags48 & (1 << num)) != 0) ? 1 : 0;
			}
		}
	default:
		warning("getTaskFlag unhandled type %d num %d", type, num);
		break;
	}
	return 0;
}

int Game::mstTask_main(Task *t) {
	assert(t->codeData);
	const int taskNum = t - _tasksTable;
	int ret = 0;
	t->state &= ~2;
	const uint8_t *p = t->codeData;
	do {
		assert(p >= _res->_mstCodeData && p < _res->_mstCodeData + _res->_mstHdr.codeSize * 4);
		assert(((p - t->codeData) & 3) == 0);
		const uint32_t codeOffset = p - _res->_mstCodeData;
		debug(kDebug_MONSTER, "executeMstCode task %d %p code %d offset 0x%04x", taskNum, t, p[0], codeOffset);
		assert(p[0] <= 242);
		switch (p[0]) {
		case 0: { // 0
				LvlObject *o = 0;
				if (t->monster1) {
					if ((t->monster1->flagsA6 & 2) == 0) {
						o = t->monster1->o16;
					}
				} else if (t->monster2) {
					o = t->monster2->o;
				}
				if (o) {
					o->actionKeyMask = 0;
					o->directionKeyMask = 0;
				}
			}
			// fall-through
		case 1: { // 1
				const int num = READ_LE_UINT16(p + 2);
				const int delay = getTaskVar(t, num, p[1]);
				t->arg1 = delay;
				if (delay > 0) {
					if (p[0] == 0) {
						t->run = &Game::mstTask_wait2;
						ret = 1;
					} else {
						t->run = &Game::mstTask_wait1;
						ret = 1;
					}
				}
			}
			break;
		case 2: { // 2 - set_var_random_range
				const int num = READ_LE_UINT16(p + 2);
				MstOp2Data *m = &_res->_mstOp2Data[num];
				int a = getTaskVar(t, m->indexVar1, m->maskVars >> 4); // vb
				int b = getTaskVar(t, m->indexVar2, m->maskVars & 15); // vg
				if (a > b) {
					SWAP(a, b);
				}
				a += _rnd.update() % (b - a + 1);
				setTaskVar(t, m->unkA, m->unk9, a);
			}
			break;
		case 3:
		case 8: // 3 - set_monster_action_direction_imm
			if (t->monster1) {
				const int num = READ_LE_UINT16(p + 2);
				const int arg = _res->_mstActionDirectionData[num].unk3;
				t->codeData = p;
				ret = mstTaskSetActionDirection(t, num, (arg == 0xFF) ? -1 : arg);
			}
			break;
		case 4: // 4 - set_monster_action_direction_task_var
			if (t->monster1) {
				const int num = READ_LE_UINT16(p + 2);
				const int arg = _res->_mstActionDirectionData[num].unk3;
				t->codeData = p;
				assert(arg < kMaxLocals);
				ret = mstTaskSetActionDirection(t, num, t->localVars[arg]);
			}
			break;
		case 13: // 8
			if (t->monster1) {
				const int num = READ_LE_UINT16(p + 2);
				if (mstTestActionDirection(t->monster1, num)) {
					const int arg = _res->_mstActionDirectionData[num].unk3;
					t->codeData = p;
					ret = mstTaskSetActionDirection(t, num, (arg == 0xFF) ? -1 : arg);
				}
			}
			break;
		case 23: // 13 - set_flag_global
			_mstFlags |= (1 << p[1]);
			break;
		case 24: // 14 - set_flag_task
			t->flags |= (1 << p[1]);
			break;
		case 25: { // 15 - set_flag_mst
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					m->flags48 |= (1 << p[1]);
				}
			}
			break;
		case 26: // 16 - clear_flag_global
			_mstFlags &= ~(1 << p[1]);
			break;
		case 27: // 17 - clear_flag_task
			t->flags &= ~(1 << p[1]);
			break;
		case 28: { // 18 - clear_flag_mst
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					m->flags48 &= ~(1 << p[1]);
				}
			}
			break;
		case 30: { // 20
				t->arg1 = 3;
				t->arg2 = p[1];
				if (((1 << p[1]) & _mstFlags) == 0) {
					LvlObject *o = 0;
					if (t->monster1) {
						if ((t->monster1->flagsA6 & 2) == 0) {
							o = t->monster1->o16;
						}
					} else if (t->monster2) {
						o = t->monster2->o;
					}
					if (o) {
						o->actionKeyMask = 0;
						o->directionKeyMask = 0;
					}
					t->run = &Game::mstTask_wait3;
					ret = 1;
				}
			}
			break;
		case 32: { // 22
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					t->arg1 = 5;
					t->arg2 = p[1];
					if (((1 << p[1]) & m->flags48) == 0) {
						LvlObject *o = 0;
						if (t->monster1) {
							if ((t->monster1->flagsA6 & 2) == 0) {
								o = t->monster1->o16;
							}
						} else if (t->monster2) {
							o = t->monster2->o;
						}
						if (o) {
							o->actionKeyMask = 0;
							o->directionKeyMask = 0;
						}
						t->run = &Game::mstTask_wait3;
						ret = 1;
					}
				}
			}
			break;
		case 33:
		case 229: { // 23 - jmp_imm
				const int num = READ_LE_UINT16(p + 2);
				p = _res->_mstCodeData + num * 4 - 4;
			}
			break;
		case 34:
			// no-op
			break;
		case 35: { // 24 - enable_trigger
				const int num = READ_LE_UINT16(p + 2);
				_res->flagMstCodeForPos(num, 1);
			}
			break;
		case 36: { // 25 - disable_trigger
				const int num = READ_LE_UINT16(p + 2);
				_res->flagMstCodeForPos(num, 0);
			}
			break;
		case 39: // 26 - remove_monsters_screen
			if (p[1] < _res->_mstHdr.screensCount) {
				mstOp26_removeMstTaskScreen(&_monsterObjects1TasksList, p[1]);
				mstOp26_removeMstTaskScreen(&_monsterObjects2TasksList, p[1]);
				// mstOp26_removeMstTaskScreen(&_mstTasksList3, p[1]);
				// mstOp26_removeMstTaskScreen(&_mstTasksList4, p[1]);
			}
			break;
		case 40: // 27 - remove_monsters_screen_type
			if (p[1] < _res->_mstHdr.screensCount) {
				mstOp27_removeMstTaskScreenType(&_monsterObjects1TasksList, p[1], p[2]);
				mstOp27_removeMstTaskScreenType(&_monsterObjects2TasksList, p[1], p[2]);
				// mstOp27_removeMstTaskScreenType(&_mstTasksList3, p[1], p[2]);
				// mstOp27_removeMstTaskScreenType(&_mstTasksList4, p[1], p[2]);
			}
			break;
		case 41: { // 28 - increment_task_var
				assert(p[1] < kMaxLocals);
				++t->localVars[p[1]];
			}
			break;
		case 42: { // 29 - increment_global_var
				const int num = p[1];
				assert(num < kMaxVars);
				++_mstVars[num];
			}
			break;
		case 43: { // 30 - increment_monster_var
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					const int num = p[1];
					assert(num < kMaxLocals);
					++m->localVars[num];
				}
			}
			break;
		case 44: { // 31 - decrement_task_var
				const int num = p[1];
				assert(num < kMaxLocals);
				--t->localVars[num];
			}
			break;
		case 45: { // 32 - decrement_global_var
				const int num = p[1];
				assert(num < kMaxVars);
				--_mstVars[num];
			}
			break;
		case 47:
		case 48:
		case 49:
		case 50:
		case 51:
		case 52:
		case 53:
		case 54:
		case 55:
		case 56: { // 34 - arith_task_var_task_var
				assert(p[1] < kMaxLocals);
				assert(p[2] < kMaxLocals);
				arithOp(p[0] - 47, &t->localVars[p[1]], t->localVars[p[2]]);
			}
			break;
		case 57:
		case 58:
		case 59:
		case 60:
		case 61:
		case 62:
		case 63:
		case 64:
		case 65:
		case 66: { // 35 - arith_global_var_task_var
				assert(p[1] < kMaxVars);
				assert(p[2] < kMaxLocals);
				arithOp(p[0] - 57, &_mstVars[p[1]], t->localVars[p[2]]);
				if (p[1] == 31 && _mstVars[31] > 0) {
					_mstTickDelay = _mstVars[31];
				}
			}
			break;
		case 67:
		case 68:
		case 69:
		case 70:
		case 71:
		case 72:
		case 73:
		case 74:
		case 75:
		case 76: { // 36
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					assert(p[1] < kMaxLocals);
					assert(p[2] < kMaxLocals);
					arithOp(p[0] - 67, &m->localVars[p[1]], t->localVars[p[2]]);
				}
			}
			break;
		case 77:
		case 78:
		case 79:
		case 80:
		case 81:
		case 82:
		case 83:
		case 84:
		case 85:
		case 86: { // 37
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					assert(p[1] < kMaxLocals);
					assert(p[2] < kMaxLocals);
					arithOp(p[0] - 77, &t->localVars[p[1]], m->localVars[p[2]]);
				}
			}
			break;
		case 87:
		case 88:
		case 89:
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96: { // 38
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					assert(p[1] < kMaxVars);
					assert(p[2] < kMaxLocals);
					arithOp(p[0] - 87, &_mstVars[p[1]], m->localVars[p[2]]);
					if (p[1] == 31 && _mstVars[31] > 0) {
						_mstTickDelay = _mstVars[31];
					}
				}
			}
			break;
		case 97:
		case 98:
		case 99:
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106: { // 39
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					assert(p[1] < kMaxLocals);
					assert(p[2] < kMaxLocals);
					arithOp(p[0] - 97, &m->localVars[p[1]], m->localVars[p[2]]);
				}
			}
			break;
		case 107:
		case 108:
		case 109:
		case 110:
		case 111:
		case 112:
		case 113:
		case 114:
		case 115:
		case 116: { // 40
				assert(p[1] < kMaxLocals);
				assert(p[2] < kMaxVars);
				arithOp(p[0] - 107, &t->localVars[p[1]], _mstVars[p[2]]);
			}
			break;
		case 117:
		case 118:
		case 119:
		case 120:
		case 121:
		case 122:
		case 123:
		case 124:
		case 125:
		case 126: { // 41
				assert(p[1] < kMaxVars);
				assert(p[2] < kMaxVars);
				arithOp(p[0] - 117, &_mstVars[p[1]], _mstVars[p[2]]);
				if (p[1] == 31 && _mstVars[31] > 0) {
					_mstTickDelay = _mstVars[31];
				}
			}
			break;
		case 127:
		case 128:
		case 129:
		case 130:
		case 131:
		case 132:
		case 133:
		case 134:
		case 135:
		case 136: { // 42
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					assert(p[1] < kMaxLocals);
					assert(p[2] < kMaxVars);
					arithOp(p[0] - 127, &m->localVars[p[1]], _mstVars[p[2]]);
				}
			}
			break;
		case 137:
		case 138:
		case 139:
		case 140:
		case 141:
		case 142:
		case 143:
		case 144:
		case 145:
		case 146: { // 43
				const int num = p[2];
				assert(p[1] < kMaxLocals);
				arithOp(p[0] - 137, &t->localVars[p[1]], getTaskOtherVar(num, t));
			}
			break;
		case 147:
		case 148:
		case 149:
		case 150:
		case 151:
		case 152:
		case 153:
		case 154:
		case 155:
		case 156: { // 44
				const int num = p[2];
				assert(p[1] < kMaxVars);
				arithOp(p[0] - 147, &_mstVars[p[1]], getTaskOtherVar(num, t));
				if (p[1] == 31 && _mstVars[31] > 0) {
					_mstTickDelay = _mstVars[31];
				}
			}
			break;
		case 157:
		case 158:
		case 159:
		case 160:
		case 161:
		case 162:
		case 163:
		case 164:
		case 165:
		case 166: { // 45
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					const int num = p[2];
					assert(p[1] < kMaxLocals);
					arithOp(p[0] - 157, &m->localVars[p[1]], getTaskOtherVar(num, t));
				}
			}
			break;
		case 167:
		case 168:
		case 169:
		case 170:
		case 171:
		case 172:
		case 173:
		case 174:
		case 175:
		case 176: { // 46
				const int16_t num = READ_LE_UINT16(p + 2);
				assert(p[1] < kMaxLocals);
				arithOp(p[0] - 167, &t->localVars[p[1]], num);
			}
			break;
		case 177:
		case 178:
		case 179:
		case 180:
		case 181:
		case 182:
		case 183:
		case 184:
		case 185:
		case 186: { // 47
				const int16_t num = READ_LE_UINT16(p + 2);
				assert(p[1] < kMaxVars);
				arithOp(p[0] - 177, &_mstVars[p[1]], num);
				if (p[1] == 31 && _mstVars[31] > 0) {
					_mstTickDelay = _mstVars[31];
				}
			}
			break;
		case 187:
		case 188:
		case 189:
		case 190:
		case 191:
		case 192:
		case 193:
		case 194:
		case 195:
		case 196: { // 48 - arith_monster_var_imm
				MonsterObject1 *m = 0;
				if (t->monster2) {
					m = t->monster2->monster1;
				} else {
					m = t->monster1;
				}
				if (m) {
					const int16_t num = READ_LE_UINT16(p + 2);
					assert(p[1] < kMaxLocals);
					arithOp(p[0] - 187, &m->localVars[p[1]], num);
				}
			}
			break;
		case 197: // 49
			if (t->monster1) {
				const int num = READ_LE_UINT16(p + 2);
				const MstOp197Data *op197Data = &_res->_mstOp197Data[num];
				const uint32_t mask = op197Data->maskVars;
				int a = getTaskVar(t, op197Data->unk0, (mask >> 16) & 15); // var1C
				int b = getTaskVar(t, op197Data->unk2, (mask >> 12) & 15); // x2
				int c = getTaskVar(t, op197Data->unk4, (mask >>  8) & 15); // var14
				int d = getTaskVar(t, op197Data->unk6, (mask >>  4) & 15); // vg
				int e = getTaskVar(t, op197Data->unkE,  mask        & 15); // va
				const int screenNum = CLIP(e, -4, _res->_mstHdr.screensCount - 1);
				ret = mstOp49_setMovingBounds(a, b, c, d, screenNum, t, num);
			}
			break;
		case 198: { // 50 - call_task
				Task *child = findFreeTask();
				if (child) {
					t->codeData = p + 4;
					memcpy(child, t, sizeof(Task));
					t->child = child;
					const uint16_t num = READ_LE_UINT16(p + 2);
					const uint32_t codeData = _res->_mstUnk60[num];
					assert(codeData != kNone);
					p = _res->_mstCodeData + codeData * 4;
					t->codeData = p;
					t->state &= ~2;
					p -= 4;
				}
			}
			break;
		case 199: // 51 - stop_monster
			mstTaskStopMonsterObject1(t);
			return 0;
		case 200: // 52 - stop_monster_action
			if (t->monster1 && t->monster1->action) {
				mstOp52();
				return 1;
			}
			mstOp52();
			break;
		case 201: { // 53 - start_monster_action
				const int num = READ_LE_UINT16(p + 2);
				mstOp53(&_res->_mstMonsterActionData[num]);
			}
			break;
		case 202: // 54 - continue_monster_action
			mstOp54();
			break;
		case 203: // 55 - monster_attack
			// _mstCurrentMonster1 = t->monster1;
			if (t->monster1) {
				const int num = READ_LE_UINT16(p + 2);
				if (mstCollidesByFlags(t->monster1, _res->_mstOp240Data[num].flags)) {
					t->codeData = p + 4;
					mstTaskAttack(t, _res->_mstOp240Data[num].codeData, 0x10);
					t->state &= ~2;
					p = t->codeData - 4;
				}
			}
			break;
		case 204: // 56 - special_action
			ret = mstOp56_specialAction(t, p[1], READ_LE_UINT16(p + 2));
			break;
		case 207:
		case 208:
		case 209: // 79
			break;
		case 210: // 57 - add_worm
			{
				MonsterObject1 *m = t->monster1;
				mstOp57_addWormHoleSprite(m->xPos + (int8_t)p[2], m->yPos + (int8_t)p[3], m->o16->screenNum);
			}
			break;
		case 211: // 58 - add_lvl_object
			mstOp58_addLvlObject(t, READ_LE_UINT16(p + 2));
			break;
		case 212: { // 59
				LvlObject *o = 0;
				if (t->monster2) {
					o = t->monster2->o;
				} else if (t->monster1) {
					o = t->monster1->o16;
				} else {
					break;
				}
				assert(o);
				int xPos = o->xPos + o->posTable[6].x; // vf
				int yPos = o->yPos + o->posTable[6].y; // vd
				const uint16_t flags1 = o->flags1;
				if (flags1 & 0x10) {
					xPos -= (int8_t)p[2];
				} else {
					xPos += (int8_t)p[2];
				}
				if (flags1 & 0x20) {
					yPos -= (int8_t)p[3];
				} else {
					yPos += (int8_t)p[3];
				}
				int dirMask = 0;
				if ((t->monster1 && (t->monster1->monsterInfos[944] == 10 || t->monster1->monsterInfos[944] == 25)) || (t->monster2 && (t->monster2->monster2Info->type == 10 || t->monster2->monster2Info->type == 25))) {
					int dx = o->posTable[6].x - o->posTable[7].x;
					int dy = o->posTable[6].y - o->posTable[7].y;
					if (dx >= 8) {
						dirMask = 2;
					} else if (dx < -8) {
						dirMask = 8;
					}
					if (dy >= 8) {
						dirMask |= 4;
					} else if (dy < -8) {
						dirMask |= 1;
					}
				} else {
					dirMask = ((flags1 & 0x10) != 0) ? 8 : 0;
				}
				if (p[1] == 255) {
					int type = 0;
					switch (dirMask) {
					case 1:
						type = 6;
						break;
					case 3:
						type = 3;
						break;
					case 4:
						type = 7;
						break;
					case 6:
						type = 4;
						break;
					case 8:
						type = 5;
						break;
					case 9:
						type = 1;
						break;
					case 12:
						type = 2;
						break;
					}
					mstOp59_addShootSpecialPowers(xPos, yPos, o->screenNum, type, (o->flags2 + 1) & 0xDFFF);
				} else {
					int type = 0;
					switch (dirMask) {
					case 1:
						type = 6;
						break;
					case 3:
						type = 3;
						break;
					case 4:
						type = 7;
						break;
					case 6:
						type = 4;
						break;
					case 8:
						type = 5;
						break;
					case 9:
						type = 1;
						break;
					case 12:
						type = 2;
						break;
					}
					mstOp59_addShootFireball(xPos, yPos, o->screenNum, p[1], type, (o->flags2 + 1) & 0xDFFF);
				}
			}
			break;
		case 213: { // 60 - monster_set_action_direction
				LvlObject *o = 0;
				if (t->monster2) {
					o = t->monster2->o;
				} else if (t->monster1) {
					o = t->monster1->o16;
				}
				if (o) {
					o->actionKeyMask = getTaskVar(t, p[2], p[1] >> 4);
					o->directionKeyMask = getTaskVar(t, p[3], p[1] & 15);
				}
			}
			break;
		case 214: { // 61 - reset_monster_energy
				MonsterObject1 *m = t->monster1;
				if (m) {
					m->flagsA5 &= ~0xC0;
					m->localVars[7] = m->behaviorState->energy;
				}
			}
			break;
		case 215: { // 62
				if (_m43Num3 != -1) {
					assert(_m43Num3 < _res->_mstHdr.monsterActionIndexDataCount);
					shuffleMstMonsterActionIndex(&_res->_mstMonsterActionIndexData[_m43Num3]);
				}
				_mstOp54Counter = 0;
			}
			break;
		case 217: { // 64
				const int16_t num = READ_LE_UINT16(p + 2);
				if (_m43Num3 != num) {
					_m43Num3 = num;
					assert(num >= 0 && num < _res->_mstHdr.monsterActionIndexDataCount);
					shuffleMstMonsterActionIndex(&_res->_mstMonsterActionIndexData[num]);
					_mstOp54Counter = 0;
				}
			}
			break;
		case 218: { // 65
				const int16_t num = READ_LE_UINT16(p + 2);
				if (num != _m43Num1) {
					_m43Num1 = num;
					_m43Num2 = num;
					assert(num >= 0 && num < _res->_mstHdr.monsterActionIndexDataCount);
					shuffleMstMonsterActionIndex(&_res->_mstMonsterActionIndexData[num]);
				}
			}
			break;
		case 220:
		case 221:
		case 222:
		case 223:
		case 224:
		case 225: { // 67 - add_monster
				const int num = READ_LE_UINT16(p + 2);
				MstOp223Data *m = &_res->_mstOp223Data[num];
				const int mask = m->maskVars; // var8
				int a = getTaskVar(t, m->indexVar1, (mask >> 16) & 15); // var1C
				int b = getTaskVar(t, m->indexVar2, (mask >> 12) & 15); // var20
				int c = getTaskVar(t, m->indexVar3, (mask >>  8) & 15); // var14, vb
				int d = getTaskVar(t, m->indexVar4, (mask >>  4) & 15); // vf
				int e = getTaskVar(t, m->indexVar5,  mask        & 15); // va
				if (a > b) {
					SWAP(a, b);
				}
				if (c > d) {
					SWAP(c, d);
				}
				LvlObject *o = 0;
				if (t->monster2) {
					o = t->monster2->o;
				} else if (t->monster1) {
					o = t->monster1->o16;
				}
				if (e <= -2 && o) {
					if (o->flags1 & 0x10) {
						const int x1 = a;
						const int x2 = b;
						a = o->xPos - x2;
						b = o->xPos - x1;
					} else {
						a += o->xPos;
						b += o->xPos;
					}

					c += o->yPos;
					d += o->yPos;
					if (e < -2) {
						a += o->posTable[6].x;
						b += o->posTable[6].x;
						c += o->posTable[6].y;
						d += o->posTable[6].y;
					} else {
						a += o->posTable[7].x;
						b += o->posTable[7].x;
						c += o->posTable[7].y;
						d += o->posTable[7].y;
					}
					e = o->screenNum;
				}
				e = CLIP(e, -1, _res->_mstHdr.screensCount - 1);
				if (p[0] == 224) {
					_mstOp67_type = m->unk8;
					_mstOp67_flags1 = m->unk9;
					_mstOp67_unk = m->unkC;
					_mstOp67_x1 = a;
					_mstOp67_x2 = b;
					_mstOp67_y1 = c;
					_mstOp67_y2 = d;
					_mstOp67_screenNum = e;
					break;
				} else if (p[0] == 225) {
					_mstOp68_type = m->unk8;
					_mstOp68_arg9 = m->unk9;
					_mstOp68_flags1 = m->unkC;
					_mstOp68_x1 = a;
					_mstOp68_x2 = b;
					_mstOp68_y1 = c;
					_mstOp68_y2 = d;
					_mstOp68_screenNum = e;
					break;
				} else {
					t->flags |= 0x80;
					if (p[0] == 222 || p[0] == 220) {
						if (e == -1) {
							if (a >= -_mstAndyScreenPosX && a <= 255 - _mstAndyScreenPosX) {
								break;
							}
						} else if (e == _currentScreen) {
							break;
						}
					}
				}
				mstOp67_addMonster(t, a, b, c, d, e, m->unk8, m->unk9, m->unkC, m->unkB, 0, m->unkE);
			}
			break;
		case 226: { // 68 - add_monster_group
				const int num = READ_LE_UINT16(p + 2);
				const MstOp226Data *m226Data = &_res->_mstOp226Data[num];
				int xPos = _res->_mstPointOffsets[_currentScreen].xOffset;
				int yPos = _res->_mstPointOffsets[_currentScreen].yOffset;
				int countRight = 0; // var1C
				int countLeft  = 0; // var8
				int xOffset  = m226Data->unk4 * 256;
				for (int i = 0; i < kMaxMonsterObjects1; ++i) {
					MonsterObject1 *m = &_monsterObjects1Table[i];
					if (!m->m46) {
						continue;
					}
					if (m->monsterInfos[944] != _res->_mstMonsterInfos[m226Data->unk0 * kMonsterInfoDataSize + 944]) {
						continue;
					}
					if (!rect_contains(xPos - xOffset, yPos, xPos + xOffset + 256, yPos + 192, m->xMstPos, m->yMstPos)) {
						continue;
					}
					if (_mstAndyLevelPosX > m->xMstPos) {
						++countLeft;
					} else {
						++countRight;
					}
				}
				t->flags |= 0x80;
				const int total = countRight + countLeft; // vb
				if (total >= m226Data->unk3) {
					break;
				}
				int vc = m226Data->unk3 - total;

				int countType1 = m226Data->unk1; // vf
				if (countLeft >= countType1) {
					countType1 = 0;
				} else {
					countType1 -= countLeft;
				}
				int countType2 = m226Data->unk2; // vg
				if (countRight >= countType2) {
					countType2 = 0;
				} else {
					countType2 -= countRight;
				}
				if (countType1 != 0) {
					if (_mstOp67_screenNum < 0) {
						if (_mstOp67_x2 >= -_mstAndyScreenPosX && _mstOp67_x1 <= 255 - _mstAndyScreenPosX) {
							countType1 = 0;
						}
					} else if (_mstOp67_screenNum == _currentScreen) {
						countType1 = 0;
					}
				}
				if (countType2 != 0) {
					if (_mstOp68_screenNum < 0) {
						if (_mstOp68_x2 >= -_mstAndyScreenPosX && _mstOp68_x1 <= 255 -_mstAndyScreenPosX) {
							countType2 = 0;
						}
					} else if (_mstOp68_screenNum == _currentScreen) {
						countType2 = 0;
					}
				}
				if (countType1 != 0 || countType2 != 0) {
					mstOp68_addMonsterGroup(t, _res->_mstMonsterInfos + m226Data->unk0 * kMonsterInfoDataSize, countType1, countType2, vc, m226Data->unk6);
				}
			}
			break;
		case 227: { // 69 - compare_vars
				const int num = READ_LE_UINT16(p + 2);
				assert(num < _res->_mstHdr.op227DataCount);
				const MstOp227Data *m = &_res->_mstOp227Data[num];
				const int a = getTaskVar(t, m->indexVar1, m->maskVars & 15);
				const int b = getTaskVar(t, m->indexVar2, m->maskVars >> 4);
				if (compareOp(m->compare, a, b)) {
					assert(m->codeData < _res->_mstHdr.codeSize);
					p = _res->_mstCodeData + m->codeData * 4 - 4;
				}
			}
			break;
		case 228: { // 70 - compare_flags
				const int num = READ_LE_UINT16(p + 2);
				assert(num < _res->_mstHdr.op227DataCount);
				const MstOp227Data *m = &_res->_mstOp227Data[num];
				const int a = getTaskFlag(t, m->indexVar1, m->maskVars & 15);
				const int b = getTaskFlag(t, m->indexVar2, m->maskVars >> 4);
				if (compareOp(m->compare, a, b)) {
					assert(m->codeData < _res->_mstHdr.codeSize);
					p = _res->_mstCodeData + m->codeData * 4 - 4;
				}
			}
			break;
		case 231:
		case 232: { // 71 - compare_flags_monster
				const int num = READ_LE_UINT16(p + 2);
				const MstOp234Data *m = &_res->_mstOp234Data[num];
				const int a = getTaskFlag(t, m->indexVar1, m->maskVars & 15);
				const int b = getTaskFlag(t, m->indexVar2, m->maskVars >> 4);
				if (compareOp(m->compare, a, b)) {
					if (p[0] == 231) {
						LvlObject *o = 0;
						if (t->monster1) {
							if ((t->monster1->flagsA6 & 2) == 0) {
								o = t->monster1->o16;
							}
						} else if (t->monster2) {
							o = t->monster2->o;
						}
						if (o) {
							o->actionKeyMask = 0;
							o->directionKeyMask = 0;
						}
						t->arg2 = num;
						t->run = &Game::mstTask_mstOp231;
						ret = 1;
					}
				} else {
					if (p[0] == 232) {
						LvlObject *o = 0;
						if (t->monster1) {
							if ((t->monster1->flagsA6 & 2) == 0) {
								o = t->monster1->o16;
							}
						} else if (t->monster2) {
							o = t->monster2->o;
						}
						if (o) {
							o->actionKeyMask = 0;
							o->directionKeyMask = 0;
						}
						t->arg2 = num;
						t->run = &Game::mstTask_mstOp232;
						ret = 1;
					}
				}
			}
			break;
		case 233:
		case 234: { // 72 - compare_vars_monster
				const int num = READ_LE_UINT16(p + 2);
				const MstOp234Data *m = &_res->_mstOp234Data[num];
				const int a = getTaskVar(t, m->indexVar1, m->maskVars & 15);
				const int b = getTaskVar(t, m->indexVar2, m->maskVars >> 4);
				if (compareOp(m->compare, a, b)) {
					if (p[0] == 233) {
						LvlObject *o = 0;
						if (t->monster1) {
							if ((t->monster1->flagsA6 & 2) == 0) {
								o = t->monster1->o16;
							}
						} else if (t->monster2) {
							o = t->monster2->o;
						}
						if (o) {
							o->actionKeyMask = 0;
							o->directionKeyMask = 0;
						}
						t->arg2 = num;
						t->run = &Game::mstTask_mstOp233;
						ret = 1;
					}
				} else {
					if (p[0] == 234) {
						LvlObject *o = 0;
						if (t->monster1) {
							if ((t->monster1->flagsA6 & 2) == 0) {
								o = t->monster1->o16;
							}
						} else if (t->monster2) {
							o = t->monster2->o;
						}
						if (o) {
							o->actionKeyMask = 0;
							o->directionKeyMask = 0;
						}
						t->arg2 = num;
						t->run = &Game::mstTask_mstOp234;
						ret = 1;
					}
				}
			}
			break;
		case 237: // 74 - remove_monster_task
			if (t->monster1) {
				if (!t->monster2) {
					mstRemoveMonsterObject1(t, &_monsterObjects1TasksList);
					return 1;
				}
				mstRemoveMonsterObject2(t, &_monsterObjects2TasksList);
				return 1;
			} else {
				if (t->monster2) {
					mstRemoveMonsterObject2(t, &_monsterObjects2TasksList);
					return 1;
				}
			}
			break;
		case 238: { // 75 - jmp
				const int i = READ_LE_UINT16(p + 2);
				const uint32_t codeData = _res->_mstUnk60[i];
				assert(codeData != kNone);
				p = _res->_mstCodeData + codeData * 4;
				t->codeData = p;
				p -= 4;
			}
			break;
		case 239: { // 76  - create_task
				const int i = READ_LE_UINT16(p + 2);
				const uint32_t codeData = _res->_mstUnk60[i];
				assert(codeData != kNone);
				createTask(_res->_mstCodeData + codeData * 4);
			}
			break;
		case 240: { // 77 - update_task
				const int num = READ_LE_UINT16(p + 2);
				MstOp240Data *m = &_res->_mstOp240Data[num];
				const uint8_t *codeData = (m->codeData == kNone) ? 0 : (_res->_mstCodeData + m->codeData * 4);
				updateTask(t, m->flags, codeData);
			}
			break;
		case 242: // 78 - terminate
			debug(kDebug_MONSTER, "child %p monster1 %p monster2 %p", t->child, t->monster1, t->monster2);
			if (t->child) {
				Task *child = t->child;
				child->prevPtr = t->prevPtr;
				child->nextPtr = t->nextPtr;
				memcpy(t, child, sizeof(Task));
				t->child = 0;
				t->state &= ~2;
				child->codeData = 0;
				MonsterObject1 *m = t->monster1;
				if (m) {
					m->flagsA5 &= ~0x70;
					if ((m->flagsA5 & 8) != 0 && !m->action) {
						mstTaskResetMonster1Direction(t);
						return 1;
					}
				}
				return 0;
			} else if (t->monster1) {
				MonsterObject1 *m = t->monster1;
				if (m->flagsA6 & 4) {
					return 1;
				}
				if ((m->flagsA5 & 0x80) == 0) {
					if ((m->flagsA5 & 8) == 0) {
						m->flags48 |= 8;
						if ((m->flagsA5 & 0x70) != 0) {
							m->flagsA5 &= ~0x70;
							switch (m->flagsA5 & 7) {
							case 1:
							case 2: {
									const uint32_t codeData = mstMonster1GetNextWalkCode(m);
									assert(codeData != kNone);
									resetTask(t, _res->_mstCodeData + codeData * 4);
									t->state &= ~2;
									p = t->codeData - 4;
								}
								break;
							case 5:
								return mstTaskInitMonster1Type1(t);
							case 6:
								return mstTaskInitMonster1Type2(t, 1);
							}
						} else {
							const uint32_t codeData = mstMonster1GetNextWalkCode(m);
							assert(codeData != kNone);
							resetTask(t, _res->_mstCodeData + codeData * 4);
							t->state &= ~2;
							const int counter = m->executeCounter;
							m->executeCounter = _executeMstLogicCounter;
							p = t->codeData - 4;
							if (m->executeCounter == counter) {
								if ((m->flagsA6 & 2) == 0) {
									if (m->o16) {
										m->o16->actionKeyMask = 0;
										m->o16->directionKeyMask = 0;
									}
								}
								ret = 1;
							}
						}
					} else {
						if (m->action) {
							mstMonster1ClearChasingMonster(m);
						}
						m->flagsA5 = (m->flagsA5 & ~0xF) | 6;
						return mstTaskInitMonster1Type2(t, 1);
					}
				} else if ((m->flagsA5 & 8) != 0) {
					m->flagsA5 &= ~8;
					const uint32_t codeData = m->behaviorState->codeData;
					if (codeData != kNone) {
						resetTask(t, _res->_mstCodeData + codeData * 4);
						return 0;
					} else {
						m->o16->actionKeyMask = 7;
						m->o16->directionKeyMask = 0;
						t->run = &Game::mstTask_idle;
						return 1;
					}
				} else {
					t->run = &Game::mstTask_idle;
					return 1;
				}
			} else if (t->monster2) {
				mstRemoveMonsterObject2(t, &_monsterObjects2TasksList);
				ret = 1;
			} else {
				if ((t->state & 1) != 0 && _mstVars[31] == 0) {
					_mstVars[31] = _mstTickDelay;
				}
				removeTask(&_tasksList, t);
				ret = 1;
			}
			break;
		default:
			warning("Unhandled opcode %d in mstTask_main", *p);
			break;
		}
		p += 4;
		if ((t->state & 2) != 0) {
			t->state &= ~2;
			p = t->codeData;
		}
		++_runTaskOpcodesCount;
		if (_runTaskOpcodesCount >= 128) { // prevent infinite loop
			warning("Stopping task %p, counter %d", t, _runTaskOpcodesCount);
			break;
		}
	} while (ret == 0);
	if (t->codeData) {
		t->codeData = p;
	}
	return 1;
}

void Game::mstOp26_removeMstTaskScreen(Task **tasksList, int screenNum) {
	Task *current = *tasksList; // vg
	while (current) {
		MonsterObject1 *m = current->monster1; // vc
		Task *next = current->nextPtr; // ve
		if (m && m->o16->screenNum == screenNum) {
			if (_mstActionNum != -1 && (m->flagsA5 & 8) != 0 && m->action) {
				mstMonster1ClearChasingMonster(m);
			}
			if (m->monsterInfos[946] & 4) {
				mstBoundingBoxClear(m, 0);
				mstBoundingBoxClear(m, 1);
			}
			mstMonster1ResetData(m);
			removeLvlObject2(m->o16);
			removeTask(tasksList, current);
		} else {
			MonsterObject2 *mo = current->monster2;
			if (mo && mo->o->screenNum == screenNum) {
				mo->monster2Info = 0;
				mo->o->dataPtr = 0;
				removeLvlObject2(mo->o);
				removeTask(tasksList, current);
			}
		}
		current = next;
	}
}

void Game::mstOp27_removeMstTaskScreenType(Task **tasksList, int screenNum, int type) {
	Task *current = *tasksList;
	while (current) {
		MonsterObject1 *m = current->monster1;
		Task *next = current->nextPtr;
		if (m && m->o16->screenNum == screenNum && (m->monsterInfos[944] & 0x7F) == type) {
			if (_mstActionNum != -1 && (m->flagsA5 & 8) != 0 && m->action) {
				mstMonster1ClearChasingMonster(m);
			}
			if (m->monsterInfos[946] & 4) {
				mstBoundingBoxClear(m, 0);
				mstBoundingBoxClear(m, 1);
			}
			mstMonster1ResetData(m);
			removeLvlObject2(m->o16);
			removeTask(tasksList, current);
		} else {
			MonsterObject2 *mo = current->monster2;
			if (mo && mo->o->screenNum == screenNum && (mo->monster2Info->type & 0x7F) == type) {
				mo->monster2Info = 0;
				mo->o->dataPtr = 0;
				removeLvlObject2(mo->o);
				removeTask(tasksList, current);
			}
		}
		current = next;
	}
}

int Game::mstOp49_setMovingBounds(int a, int b, int c, int d, int screen, Task *t, int num) {
	debug(kDebug_MONSTER, "mstOp49 %d %d %d %d %d %d", a, b, c, d, screen, num);
	MonsterObject1 *m = t->monster1;
	const MstOp197Data *op197Data = &_res->_mstOp197Data[num];
	MstMovingBounds *m49 = &_res->_mstMovingBoundsData[op197Data->indexUnk49];
	m->m49 = m49;
	m->indexUnk49Unk1 = op197Data->unkF;
	if (m->indexUnk49Unk1 < 0) {
		if (m49->indexDataCount == 0) {
			m->indexUnk49Unk1 = 0;
		} else {
			m->indexUnk49Unk1 = m49->indexData[_rnd.getMstNextNumber(m->rnd_m49)];
		}
	}
	assert((uint32_t)m->indexUnk49Unk1 < m49->count1);
	m->m49Unk1 = &m49->data1[m->indexUnk49Unk1];
	m->goalScreenNum = screen;
	if (a > b) {
		m->goalDistance_x1 = b;
		m->goalDistance_x2 = a;
	} else {
		m->goalDistance_x1 = a;
		m->goalDistance_x2 = b;
	}
	if (c > d) {
		m->goalDistance_y1 = d;
		m->goalDistance_y2 = c;
	} else {
		m->goalDistance_y1 = c;
		m->goalDistance_y2 = d;
	}
	switch (screen + 4) {
	case 1: { // 0xFD
			m->unkE4 = 255;
			const uint8_t _al = m->monsterInfos[946];
			if (_al & 4) {
				t->run = &Game::mstTask_monsterWait10;
			} else if (_al & 2) {
				t->run = &Game::mstTask_monsterWait8;
			} else {
				t->run = &Game::mstTask_monsterWait6;
			}
			if (m->goalDistance_x2 <= 0) {
				m->goalScreenNum = kNoScreen;
				if (m->xMstPos < _mstAndyLevelPosX) {
					m->goalDistance_x1 = -m->goalDistance_x2;
					m->goalDistance_x2 = -m->goalDistance_x1;
				}
			}
			if ((_al & 2) != 0 && m->goalDistance_y2 <= 0) {
				m->goalScreenNum = kNoScreen;
				if (m->yMstPos < _mstAndyLevelPosY) {
					m->goalDistance_y1 = -m->goalDistance_y2;
					m->goalDistance_y2 = -m->goalDistance_y1;
				}
			}
			
		}
		break;
	case 0: { // 0xFC
			const uint8_t _al = m->unkF8;
			if (_al & 8) {
				m->goalDistance_x1 = -b;
				m->goalDistance_x2 = -a;
			} else if (_al & 2) {
				m->goalDistance_x1 = a;
				m->goalDistance_x2 = b;
			} else {
				m->goalDistance_x1 = 0;
				m->goalDistance_x2 = 0;
			}
			if (_al & 1) {
				m->goalDistance_y1 = -d;
				m->goalDistance_y2 = -c;
			} else if (_al & 4) {
				m->goalDistance_y1 = c;
				m->goalDistance_y2 = d;
			} else {
				m->goalDistance_y1 = 0;
				m->goalDistance_y2 = 0;
			}
		}
		// fall-through
	case 2: // 0xFE
		if (m->monsterInfos[946] & 4) {
			t->run = &Game::mstTask_monsterWait9;
		} else if (m->monsterInfos[946] & 2) {
			t->run = &Game::mstTask_monsterWait7;
		} else {
			t->run = &Game::mstTask_monsterWait5;
		}
		m->goalDistance_x1 += m->xMstPos;
		m->goalDistance_x2 += m->xMstPos;
		m->goalDistance_y1 += m->yMstPos;
		m->goalDistance_y2 += m->yMstPos;
		break;
	case 3: // 0xFF
		if (m->monsterInfos[946] & 4) {
			t->run = &Game::mstTask_monsterWait10;
		} else if (m->monsterInfos[946] & 2) {
			t->run = &Game::mstTask_monsterWait8;
		} else {
			t->run = &Game::mstTask_monsterWait6;
		}
		break;
	default:
		if (m->monsterInfos[946] & 4) {
			t->run = &Game::mstTask_monsterWait9;
		} else if (m->monsterInfos[946] & 2) {
			t->run = &Game::mstTask_monsterWait7;
		} else {
			t->run = &Game::mstTask_monsterWait5;
		}
		m->goalDistance_x1 += _res->_mstPointOffsets[screen].xOffset;
		m->goalDistance_x2 += _res->_mstPointOffsets[screen].xOffset;
		m->goalDistance_y1 += _res->_mstPointOffsets[screen].yOffset;
		m->goalDistance_y2 += _res->_mstPointOffsets[screen].yOffset;
		break;
	}
	m->goalPos_x1 = m->goalDistance_x1;
	m->goalPos_x2 = m->goalDistance_x2;
	m->goalPos_y1 = m->goalDistance_y1;
	m->goalPos_y2 = m->goalDistance_y2;
	m->walkBoxNum = 0xFF;
	m->unkC0 = -1;
	m->unkBC = -1;
	m->targetDirectionMask = 0xFF;
	const uint8_t *ptr = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
	if ((ptr[2] & kDirectionKeyMaskVertical) == 0) {
		m->goalDistance_y1 = m->goalPos_y1 = m->goalDistance_y2 = m->goalPos_y2 = m->yMstPos;
	}
	if ((ptr[2] & kDirectionKeyMaskHorizontal) == 0) {
		m->goalDistance_x1 = m->goalPos_x1 = m->goalDistance_x2 = m->goalPos_x2 = m->xMstPos;
	}
	if (m->monsterInfos[946] & 4) {
		m->unkAB = 0xFF;
		m->targetLevelPos_x = -1;
		m->targetLevelPos_y = -1;
		mstBoundingBoxClear(m, 1);
	}
	uint8_t _dl = m->goalScreenNum;
	if (_dl != 0xFC && (m->flagsA5 & 8) != 0 && (t->flags & 0x20) != 0 && m->action) {
		if (t->run != &Game::mstTask_monsterWait6 && t->run != &Game::mstTask_monsterWait8 && t->run != &Game::mstTask_monsterWait10) {
			if ((_dl == 0xFE && m->o16->screenNum != _currentScreen) || (_dl != 0xFE && _dl != _currentScreen)) {
				if (m->monsterInfos[946] & 4) {
					mstBoundingBoxClear(m, 1);
				}
				return mstTaskStopMonsterObject1(t);
			}
		} else {
			int x1, x2;
			const int x = MIN(_mstAndyScreenPosX, 255);
			if (_mstAndyScreenPosX < 0) {
				x1 = x;
				x2 = x + 255;
			} else {
				x1 = -x;
				x2 = 255 - x;
			}
			const int y = MIN(_mstAndyScreenPosY, 191);
			int y1, y2; // ve, var4
			if (_mstAndyScreenPosY < 0) {
				y1 = y;
				y2 = y + 191;
			} else {
				y1 = -y;
				y2 = 191 - y;
			}
			int vd, vf;
			if (_dl == 0xFD && m->xMstPos < _mstAndyLevelPosX) {
				vd = -m->goalDistance_x2;
				vf = -m->goalDistance_x1;
			} else {
				vd = m->goalDistance_x1;
				vf = m->goalDistance_x2;
			}
			uint8_t _bl = m->monsterInfos[946] & 2;
			int va, vc;
			if (_bl != 0 && _dl == 0xFD && m->yMstPos < _mstAndyLevelPosY) {
				va = -m->goalDistance_y2;
				vc = -m->goalDistance_y1;
			} else {
				va = m->goalDistance_y1;
				vc = m->goalDistance_y2;
			}
			if (vd < x1 || vf > x2 || (_bl != 0 && (va < y1 || vc > y2))) {
				if ((m->monsterInfos[946] & 4) != 0) {
					mstBoundingBoxClear(m, 1);
				}
				return mstTaskStopMonsterObject1(t);
			}
		}
	}
	const uint8_t *p = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
	if ((m->monsterInfos[946] & 4) != 0 && p[0xE] != 0 && m->bboxNum[0] == 0xFF) {
		const int x1 = m->xMstPos + (int8_t)p[0xC];
		const int y1 = m->yMstPos + (int8_t)p[0xD];
		const int x2 = x1 + p[0xE] - 1;
		const int y2 = y1 + p[0xF] - 1;
		if (mstBoundingBoxCollides2(m->monster1Index, x1, y1, x2, y2) != 0) {
			m->indexUnk49Unk1 = 0;
			m->m49Unk1 = &m->m49->data1[0];
			m->unkAB = 0xFF;
			m->targetLevelPos_x = -1;
			m->targetLevelPos_y = -1;
			mstBoundingBoxClear(m, 1);
			if (p[0xE] != 0) {
				t->flags |= 0x80;
				mstTaskResetMonster1WalkPath(t);
				return 0;
			}
		} else {
			m->bboxNum[0] = mstBoundingBoxUpdate(0xFF, m->monster1Index, x1, y1, x2, y2);
		}
	}
	t->flags &= ~0x80;
	if (m->monsterInfos[946] & 2) {
		if (t->run == &Game::mstTask_monsterWait10) {
			mstMonster1UpdateGoalPosition(m);
			mstMonster1MoveTowardsGoal2(m);
		} else if (t->run == &Game::mstTask_monsterWait8) {
			mstMonster1UpdateGoalPosition(m);
			mstMonster1MoveTowardsGoal1(m);
		} else if (t->run == &Game::mstTask_monsterWait9) {
			mstMonster1MoveTowardsGoal2(m);
		} else { // &Game::mstTask_monsterWait7
			mstMonster1MoveTowardsGoal1(m);
		}
		const MstMovingBoundsUnk1 *m49Unk1 = m->m49Unk1;
		uint8_t xDist, yDist;
		if (_mstLut1[m->goalDirectionMask] & 1) {
			xDist = m49Unk1->unkE;
			yDist = m49Unk1->unkF;
		} else {
			xDist = m49Unk1->unkC;
			yDist = m49Unk1->unkD;
		}
		if (_xMstPos2 < xDist && _yMstPos2 < yDist && !mstMonster1TestGoalDirection(m)) {
			
		} else {
			if (m->goalDirectionMask) {
				return (this->*(t->run))(t);
			}
		}
	} else {
		if (t->run == &Game::mstTask_monsterWait6) {
			if (m->goalScreenNum == 0xFD && m->xMstPos < _mstAndyLevelPosX) {
				m->goalPos_x1 = _mstAndyLevelPosX - m->goalDistance_x2;
				m->goalPos_x2 = _mstAndyLevelPosX - m->goalDistance_x1;
			} else {
				m->goalPos_x1 = _mstAndyLevelPosX + m->goalDistance_x1;
				m->goalPos_x2 = _mstAndyLevelPosX + m->goalDistance_x2;
			}
		}
		mstMonster1SetGoalHorizontal(m);
		bool flag = false;
		while (_xMstPos2 < m->m49Unk1->unkC) {
			if (--m->indexUnk49Unk1 >= 0) {
				m->m49Unk1 = &m->m49->data1[m->indexUnk49Unk1];
			} else {
				flag = true;
				break;
				
			}
		}
		if (!flag) {
			if (m->goalDirectionMask) {
				return (this->*(t->run))(t);
			}
		}
	}
	if (m->monsterInfos[946] & 4) {
		mstBoundingBoxClear(m, 1);
	}
	t->flags |= 0x80;
	mstTaskResetMonster1WalkPath(t);
	return 0;
}

void Game::mstOp52() {
	if (_mstActionNum == -1) {
		return;
	}
	MstMonsterAction *m48 = &_res->_mstMonsterActionData[_mstActionNum];
	for (int i = 0; i < m48->areaCount; ++i) {
		MstMonsterArea *m48Area = &m48->area[i];
		const uint8_t num = m48Area->data->monster1Index;
		if (num != 0xFF) {
			assert(num < kMaxMonsterObjects1);
			MonsterObject1 *m = &_monsterObjects1Table[num];
			mstMonster1ClearChasingMonster(m);
			if ((m->flagsA5 & 0x70) == 0) {
				assert(m->task->monster1 == m);
				Task *t = m->task;
				const int num = m->o16->flags0 & 0xFF;
				if (m->monsterInfos[num * 28] != 0) {
					if (t->run != &Game::mstTask_monsterWait1 && t->run != &Game::mstTask_monsterWait4 && t->run != &Game::mstTask_monsterWait2 && t->run != &Game::mstTask_monsterWait3 && t->run != &Game::mstTask_monsterWait5 && t->run != &Game::mstTask_monsterWait6 && t->run != &Game::mstTask_monsterWait7 && t->run != &Game::mstTask_monsterWait8 && t->run != &Game::mstTask_monsterWait9 && t->run != &Game::mstTask_monsterWait10) {
						m->flagsA5 = (m->flagsA5 & ~0xF) | 6;
						mstTaskInitMonster1Type2(m->task, 1);
					} else {
						m->o16->actionKeyMask = 0;
						m->o16->directionKeyMask = 0;
						t->run = &Game::mstTask_monsterWait11;
					}
				} else {
					m->flagsA5 = (m->flagsA5 & ~0xF) | 6;
					mstTaskInitMonster1Type2(m->task, 1);
				}
			}
		}
	}
	_mstActionNum = -1;
}

bool Game::mstHasMonsterInRange(const MstMonsterAction *m48, uint8_t flag) {
	for (int i = 0; i < 2; ++i) {
		for (uint32_t j = 0; j < m48->count[i]; ++j) {
			uint32_t a = (i ^ flag); // * 32; // vd
			uint32_t n = m48->data1[i][j]; // va
			if (_mstCollisionTable[a][n].count < m48->data2[i][j]) {
				return false;
			}
		}
	}

	uint8_t _op54Data[kMaxMonsterObjects1];
	memset(_op54Data, 0, sizeof(_op54Data));

	int var24 = 0;
	//int var28 = 0;
	int vf = 0;
	for (int i = 0; i < m48->areaCount; ++i) {
		const MstMonsterArea *m12 = &m48->area[i];
		assert(m12->count == 1);
		MstMonsterAreaAction *m12u4 = m12->data;
		if (m12->unk0 != 0) {
			uint8_t var1C = m12u4->unk18;
			if (var1C != 2) {
				vf = var1C;
			}
l1:
			int var4C = vf;

			int var8 = m12u4->xPos;
			int vb = var8; // xPos
			int var4 = m12u4->yPos;
			int vg = var4; // yPos

			int va = vf ^ flag;
			if (va == 1) {
				vb = -vb;
			}
			debug(kDebug_MONSTER, "mstHasMonsterInRange (unk0!=0) count:%d %d %d [%d,%d] screen:%d", m12->count, vb, vg, _mstPosXmin, _mstPosXmax, m12u4->screenNum);
			if (vb >= _mstPosXmin && vb <= _mstPosXmax) {
				uint8_t var4D = _res->_mstMonsterInfos[m12u4->unk0 * kMonsterInfoDataSize + 946] & 2;
				if (var4D == 0 || (vg >= _mstPosYmin && vg <= _mstPosYmax)) {
					MstCollision *varC = &_mstCollisionTable[va][m12u4->unk0];
					vb += _mstAndyLevelPosX;
					const int xLevelPos = vb;
					vg += _mstAndyLevelPosY;
					const int yLevelPos = vg;
					int minDistY = 0x1000000;
					int minDistX = 0x1000000;
					int var34 = -1;
					int var10 = varC->count;
					//MstCollision *var20 = varC;
					for (int j = 0; j < var10; ++j) {
						MonsterObject1 *m = varC->monster1[j];
						if (_op54Data[m->monster1Index] == 0 && (m12u4->screenNum < 0 || m->o16->screenNum == m12u4->screenNum)) {
							int ve = yLevelPos - m->yMstPos;
							int va = ABS(ve);
							int vg = xLevelPos - m->xMstPos;
							int vc = ABS(vg);
							if (vc > m48->unk0 || va > m48->unk2) {
								continue;
							}
							if ((var8 || var4) && m->monsterInfos[944] != 10 && m->monsterInfos[944] != 16 && m->monsterInfos[944] != 9) {
								if (vg <= 0) {
									if (m->levelPosBounds_x1 > xLevelPos) {
										continue;
									}
								} else {
									if (m->levelPosBounds_x2 < xLevelPos) {
										continue;
									}
								}
								if (var4D != 0) { // vertical move
									if (ve <= 0) {
										if (m->levelPosBounds_y1 > yLevelPos) {
											continue;
										}
									} else {
										if (m->levelPosBounds_y2 < yLevelPos) {
											continue;
										}
									}
								}
							}
							if (vc <= minDistX && va <= minDistY) {
								minDistY = va;
								minDistX = vc;
								var34 = j;
							}
						}
					}
					if (var34 != -1) {
						const uint8_t num = varC->monster1[var34]->monster1Index;
						m12u4->monster1Index = num;
						_op54Data[num] = 1;
						debug(kDebug_MONSTER, "monster %d in range", num);
						++var24;
						continue;
					}
				}
			}
			if (var1C != 2 || var4C == 1) {
				return false;
			}
			vf = 1;
			var4C = vf;
			goto l1; 
		}
		//++var28;
	}
	//var28 = vf;
	for (int i = vf; i < m48->areaCount; ++i) {
		MstMonsterArea *m12 = &m48->area[i];
		assert(m12->count == 1);
		MstMonsterAreaAction *m12u4 = m12->data;
		if (m12->unk0 == 0) {
			uint8_t var1C = m12u4->unk18;
			m12u4->monster1Index = 0xFF;
			int var4C = (var1C == 2) ? 0 : var1C;
			int vd = var4C;
l2:
			int var4 = m12u4->xPos;
			int vb = var4;
			int var8 = m12u4->yPos;
			int vg = var8;
			int va = vd ^ flag;
			if (va == 1) {
				vb = -vb;
			}
			debug(kDebug_MONSTER, "mstHasMonsterInRange (unk0==0) count:%d %d %d [%d,%d] screen:%d", m12->count, vb, vg, _mstPosXmin, _mstPosXmax, m12u4->screenNum);
			if (vb >= _mstPosXmin && vb <= _mstPosXmax) {
				uint8_t var4D = _res->_mstMonsterInfos[m12u4->unk0 * kMonsterInfoDataSize + 946] & 2;
				if (var4D == 0 || (vg >= _mstPosYmin && vg <= _mstPosYmax)) {
					MstCollision *varC = &_mstCollisionTable[va][m12u4->unk0];
					vb += _mstAndyLevelPosX;
					const int xLevelPos = vb;
					vg += _mstAndyLevelPosY;
					const int yLevelPos = vg;
					int minDistY = 0x1000000;
					int minDistX = 0x1000000;
					int var34 = -1;
					int var10 = varC->count;
					for (int j = 0; j < var10; ++j) {
						MonsterObject1 *m = varC->monster1[j];
						if (_op54Data[m->monster1Index] == 0 && (m12u4->screenNum < 0 || m->o16->screenNum == m12u4->screenNum)) {
							int ve = yLevelPos - m->yMstPos;
							int va = ABS(ve);
							int vg = xLevelPos - m->xMstPos;
							int vc = ABS(vg);
							if (vc > m48->unk0 || va > m48->unk2) {
								continue;
							}
							if ((var8 || var4) && m->monsterInfos[944] != 10 && m->monsterInfos[944] != 16 && m->monsterInfos[944] != 9) {
								if (vg <= 0) {
									if (m->levelPosBounds_x1 > xLevelPos) {
										continue;
									}
								} else {
									if (m->levelPosBounds_x2 < xLevelPos) {
										continue;
									}
								}
								if (var4D != 0) { // vertical move
									if (ve <= 0) {
										if (m->levelPosBounds_y1 > yLevelPos) {
											continue;
										}
									} else {
										if (m->levelPosBounds_y2 < yLevelPos) {
											continue;
										}
									}
								}
							}
							if (vc <= minDistX && va <= minDistY) {
								minDistY = va;
								minDistX = vc;
								var34 = j;
							}
						}
					}
					if (var34 != -1) {
						const uint8_t num = varC->monster1[var34]->monster1Index;
						m12u4->monster1Index = num;
						_op54Data[num] = 1;
						debug(kDebug_MONSTER, "monster %d in range", num);
						++var24;
						continue;
					}
				}
			}
			if (var1C == 2 && var4C != 1) {
				vd = 1;
				var4C = 1;
				goto l2; 
			}
		}
		//++var28;
	}
	return var24 != 0;
}

void Game::mstOp53(MstMonsterAction *m) {
	if (_mstActionNum != -1) {
		return;
	}
	const int x = MIN(_mstAndyScreenPosX, 255);
	if (_mstAndyScreenPosX < 0) {
		_mstPosXmin = x;
		_mstPosXmax = 255 + x;
	} else {
		_mstPosXmin = -x;
		_mstPosXmax = 255 - x;
	}
	const int y = MIN(_mstAndyScreenPosY, 191);
	if (_mstAndyScreenPosY < 0) {
		_mstPosYmin = y;
		_mstPosYmax = 191 + y;
	} else {
		_mstPosYmin = -y;
		_mstPosYmax = 191 - y;
	}
	mstResetCollisionTable();
	mstUpdateInRange(m);
}

void Game::mstOp54() {
	debug(kDebug_MONSTER, "mstOp54 %d %d %d", _mstActionNum, _m43Num2, _m43Num3);
	if (_mstActionNum != -1) {
		return;
	}
	MstMonsterActionIndex *m43 = 0;
	if (_mstFlags & 0x20000000) {
		if (_m43Num2 == -1) {
			return;
		}
		m43 = &_res->_mstMonsterActionIndexData[_m43Num2];
	} else {
		if (_m43Num3 == -1) {
			return;
		}
		m43 = &_res->_mstMonsterActionIndexData[_m43Num3];
		_m43Num2 = _m43Num1;
	}
	const int x = MIN(_mstAndyScreenPosX, 255);
	if (_mstAndyScreenPosX < 0) {
		_mstPosXmin = x;
		_mstPosXmax = 255 + x;
	} else {
		_mstPosXmin = -x;
		_mstPosXmax = 255 - x;
	}
	const int y = MIN(_mstAndyScreenPosY, 191);
	if (_mstAndyScreenPosY < 0) {
		_mstPosYmin = y;
		_mstPosYmax = 191 + y;
	} else {
		_mstPosYmin = -y;
		_mstPosYmax = 191 - y;
	}
	mstResetCollisionTable();
	if (m43->dataCount == 0) {
		const uint32_t indexUnk48 = m43->indexUnk48[0];
		MstMonsterAction *m48 = &_res->_mstMonsterActionData[indexUnk48];
		mstUpdateInRange(m48);
		if (_mstActionNum == -1) {
			++_mstOp54Counter;
		}
		if (_mstOp54Counter <= 16) {
			return;
		}
		_mstOp54Counter = 0;
		shuffleMstMonsterActionIndex(m43);
	} else {
		memset(_mstOp54Table, 0, sizeof(_mstOp54Table));
		bool var4 = false;
		uint32_t i = 0;
		for (; i < m43->dataCount; ++i) {
			uint8_t num = m43->data[i];
			if ((num & 0x80) == 0) {
				var4 = true;
				if (_mstOp54Table[num] == 0) {
					_mstOp54Table[num] = 1;
					const uint32_t indexUnk48 = m43->indexUnk48[num];
					MstMonsterAction *m48 = &_res->_mstMonsterActionData[indexUnk48];
					if (mstUpdateInRange(m48)) {
						break; 
					}
				}
			}
		}
		if (_mstActionNum != -1) {
			assert(i < m43->dataCount);
			m43->data[i] |= 0x80;
		} else {
			if (var4) {
				++_mstOp54Counter;
				if (_mstOp54Counter <= 16) {
					return;
				}
			}
			_mstOp54Counter = 0;
			if (m43->dataCount != 0) {
				shuffleMstMonsterActionIndex(m43);
			}
		}
	}
}

static uint8_t getLvlObjectFlag(uint8_t type, const LvlObject *o, const LvlObject *andyObject) {
	switch (type) {
	case 0:
		return 0;
	case 1:
		return 1;
	case 2:
		return (o->flags1 >> 4) & 1;
	case 3:
		return ~(o->flags1 >> 4) & 1;
	case 4:
		return (andyObject->flags1 >> 4) & 1;
	case 5:
		return ~(andyObject->flags1 >> 4) & 1;
	default:
		warning("getLvlObjectFlag unhandled type %d", type);
		break;
	}
	return 0;
}

int Game::mstOp56_specialAction(Task *t, int code, int num) {
	assert(num < _res->_mstHdr.op204DataCount);
	const MstOp204Data *op204Data = &_res->_mstOp204Data[num];
	debug(kDebug_MONSTER, "mstOp56_specialAction code %d", code);
	switch (code) {
	case 0:
		if (!_specialAnimFlag && setAndySpecialAnimation(0x71) != 0) {
			_plasmaCannonFlags |= 1;
			if (_andyObject->spriteNum == 0) {
				_mstCurrentAnim = op204Data->arg0 & 0xFFFF;
			} else {
				_mstCurrentAnim = op204Data->arg0 >> 16;
			}
			LvlObject *o = 0;
			if (t->monster2) {
				o = t->monster2->o;
			} else if (t->monster1) {
				o = t->monster1->o16;
			}
			if (op204Data->arg3 != 6 && o) {
				LvlObject *tmpObject = t->monster1->o16;
				const uint8_t flags = getLvlObjectFlag(op204Data->arg3 & 255, tmpObject, _andyObject);
				_specialAnimMask = ((flags & 3) << 4) | (_specialAnimMask & ~0x30);
				// _specialAnimScreenNum = tmpObject->screenNum;
				_specialAnimLvlObject = tmpObject;
				_mstOriginPosX = op204Data->arg1 & 0xFFFF;
				_mstOriginPosY = op204Data->arg2 & 0xFFFF;
			} else {
				_specialAnimMask = merge_bits(_specialAnimMask, _andyObject->flags1, 0x30); // _specialAnimMask ^= (_specialAnimMask ^ _andyObject->flags1) & 0x30;
				// _specialAnimScreenNum = _andyObject->screenNum;
				_specialAnimLvlObject = _andyObject;
				_mstOriginPosX = _andyObject->posTable[3].x - _andyObject->posTable[6].x;
				_mstOriginPosY = _andyObject->posTable[3].y - _andyObject->posTable[6].y;
			}
			_specialAnimFlag = true;
		}
		if (_mstAndyRectNum != 0xFF) {
			_mstBoundingBoxesTable[_mstAndyRectNum].monster1Index = 0xFF;
		}
		break;
	case 1:
		if (!_specialAnimFlag) {
			break;
		}
		if (setAndySpecialAnimation(0x61) != 0) {
			_plasmaCannonFlags &= ~1;
			if (_andyObject->spriteNum == 0) {
				_mstCurrentAnim = op204Data->arg0 & 0xFFFF;
			} else {
				_mstCurrentAnim = op204Data->arg0 >> 16;
			}
			LvlObject *o = 0;
			if (t->monster2) {
				o = t->monster2->o;
			} else if (t->monster1) {
				o = t->monster1->o16;
			}
			if (op204Data->arg3 != 6 && o) {
				LvlObject *tmpObject = t->monster1->o16;
				const uint8_t flags = getLvlObjectFlag(op204Data->arg3 & 255, tmpObject, _andyObject);
				_specialAnimMask = ((flags & 3) << 4) | (_specialAnimMask & 0xFFCF);
				// _specialAnimScreenNum = tmpObject->screenNum;
				_specialAnimLvlObject = tmpObject;
				_mstOriginPosX = op204Data->arg1 & 0xFFFF;
				_mstOriginPosY = op204Data->arg2 & 0xFFFF;
			} else {
				_specialAnimMask = merge_bits(_specialAnimMask, _andyObject->flags1, 0x30); // _specialAnimMask ^= (_specialAnimMask ^ _andyObject->flags1) & 0x30;
				// _specialAnimScreenNum = _andyObject->screenNum;
				_specialAnimLvlObject = _andyObject;
				_mstOriginPosX = _andyObject->posTable[3].x - _andyObject->posTable[6].x;
				_mstOriginPosY = _andyObject->posTable[3].y - _andyObject->posTable[6].y;
			}
			_specialAnimFlag = false;
		}
		_mstAndyRectNum = mstBoundingBoxUpdate(_mstAndyRectNum, 0xFE, _mstAndyLevelPosX, _mstAndyLevelPosY, _mstAndyLevelPosX + _andyObject->width - 1, _mstAndyLevelPosY + _andyObject->height - 1) & 0xFF;
		break;
	case 2: {
			LvlObject *o = t->monster1->o16;
			const uint8_t flags = getLvlObjectFlag(op204Data->arg0 & 255, o, _andyObject);
			setAndySpecialAnimation(flags | 0x10);
		}
		break;
	case 3:
		setAndySpecialAnimation(0x12);
		break;
	case 4:
		setAndySpecialAnimation(0x80);
		break;
	case 5: // game over, restart level
		setAndySpecialAnimation(0xA4);
		break;
	case 6:
		setAndySpecialAnimation(0xA3);
		break;
	case 7:
		setAndySpecialAnimation(0x05);
		break;
	case 8:
		setAndySpecialAnimation(0xA1);
		break;
	case 9:
		setAndySpecialAnimation(0xA2);
		break;
	case 10:
		if (op204Data->arg0 == 1) {
			setShakeScreen(2, op204Data->arg1 & 255);
		} else if (op204Data->arg0 == 2) {
			setShakeScreen(1, op204Data->arg1 & 255);
		} else {
			setShakeScreen(3, op204Data->arg1 & 255);
		}
		break;
	case 11: {
			MonsterObject2 *m = t->monster2;
			const int type = op204Data->arg3;
			m->x1 = getTaskVar(t, op204Data->arg0, (type >> 0xC) & 15);
			m->y1 = getTaskVar(t, op204Data->arg1, (type >> 0x8) & 15);
			m->x2 = getTaskVar(t, op204Data->arg2, (type >> 0x4) & 15);
			m->y2 = getTaskVar(t, type >> 16     ,  type         & 15);
		}
		break;
	case 12: {
			const int type1 = ((op204Data->arg3 >> 4) & 15);
			const int hint  = getTaskVar(t, op204Data->arg0, type1);
			const int type2 = (op204Data->arg3 & 15);
			const int pause = getTaskVar(t, op204Data->arg1, type2);
			displayHintScreen(hint, pause);
		}
		break;
	case 13:
	case 14:
	case 22:
	case 23:
	case 24:
	case 25: {
			const int mask = op204Data->arg3;
			int xPos = getTaskVar(t, op204Data->arg0, (mask >> 8) & 15);
			int yPos = getTaskVar(t, op204Data->arg1, (mask >> 4) & 15);
			int screenNum = getTaskVar(t, op204Data->arg2, mask & 15);
			LvlObject *o = 0;
			if (t->monster2) {
				o = t->monster2->o;
			} else if (t->monster1) {
				o = t->monster1->o16;
			}
			if (screenNum < 0) {
				if (screenNum == -2) {
					if (!o) {
						break;
					}
					screenNum = o->screenNum;
					if (t->monster2) {
						xPos += t->monster2->xMstPos;
						yPos += t->monster2->yMstPos;
					} else if (t->monster1) {
						xPos += t->monster1->xMstPos;
						yPos += t->monster1->yMstPos;
					} else {
						break;
					}
				} else if (screenNum == -1) {
					xPos += _mstAndyLevelPosX;
					yPos += _mstAndyLevelPosY;
					screenNum = _currentScreen;
				} else {
					if (!o) {
						break;
					}
					xPos += o->posTable[6].x - o->posTable[7].x;
					yPos += o->posTable[6].y - o->posTable[7].y;
					if (t->monster2) {
						xPos += t->monster2->xMstPos;
						yPos += t->monster2->yMstPos;
					} else {
						assert(t->monster1);
						xPos += t->monster1->xMstPos;
						yPos += t->monster1->yMstPos;
					}
				}
			} else {
				if (screenNum >= _res->_mstHdr.screensCount) {
					screenNum = _res->_mstHdr.screensCount - 1;
				}
				xPos += _res->_mstPointOffsets[screenNum].xOffset;
				yPos += _res->_mstPointOffsets[screenNum].yOffset;
			}
			if (code == 13) {
				assert(o);
				xPos -= _res->_mstPointOffsets[screenNum].xOffset;
				xPos -= o->posTable[7].x;
				yPos -= _res->_mstPointOffsets[screenNum].yOffset;
				yPos -= o->posTable[7].y;
				o->screenNum = screenNum;
				o->xPos = xPos;
				o->yPos = yPos;
				setLvlObjectPosInScreenGrid(o, 7);
				if (t->monster2) {
					mstTaskSetMonster2ScreenPosition(t);
				} else {
					assert(t->monster1);
					mstTaskUpdateScreenPosition(t);
				}
			} else if (code == 14) {
				const int pos = mask >> 16;
				assert(pos < 8);
				xPos -= _res->_mstPointOffsets[screenNum].xOffset;
				xPos -= _andyObject->posTable[pos].x;
				yPos -= _res->_mstPointOffsets[screenNum].yOffset;
				yPos -= _andyObject->posTable[pos].y;
				_andyObject->screenNum = screenNum;
				_andyObject->xPos = xPos;
				_andyObject->yPos = yPos;
				updateLvlObjectScreen(_andyObject);
				mstUpdateRefPos();
				mstUpdateMonstersRect();
			} else if (code == 22) {
				updateScreenMaskBuffer(xPos, yPos, 1);
			} else if (code == 24) {
				updateScreenMaskBuffer(xPos, yPos, 2);
			} else if (code == 25) {
				updateScreenMaskBuffer(xPos, yPos, 3);
			} else {
				assert(code == 23);
				updateScreenMaskBuffer(xPos, yPos, 0);
			}
		}
		break;
	case 15: {
			_andyObject->anim  = op204Data->arg0;
			_andyObject->frame = op204Data->arg1;
			LvlObject *o = 0;
			if (t->monster2) {
				o = t->monster2->o;
			} else if (t->monster1) {
				o = t->monster1->o16;
			} else {
				o = _andyObject;
			}
			const uint8_t flags = getLvlObjectFlag(op204Data->arg2 & 255, o, _andyObject);
			_andyObject->flags1 = ((flags & 3) << 4) | (_andyObject->flags1 & 0xFFCF);
			const int x3 = _andyObject->posTable[3].x;
			const int y3 = _andyObject->posTable[3].y;
			setupLvlObjectBitmap(_andyObject);
			_andyObject->xPos += (x3 - _andyObject->posTable[3].x);
			_andyObject->yPos += (y3 - _andyObject->posTable[3].y);
			updateLvlObjectScreen(o);
			mstUpdateRefPos();
			mstUpdateMonstersRect();
		}
		break;
	case 16:
	case 17: {
			LvlObject *o = _andyObject;
			if (code == 16) {
				if (t->monster2) {
					o = t->monster2->o;
				} else if (t->monster1) {
					o = t->monster1->o16;
				}
			}
			const int pos = op204Data->arg2;
			assert(pos < 8);
			const int xPos = o->xPos + o->posTable[pos].x;
			const int yPos = o->yPos + o->posTable[pos].y;
			const int type1  = (op204Data->arg3 >> 4) & 15;
			const int index1 = op204Data->arg0;
			setTaskVar(t, index1, type1, xPos);
			const int type2  = op204Data->arg3 & 15;
			const int index2 = op204Data->arg1;
			setTaskVar(t, index2, type2, yPos);
		}
		break;
	case 18: {
			_mstCurrentActionKeyMask = op204Data->arg0 & 255;
		}
		break;
	case 19: {
			_andyActionKeyMaskAnd    = op204Data->arg0 & 255;
			_andyActionKeyMaskOr     = op204Data->arg1 & 255;
			_andyDirectionKeyMaskAnd = op204Data->arg2 & 255;
			_andyDirectionKeyMaskOr  = op204Data->arg3 & 255;
		}
		break;
	case 20: {
			_mstCurrentActionKeyMask = 0;
			t->monster1->flagsA6 |= 2;
			t->run = &Game::mstTask_idle;
			t->monster1->o16->actionKeyMask = _mstCurrentActionKeyMask;
			t->monster1->o16->directionKeyMask = _andyObject->directionKeyMask;
			return 1;
		}
		break;
	case 21: {
			t->monster1->flagsA6 &= ~2;
			t->monster1->o16->actionKeyMask = 0;
			t->monster1->o16->directionKeyMask = 0;
		}
		break;
	case 26: {
			int screenNum = op204Data->arg2;
			if (screenNum < -1 && !t->monster1) {
				break;
			}
			if (screenNum == -1) {
				screenNum = _currentScreen;
			} else if (screenNum < -1) {
				screenNum = t->monster1->o16->screenNum;
			}
			if (screenNum >= _res->_mstHdr.screensCount) {
				screenNum = _res->_mstHdr.screensCount - 1;
			}
			const int x = _res->_mstPointOffsets[screenNum].xOffset;
			const int y = _res->_mstPointOffsets[screenNum].yOffset;
			const int xOffset = op204Data->arg3 * 256;
			int count = 0;
			for (int i = 0; i < kMaxMonsterObjects1; ++i) {
				const MonsterObject1 *m = &_monsterObjects1Table[i];
				if (!m->m46) {
					continue;
				}
				if (!rect_contains(x - xOffset, y, x + xOffset + 256, y + 192, m->xMstPos, m->yMstPos)) {
					continue;
				}
				const int num = op204Data->arg1;
				switch (op204Data->arg0) {
				case 0:
					if (m->m46 == &_res->_mstBehaviorData[num]) {
						++count;
					}
					break;
				case 1:
					if (m->monsterInfos == &_res->_mstMonsterInfos[num * kMonsterInfoDataSize]) {
						++count;
					}
					break;
				case 2:
					if (m->monsterInfos[944] == num) {
						++count;
					}
					break;
				}
			}
			_mstOp56Counter = count;
		}
		break;
	case 27: {
			const int type = op204Data->arg3;
			int a = getTaskVar(t, op204Data->arg0, (type >> 0xC) & 15);
			int b = getTaskVar(t, op204Data->arg1, (type >> 0x8) & 15);
			int c = getTaskVar(t, op204Data->arg2, (type >> 0x4) & 15);
			int d = getTaskVar(t, type >> 16,       type         & 15);
			setScreenMaskRect(a - 16, b, a + 16, c, d);
		}
		break;
	case 28:
		// no-op
		break;
	case 29: {
			const uint8_t state  = op204Data->arg1 & 255;
			const uint8_t screen = op204Data->arg0 & 255;
			_res->_screensState[screen].s0 = state;
		}
		break;
	case 30:
		++_level->_checkpoint;
		break;
	default:
		warning("Unhandled opcode %d in mstOp56_specialAction", code);
		break;
	}
	return 0;
}

static void initWormHoleSprite(WormHoleSprite *s, const uint8_t *p) {
	s->screenNum = p[0];
	s->initData1 = p[1];
	s->xPos = p[2];
	s->yPos = p[3];
	s->initData4 = READ_LE_UINT32(p + 4);
	s->rect1_x1 = p[8];
	s->rect1_y1 = p[9];
	s->rect1_x2 = p[0xA];
	s->rect1_y2 = p[0xB];
	s->rect2_x1 = p[0xC];
	s->rect2_y1 = p[0xD];
	s->rect2_x2 = p[0xE];
	s->rect2_y2 = p[0xF];
}

void Game::mstOp57_addWormHoleSprite(int x, int y, int screenNum) {
	bool found = false;
	int spriteNum = 0;
	for (int i = 0; i < 6; ++i) {
		if (_wormHoleSpritesTable[i].screenNum == screenNum) {
			found = true;
			break;
		}
		if (_wormHoleSpritesTable[i].screenNum == 0xFF) {
			break;
		}
		++spriteNum;
	}
	if (!found) {
		found = true;
		if (spriteNum == 6) {
			++_wormHoleSpritesCount;
			if (_wormHoleSpritesCount >= spriteNum) {
				_wormHoleSpritesCount = 0;
				spriteNum = 0;
			} else {
				spriteNum = _wormHoleSpritesCount - 1;
			}
		} else {
			spriteNum = _wormHoleSpritesCount;
		}
		switch (_currentLevel) {
		case 2:
			initWormHoleSprite(&_wormHoleSpritesTable[spriteNum], _pwr1_spritesData + screenNum * 16);
			break;
		case 3:
			initWormHoleSprite(&_wormHoleSpritesTable[spriteNum], _isld_spritesData + screenNum * 16);
			break;
		case 4:
			initWormHoleSprite(&_wormHoleSpritesTable[spriteNum], _lava_spritesData + screenNum * 16);
			break;
		case 6:
			initWormHoleSprite(&_wormHoleSpritesTable[spriteNum], _lar1_spritesData + screenNum * 16);
			break;
		default:
			warning("mstOp57 unhandled level %d", _currentLevel);
			return;
		}
	}
	WormHoleSprite *boulderWormSprite = &_wormHoleSpritesTable[spriteNum];
	const int dx = x - boulderWormSprite->xPos;
	const int dy = y + 15 - boulderWormSprite->yPos;
	spriteNum = _rnd._rndSeed & 3;
	if (spriteNum == 0) {
		spriteNum = 1;
	}
	static const uint8_t data[32] = {
		0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x04, 0x04, 0x04, 0x06, 0x06, 0x06, 0x08, 0x08, 0x08, 0x0A,
		0x0A, 0x0A, 0x0C, 0x0C, 0x0C, 0x0E, 0x0E, 0x0E, 0x10, 0x10, 0x10, 0x12, 0x12, 0x12, 0x14, 0x14,
	};
	const int pos = data[(dx >> 3) & 31];
	const int num = dy >> 5;
	if ((boulderWormSprite->flags[num] & (3 << pos)) == 0) {
		if (addLvlObjectToList3(20)) {
			LvlObject *o = _lvlObjectsList3;
			o->flags0 = _andyObject->flags0;
			o->flags1 = _andyObject->flags1;
			o->screenNum = screenNum;
			o->flags2 = 0x1007;
			o->anim = 0;
			o->frame = 0;
			setupLvlObjectBitmap(o);
			setLvlObjectPosRelativeToPoint(o, 7, x, y);
		}
		boulderWormSprite->flags[num] |= (spriteNum << pos);
	}
}

void Game::mstOp58_addLvlObject(Task *t, int num) {
	const MstOp211Data *dat = &_res->_mstOp211Data[num];
	const int mask = dat->maskVars;
	int xPos = getTaskVar(t, dat->indexVar1, (mask >> 8) & 15); // vb
	int yPos = getTaskVar(t, dat->indexVar2, (mask >> 4) & 15); // ve
	const uint8_t type = getTaskVar(t, dat->indexVar3, mask & 15); // va
	LvlObject *o = 0;
	if (t->monster2) {
		o = t->monster2->o;
	} else if (t->monster1) {
		o = t->monster1->o16;
	}
	uint8_t screen = type;
	if (type == 0xFB) { // -5
		if (!o) {
			return;
		}
		xPos += o->xPos + o->posTable[6].x;
		yPos += o->yPos + o->posTable[6].y;
		screen = o->screenNum;
	} else if (type == 0xFE) { // -2
		if (!o) {
			return;
		}
		xPos += o->xPos + o->posTable[7].x;
		yPos += o->yPos + o->posTable[7].y;
		screen = o->screenNum;
	} else if (type == 0xFF) { // -1
		xPos += _mstAndyScreenPosX; // vb
		yPos += _mstAndyScreenPosY; // ve
		screen = _currentScreen;
	}
	const uint16_t flags = (dat->unk6 == -1 && o) ? o->flags2 : 0x3001;
	o = addLvlObject(2, xPos, yPos, screen, dat->unk8, dat->unk4, dat->unkB, flags, dat->unk9, dat->unkA);
	if (o) {
		o->dataPtr = 0;
	}
}

void Game::mstOp59_addShootSpecialPowers(int x, int y, int screenNum, int state, uint16_t flags) {
	LvlObject *o = addLvlObjectToList0(3);
	if (o) {
		o->dataPtr = _shootLvlObjectDataNextPtr;
		if (_shootLvlObjectDataNextPtr) {
			_shootLvlObjectDataNextPtr = _shootLvlObjectDataNextPtr->nextPtr;
			memset(o->dataPtr, 0, sizeof(ShootLvlObjectData));
		}
		ShootLvlObjectData *s = (ShootLvlObjectData *)o->dataPtr;
		assert(s);
		o->callbackFuncPtr = &Game::lvlObjectSpecialPowersCallback;
		s->state = state;
		s->type = 0;
		s->counter = 17;
		s->dxPos = (int8_t)_specialPowersDxDyTable[state * 2];
		s->dyPos = (int8_t)_specialPowersDxDyTable[state * 2 + 1];
		static const uint8_t data[16] = {
			0x0D, 0x00, 0x0C, 0x01, 0x0C, 0x03, 0x0C, 0x00, 0x0C, 0x02, 0x0D, 0x01, 0x0B, 0x00, 0x0B, 0x02,
		};
		assert(state < 8);
		o->anim = data[state * 2];
		o->flags1 = ((data[state * 2 + 1] & 3) << 4) | (o->flags1 & ~0x0030);
		o->frame = 0;
		o->flags2 = flags;
		o->screenNum = screenNum;
		setupLvlObjectBitmap(o);
		setLvlObjectPosRelativeToPoint(o, 6, x, y);
	}
}

void Game::mstOp59_addShootFireball(int x, int y, int screenNum, int type, int state, uint16_t flags) {
	LvlObject *o = addLvlObjectToList2(7);
	if (o) {
		o->dataPtr = _shootLvlObjectDataNextPtr;
		if (_shootLvlObjectDataNextPtr) {
			_shootLvlObjectDataNextPtr = _shootLvlObjectDataNextPtr->nextPtr;
			memset(o->dataPtr, 0, sizeof(ShootLvlObjectData));
		}
		ShootLvlObjectData *s = (ShootLvlObjectData *)o->dataPtr;
		assert(s);
		s->state = state;
		static const uint8_t fireballDxDy1[16] = {
			0x0A, 0x00, 0xF7, 0xFA, 0xF7, 0x06, 0x09, 0xFA, 0x09, 0x06, 0xF6, 0x00, 0x00, 0xF6, 0x00, 0x0A
		};
		static const uint8_t fireballDxDy2[16] = {
			0x0D, 0x00, 0xF5, 0xF9, 0xF5, 0x07, 0x0B, 0xF9, 0x0B, 0x07, 0xF3, 0x00, 0x00, 0xF3, 0x00, 0x0D
		};
		static const uint8_t data1[16] = {
			0x02, 0x00, 0x01, 0x01, 0x01, 0x03, 0x01, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00, 0x00, 0x00, 0x02
		};
		static const uint8_t data2[16] = {
			0x0D, 0x00, 0x0D, 0x01, 0x0D, 0x03, 0x0D, 0x00, 0x0D, 0x02, 0x0D, 0x01, 0x0D, 0x00, 0x0D, 0x02
		};
		assert(state < 8);
		const uint8_t *anim;
		if (type >= 7) {
			s->dxPos = (int8_t)fireballDxDy2[state * 2];
			s->dyPos = (int8_t)fireballDxDy2[state * 2 + 1];
			s->counter = 33;
			anim = &data2[state * 2];
		} else {
			s->dxPos = (int8_t)fireballDxDy1[state * 2];
			s->dyPos = (int8_t)fireballDxDy1[state * 2 + 1];
			s->counter = 39;
			anim = &data1[state * 2];
		}
		s->type = type;
		o->anim = anim[0];
		o->screenNum = screenNum;
		o->flags1 = ((anim[1] & 3) << 4) | (o->flags1 & ~0x0030);
		o->flags2 = flags;
		o->frame = 0;
		setupLvlObjectBitmap(o);
		setLvlObjectPosRelativeToPoint(o, 6, x - s->dxPos, y - s->dyPos);
	}
}

void Game::mstTaskResetMonster1WalkPath(Task *t) {
	MonsterObject1 *m = t->monster1;
	t->run = &Game::mstTask_main;
	m->o16->actionKeyMask = 0;
	m->o16->directionKeyMask = 0;
	if ((m->flagsA5 & 4) != 0 && (m->flagsA5 & 0x28) == 0) {
		switch (m->flagsA5 & 7) {
		case 5:
			m->flagsA5 = (m->flagsA5 & ~6) | 1;
			if (!mstMonster1UpdateWalkPath(m)) {
				mstMonster1ResetWalkPath(m);
			}
			mstTaskSetNextWalkCode(t);
			break;
		case 6:
			m->flagsA5 &= ~7;
			if (!mstSetCurrentPos(m, m->xMstPos, m->yMstPos)) {
				m->flagsA5 |= 1;
				if (!mstMonster1UpdateWalkPath(m)) {
					mstMonster1ResetWalkPath(m);
				}
				const uint32_t indexWalkCode = m->walkNode->walkCodeStage1;
				if (indexWalkCode != kNone) {
					m->walkCode = &_res->_mstWalkCodeData[indexWalkCode];
				}
			} else {
				m->flagsA5 |= 2;
				if (!mstMonster1UpdateWalkPath(m)) {
					mstMonster1ResetWalkPath(m);
				}
			}
			mstTaskSetNextWalkCode(t);
			break;
		}
	} else {
		m->flagsA5 &= ~4;
		mstMonster1UpdateWalkPath(m);
	}
}

bool Game::mstSetCurrentPos(MonsterObject1 *m, int x, int y) {
	_mstCurrentPosX = x;
	_mstCurrentPosY = y;
	const uint8_t *ptr = m->monsterInfos;
	const int32_t a = READ_LE_UINT32(ptr + 900);
	int x1 = _mstAndyLevelPosX - a; // vc
	int x2 = _mstAndyLevelPosX + a; // vf
	if (ptr[946] & 2) { // horizontal and vertical
		int y1 = _mstAndyLevelPosY - a; // vb
		int y2 = _mstAndyLevelPosY + a; // ve
		if (x > x1 && x < x2 && y > y1 && y < y2) {
			if (ABS(x - _mstAndyLevelPosX) > ABS(y - _mstAndyLevelPosY)) {
				if (x >= _mstAndyLevelPosX) {
					_mstCurrentPosX = x2;
				} else {
					_mstCurrentPosX = x1;
				}
			} else {
				if (y >= _mstAndyLevelPosY) {
					_mstCurrentPosY = y2;
				} else {
					_mstCurrentPosY = y1;
				}
			}
			return false;
		}
		const int32_t b = READ_LE_UINT32(ptr + 896);
		x1 -= b;
		x2 += b;
		y1 -= b;
		y2 += b;
		if (x < x1) {
			_mstCurrentPosX = x1;
		} else if (x > x2) {
			_mstCurrentPosX = x2;
		}
		if (y < y1) {
			_mstCurrentPosY = y1;
		} else if (y > y2) {
			_mstCurrentPosY = y2;
		}
		return (_mstCurrentPosX == x && _mstCurrentPosY == y);
	} // horizontal only
	if (x > x1 && x < x2) {
		if (x >= _mstAndyLevelPosX) {
			_mstCurrentPosX = x2;
		} else {
			_mstCurrentPosX = x1;
		}
		return false;
	}
	const int32_t b = READ_LE_UINT32(ptr + 896);
	x1 -= b;
	x2 += b;
	if (x < x1) {
		_mstCurrentPosX = x1;
		return false;
	} else if (x > x2) {
		_mstCurrentPosX = x2;
		return false;
	}
	return true;
}

void Game::mstMonster1SetGoalHorizontal(MonsterObject1 *m) {
	Task *t = m->task;
	t->flags &= ~0x80;
	int x = m->xMstPos;
	if (x < m->goalPos_x1) {
		_xMstPos1 = x = m->goalPos_x2;
		if ((m->flagsA5 & 2) != 0 && (m->flags48 & 8) != 0 && x > m->goalPosBounds_x2) {
			t->flags |= 0x80;
			x = m->goalPosBounds_x2;
		}
		if (x > m->levelPosBounds_x2) {
			t->flags |= 0x80;
			x = m->levelPosBounds_x2;
		}
		_xMstPos2 = x - m->xMstPos;
		m->goalDirectionMask = kDirectionKeyMaskRight;
	} else if (x > m->goalPos_x2) {
		_xMstPos1 = x = m->goalPos_x1;
		if ((m->flagsA5 & 2) != 0 && (m->flags48 & 8) != 0 && x < m->goalPosBounds_x1) {
			t->flags |= 0x80;
			x = m->goalPosBounds_x1;
		}
		if (x < m->levelPosBounds_x1) {
			t->flags |= 0x80;
			x = m->levelPosBounds_x1;
		}
		_xMstPos2 = m->xMstPos - x;
		m->goalDirectionMask = kDirectionKeyMaskLeft;
	} else {
		_xMstPos1 = x;
		_xMstPos2 = 0;
		m->goalDirectionMask = 0;
	}
}

void Game::mstResetCollisionTable() {
	const int count = MIN(_res->_mstHdr.infoMonster1Count, 32);
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < count; ++j) {
			_mstCollisionTable[i][j].count = 0;
		}
	}
	for (int i = 0; i < kMaxMonsterObjects1; ++i) {
		MonsterObject1 *m = &_monsterObjects1Table[i];
		if (!m->m46) {
			continue;
		}
		const uint8_t _bl = m->flagsA5;
		if ((_bl & 2) != 0 || ((_bl & 1) != 0 && ((m->walkNode->walkCodeStage1 == kNone && !m->walkCode) || (m->walkNode->walkCodeStage1 != kNone && m->walkCode == &_res->_mstWalkCodeData[m->walkNode->walkCodeStage1])))) {
			if ((_bl & 0xB0) != 0) {
				continue;
			}
			const int num = m->o16->flags0 & 0xFF;
			if (m->monsterInfos[num * 28] != 0) {
				continue;
			}
			if (m->task->run == &Game::mstTask_monsterWait4) {
				continue;
			}
			uint8_t _al = m->flagsA6;
			if (_al & 2) {
				continue;
			}
			assert(m->task->monster1 == m);
			if ((_al & 8) == 0 || m->monsterInfos[945] != 0) {
				const uint32_t offset = m->monsterInfos - _res->_mstMonsterInfos;
				assert(offset % kMonsterInfoDataSize == 0);
				const uint32_t num = offset / kMonsterInfoDataSize;
				assert(num < 32);
				const int dir = (m->xMstPos < _mstAndyLevelPosX) ? 1 : 0;
				const int count = _mstCollisionTable[dir][num].count;
				_mstCollisionTable[dir][num].monster1[count] = m;
				++_mstCollisionTable[dir][num].count;
			}
		}
	}
}

// resume bytecode execution
void Game::mstTaskRestart(Task *t) {
	t->run = &Game::mstTask_main;
	LvlObject *o = 0;
	if (t->monster1) {
		o = t->monster1->o16;
	} else if (t->monster2) {
		o = t->monster2->o;
	}
	if (o) {
		o->actionKeyMask = 0;
		o->directionKeyMask = 0;
	}
}

bool Game::mstMonster1CheckLevelBounds(MonsterObject1 *m, int x, int y, uint8_t dir) {
	if ((m->flagsA5 & 2) != 0 && (m->flags48 & 8) != 0 && !mstSetCurrentPos(m, x, y)) {
		return true;
	}
	if ((dir & kDirectionKeyMaskLeft) != 0 && x < m->levelPosBounds_x1) {
		return true;
	}
	if ((dir & kDirectionKeyMaskRight) != 0 && x > m->levelPosBounds_x2) {
		return true;
	}
	if ((dir & kDirectionKeyMaskUp) != 0 && y < m->levelPosBounds_y1) {
		return true;
	}
	if ((dir & kDirectionKeyMaskDown) != 0 && y > m->levelPosBounds_y2) {
		return true;
	}
	return false;
}

int Game::mstTaskResetMonster1Direction(Task *t) {
	MonsterObject1 *m = t->monster1;
	m->flagsA5 = (m->flagsA5 & ~0xF) | 6;
	return mstTaskInitMonster1Type2(t, 1);
}

int Game::mstTaskInitMonster1Type1(Task *t) {
	t->flags &= ~0x80;
	MonsterObject1 *m = t->monster1;
	m->flagsA5 = (m->flagsA5 & ~2) | 5;
	mstMonster1ResetWalkPath(m);
	const uint32_t indexWalkBox = m->walkNode->walkBox;
	const MstWalkBox *m34 = &_res->_mstWalkBoxData[indexWalkBox];
	int y = 0;
	int x = 0;
	bool flag = false;
	if (m->monsterInfos[946] & 2) {
		m->unkC0 = -1;
		m->unkBC = -1;
		m->walkBoxNum = 0xFF;
		m->targetDirectionMask = 0xFF;
		y = m34->top;
		if (m->yMstPos < m34->top || m->yMstPos > m34->bottom) {
			flag = true;
		}
	}
	x = m34->left;
	if (m->monsterInfos[946] & 4) {
		m->unkAB = 0xFF;
		m->targetLevelPos_x = -1;
		m->targetLevelPos_y = -1;
		mstBoundingBoxClear(m, 1);
	}
	if (!flag && m->xMstPos >= x && m->xMstPos <= m34->right) {
		mstTaskResetMonster1WalkPath(t);
		return 0;
	}
	const uint32_t indexUnk36 = m->walkNode->movingBoundsIndex1;
	MstMovingBoundsIndex *m36 = &_res->_mstMovingBoundsIndexData[indexUnk36];
	MstMovingBounds *m49 = &_res->_mstMovingBoundsData[m36->indexUnk49];
	m->m49 = m49;
	m->indexUnk49Unk1 = m36->unk4;
	if (m->indexUnk49Unk1 < 0) {
		if (m49->indexDataCount == 0) {
			m->indexUnk49Unk1 = 0;
		} else {
			m->indexUnk49Unk1 = m49->indexData[_rnd.getMstNextNumber(m->rnd_m49)];
		}
	}
	assert((uint32_t)m->indexUnk49Unk1 < m49->count1);
	m->m49Unk1 = &m49->data1[m->indexUnk49Unk1];
	int xDelta = (m34->right - x) / 4; // vf
	m->goalDistance_x1 = x + xDelta;
	m->goalDistance_x2 = m34->right - xDelta;
	if (xDelta != 0) {
		xDelta = _rnd.update() % xDelta;
	}
	m->goalDistance_x1 += xDelta;
	m->goalDistance_x2 -= xDelta;
	m->goalPos_x1 = m->goalDistance_x1;
	m->goalPos_x2 = m->goalDistance_x2;
	const uint8_t *ptr = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
	if ((ptr[2] & kDirectionKeyMaskHorizontal) == 0) {
		m->goalDistance_x1 = m->goalPos_x1 = m->goalDistance_x2 = m->goalPos_x2 = m->xMstPos;
	}
	int vf;
	if (m->monsterInfos[946] & 2) {
		int yDelta = (m34->bottom - y) / 4; // vf
		m->goalDistance_y1 = y + yDelta;
		m->goalDistance_y2 = m34->bottom - yDelta;
		if (yDelta != 0) {
			yDelta = _rnd.update() % yDelta;
		}
		m->goalDistance_y1 += yDelta;
		m->goalDistance_y2 -= yDelta;
		m->goalPos_y1 = m->goalDistance_y1;
		m->goalPos_y2 = m->goalDistance_y2;
		if ((ptr[2] & kDirectionKeyMaskVertical) == 0) {
			m->goalDistance_y1 = m->goalPos_y1 = m->goalDistance_y2 = m->goalPos_y2 = m->yMstPos;
		}
		if (m->monsterInfos[946] & 4) {
			mstMonster1MoveTowardsGoal2(m);
		} else {
			mstMonster1MoveTowardsGoal1(m);
		}
		vf = 1;
		const MstMovingBoundsUnk1 *m49Unk1 = m->m49Unk1;
		uint8_t xDist, yDist;
		if (_mstLut1[m->goalDirectionMask] & 1) {
			xDist = m49Unk1->unkE;
			yDist = m49Unk1->unkF;
		} else {
			xDist = m49Unk1->unkC;
			yDist = m49Unk1->unkD;
		}
		if (_xMstPos2 < xDist && _yMstPos2 < yDist && !mstMonster1TestGoalDirection(m)) {
			vf = 0;
		}
	} else {
		mstMonster1SetGoalHorizontal(m);
		vf = 1;
		while (_xMstPos2 < m->m49Unk1->unkC) {
			if (--m->indexUnk49Unk1 >= 0) {
				m->m49Unk1 = &m->m49->data1[m->indexUnk49Unk1];
			} else {
				vf = 0;
				break;
			}
		}
	}
	if (_xMstPos2 <= 0 && ((m->monsterInfos[946] & 2) == 0 || _yMstPos2 <= 0)) {
		if (m->monsterInfos[946] & 4) {
			mstBoundingBoxClear(m, 1);
		}
		mstTaskResetMonster1WalkPath(t);
		return 0;
	}
	if (vf != 0 && ((_xMstPos2 >= m->m49->unk14 || ((m->monsterInfos[946] & 2) != 0 && _yMstPos2 >= m->m49->unk15)))) {
		const uint8_t *p = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
		if ((m->monsterInfos[946] & 4) != 0 && p[0xE] != 0 && m->bboxNum[0] == 0xFF) {
			const int x1 = m->xMstPos + (int8_t)p[0xC];
			const int y1 = m->yMstPos + (int8_t)p[0xD];
			const int x2 = x1 + p[0xE] - 1;
			const int y2 = y1 + p[0xF] - 1;
			if (mstBoundingBoxCollides2(m->monster1Index, x1, y1, x2, y2) != 0) {
				m->indexUnk49Unk1 = 0;
				m->m49Unk1 = &m->m49->data1[0];
				m->unkAB = 0xFF;
				m->targetLevelPos_x = -1;
				m->targetLevelPos_y = -1;
				mstBoundingBoxClear(m, 1);
				if (p[0xE] != 0) {
					t->flags |= 0x80;
					mstTaskResetMonster1WalkPath(t);
					return 0;
				}
			} else {
				m->bboxNum[0] = mstBoundingBoxUpdate(0xFF, m->monster1Index, x1, y1, x2, y2);
			}
		}
		if (_xMstPos2 >= m36->unk8 || ((m->monsterInfos[946] & 2) != 0 && _yMstPos2 >= m36->unk8)) {
			m->indexUnk49Unk1 = m->m49->count1 - 1;
			m->m49Unk1 = &m->m49->data1[m->indexUnk49Unk1];
			if (m->monsterInfos[946] & 4) {
				m->unkAB = 0xFF;
				m->targetLevelPos_x = -1;
				m->targetLevelPos_y = -1;
				mstBoundingBoxClear(m, 1);
			}
		}
		if (m->monsterInfos[946] & 4) {
			t->run = &Game::mstTask_monsterWait9;
		} else if (m->monsterInfos[946] & 2) {
			t->run = &Game::mstTask_monsterWait7;
		} else {
			t->run = &Game::mstTask_monsterWait5;
		}
		return (this->*(t->run))(t);
	} else if (m->monsterInfos[946] & 4) {
		mstBoundingBoxClear(m, 1);
	}
	t->flags |= 0x80;
	mstTaskResetMonster1WalkPath(t);
	return -1;
}

int Game::mstTaskInitMonster1Type2(Task *t, int flag) {
	debug(kDebug_MONSTER, "mstTaskInitMonster1Type2 t %p flag %d", t, flag);
	t->flags &= ~0x80;
	MonsterObject1 *m = t->monster1;
	m->flagsA5 = (m->flagsA5 & ~1) | 6;
	mstMonster1ResetWalkPath(m);
	const uint32_t indexUnk36 = m->walkNode->movingBoundsIndex2;
	MstMovingBoundsIndex *m36 = &_res->_mstMovingBoundsIndexData[indexUnk36];
	MstMovingBounds *m49 = &_res->_mstMovingBoundsData[m36->indexUnk49];
	m->m49 = m49;
	if (flag != 0) {
		m->indexUnk49Unk1 = m49->count1 - 1;
	} else {
		m->indexUnk49Unk1 = m36->unk4;
	}
	if (m->indexUnk49Unk1 < 0) {
		if (m49->indexDataCount == 0) {
			m->indexUnk49Unk1 = 0;
		} else {
			m->indexUnk49Unk1 = m49->indexData[_rnd.getMstNextNumber(m->rnd_m49)];
		}
	}
	assert((uint32_t)m->indexUnk49Unk1 < m49->count1);
	m->m49Unk1 = &m49->data1[m->indexUnk49Unk1];
	m->goalScreenNum = 0xFD;
	m->unkC0 = -1;
	m->unkBC = -1;
	m->walkBoxNum = 0xFF;
	m->targetDirectionMask = 0xFF;
	if (mstSetCurrentPos(m, m->xMstPos, m->yMstPos)) {
		mstTaskResetMonster1WalkPath(t);
		return 0;
	}
	int vf;
	const uint8_t *p = m->monsterInfos;
	if (p[946] & 2) {
		m->unkE4 = 255;
		if (p[946] & 4) {
			m->unkAB = 0xFF;
			m->targetLevelPos_x = -1;
			m->targetLevelPos_y = -1;
			mstBoundingBoxClear(m, 1);
		}
		m->goalDistance_x1 = READ_LE_UINT32(m->monsterInfos + 900);
		m->goalDistance_x2 = m->goalDistance_x1 + READ_LE_UINT32(m->monsterInfos + 896);
		m->goalDistance_y1 = READ_LE_UINT32(m->monsterInfos + 900);
		m->goalDistance_y2 = m->goalDistance_y1 + READ_LE_UINT32(m->monsterInfos + 896);
		m->goalPos_x1 = m->goalDistance_x1;
		m->goalPos_x2 = m->goalDistance_x2;
		m->goalPos_y1 = m->goalDistance_y1;
		m->goalPos_y2 = m->goalDistance_y2;
		const uint8_t *ptr1 = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
		if ((ptr1[2] & kDirectionKeyMaskVertical) == 0) {
			m->goalDistance_y1 = m->goalPos_y1 = m->goalDistance_y2 = m->goalPos_y2 = m->yMstPos;
		}
		if ((ptr1[2] & kDirectionKeyMaskHorizontal) == 0) {
			m->goalDistance_x1 = m->goalPos_x1 = m->goalDistance_x2 = m->goalPos_x2 = m->xMstPos;
		}
		if (p[946] & 4) {
			mstMonster1UpdateGoalPosition(m);
			mstMonster1MoveTowardsGoal2(m);
		} else {
			mstMonster1UpdateGoalPosition(m);
			mstMonster1MoveTowardsGoal1(m);
		}
		vf = 1;
		const MstMovingBoundsUnk1 *m49Unk1 = m->m49Unk1;
		uint8_t xDist, yDist;
		if (_mstLut1[m->goalDirectionMask] & 1) {
			xDist = m49Unk1->unkE;
			yDist = m49Unk1->unkF;
		} else {
			xDist = m49Unk1->unkC;
			yDist = m49Unk1->unkD;
		}
		if (_xMstPos2 < xDist && _yMstPos2 < yDist && !mstMonster1TestGoalDirection(m)) {
			vf = 0;
		}
	} else {
		int32_t ve = READ_LE_UINT32(p + 900);
		int32_t vc = READ_LE_UINT32(p + 896);
		int r = vc / 4;
		m->goalDistance_x1 = ve + r;
		m->goalDistance_x2 = vc + ve - r;
		if (r != 0) {
			r = _rnd.update() % r;
		}
		m->goalDistance_x1 += r;
		m->goalDistance_x2 -= r;
		if (m->goalScreenNum == 0xFD && m->xMstPos < _mstAndyLevelPosX) {
			m->goalPos_x1 = _mstAndyLevelPosX - m->goalDistance_x2;
			m->goalPos_x2 = _mstAndyLevelPosX - m->goalDistance_x1;
		} else {
			m->goalPos_x1 = _mstAndyLevelPosX + m->goalDistance_x1;
			m->goalPos_x2 = _mstAndyLevelPosX + m->goalDistance_x2;
		}
		mstMonster1SetGoalHorizontal(m);
		vf = 1;
		while (_xMstPos2 < m->m49Unk1->unkC) {
			if (--m->indexUnk49Unk1 >= 0) {
				m->m49Unk1 = &m->m49->data1[m->indexUnk49Unk1];
			} else {
				vf = 0;
				break;
			}
		}
	}
	if (_xMstPos2 <= 0 && ((m->monsterInfos[946] & 2) == 0 || _yMstPos2 <= 0)) {
		if (m->monsterInfos[946] & 4) {
			mstBoundingBoxClear(m, 1);
		}
		mstTaskResetMonster1WalkPath(t);
		return 0;
	}
	if (vf) {
		if (_xMstPos2 >= m->m49->unk14 || ((m->monsterInfos[946] & 2) != 0 && _yMstPos2 >= m->m49->unk15)) {
			const uint8_t *p = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
			if ((m->monsterInfos[946] & 4) != 0 && p[0xE] != 0 && m->bboxNum[0] == 0xFF) {
				const int x1 = m->xMstPos + (int8_t)p[0xC];
				const int y1 = m->yMstPos + (int8_t)p[0xD];
				const int x2 = x1 + p[0xE] - 1;
				const int y2 = y1 + p[0xF] - 1;
				if (mstBoundingBoxCollides2(m->monster1Index, x1, y1, x2, y2) != 0) {
					m->indexUnk49Unk1 = 0;
					m->m49Unk1 = &m->m49->data1[0];
					m->unkAB = 0xFF;
					m->targetLevelPos_x = -1;
					m->targetLevelPos_y = -1;
					mstBoundingBoxClear(m, 1);
					if (p[0xE] != 0) {
						t->flags |= 0x80;
						mstTaskResetMonster1WalkPath(t);
						return 0;
					}
				} else {
					m->bboxNum[0] = mstBoundingBoxUpdate(0xFF, m->monster1Index, x1, y1, x2, y2);
				}
			}
			if (_xMstPos2 >= m36->unk8 || ((m->monsterInfos[946] & 2) != 0 && _yMstPos2 >= m36->unk8)) {
				m->indexUnk49Unk1 = m->m49->count1 - 1;
				m->m49Unk1 = &m->m49->data1[m->indexUnk49Unk1];
				if (m->monsterInfos[946] & 4) {
					m->unkAB = 0xFF;
					m->targetLevelPos_x = -1;
					m->targetLevelPos_y = -1;
					mstBoundingBoxClear(m, 1);
				}
			}
			if (m->monsterInfos[946] & 4) {
				t->run = &Game::mstTask_monsterWait10;
			} else if (m->monsterInfos[946] & 2) {
				t->run = &Game::mstTask_monsterWait8;
			} else {
				t->run = &Game::mstTask_monsterWait6;
			}
			return (this->*(t->run))(t);
		}
	}
	if (m->monsterInfos[946] & 4) {
		mstBoundingBoxClear(m, 1);
	}
	t->flags |= 0x80;
	mstTaskResetMonster1WalkPath(t);
	return -1;
}

void Game::mstOp67_addMonster(Task *currentTask, int x1, int x2, int y1, int y2, int screen, int type, int o_flags1, int o_flags2, int arg1C, int arg20, int arg24) {
	debug(kDebug_MONSTER, "mstOp67_addMonster pos %d,%d,%d,%d %d %d 0x%x 0x%x %d %d %d", y1, x1, y2, x2, screen, type, o_flags1, o_flags2, arg1C, arg20, arg24);
	if (o_flags2 == 0xFFFF) {
		LvlObject *o = 0;
		if (currentTask->monster1) {
			o = currentTask->monster1->o16;
		} else if (currentTask->monster2) {
			o = currentTask->monster2->o;
		}
		o_flags2 = o ? o->flags2 : 0x3001;
	}
	if (y1 != y2) {
		y1 += _rnd.update() % ABS(y2 - y1 + 1);
	}
	if (x1 != x2) {
		x1 += _rnd.update() % ABS(x2 - x1 + 1);
	}
	int objScreen = (screen < 0) ? _currentScreen : screen;

	LvlObject *o = 0; // vf
	MonsterObject2 *mo = 0; // ve
	MonsterObject1 *m = 0; // vg

	if (arg1C != -128) {
		if (_mstVars[30] > kMaxMonsterObjects1) {
			_mstVars[30] = kMaxMonsterObjects1;
		}
		int count = 0;
		for (int i = 0; i < kMaxMonsterObjects1; ++i) {
			if (_monsterObjects1Table[i].m46) {
				++count;
			}
		}
		if (count >= _mstVars[30]) {
			return;
		}
		if (arg1C < 0) { // vd
			const MstBehaviorIndex *m42 = &_res->_mstBehaviorIndexData[arg24];
			if (m42->dataCount == 0) {
				arg1C = m42->data[0];
			} else {
				arg1C = m42->data[_rnd.update() % m42->dataCount];
			}
		}
		for (int i = 0; i < kMaxMonsterObjects1; ++i) {
			if (!_monsterObjects1Table[i].m46) {
				m = &_monsterObjects1Table[i];
				break;
			}
		}
		if (!m) {
			warning("mstOp67 unable to find a free MonsterObject1");
			return;
		}
		memset(m->localVars, 0, sizeof(m->localVars));
		m->flags48 = 0x1C;
		m->flagsA5 = 0;
		m->collideDistance = -1;
		m->walkCode = 0;
		m->flagsA6 = 0;

		assert((uint32_t)arg1C < _res->_mstBehaviorIndexData[arg24].count1);
		const uint32_t indexBehavior = _res->_mstBehaviorIndexData[arg24].behavior[arg1C];
		MstBehavior *m46 = &_res->_mstBehaviorData[indexBehavior];
		m->m46 = m46;
		assert((uint32_t)arg20 < m46->count);
		MstBehaviorState *behaviorState = &m46->data[arg20]; // vc
		m->behaviorState = behaviorState;
		m->monsterInfos = _res->_mstMonsterInfos + behaviorState->indexMonsterInfo * kMonsterInfoDataSize;

		m->localVars[7] = behaviorState->energy;

		if (behaviorState->indexUnk51 == kNone) {
			m->flags48 &= ~4;
		}

		const uint8_t *ptr = m->monsterInfos;
		o = addLvlObject(ptr[945], x1, y1, objScreen, ptr[944], behaviorState->anim, o_flags1, o_flags2, 0, 0);
		if (!o) {
			mstMonster1ResetData(m);
			return;
		}
		m->o16 = o;
		if (_currentLevel == kLvl_lar2 && m->monsterInfos[944] == 26) { // Master of Darkness
			m->o20 = addLvlObject(ptr[945], x1, y1, objScreen, ptr[944], behaviorState->anim + 1, o_flags1, 0x3001, 0, 0);
			if (!m->o20) {
				warning("mstOp67 failed to addLvlObject in kLvl_lar2");
				mstMonster1ResetData(m);
				return;
			}
			if (screen < 0) {
				m->o20->xPos += _mstAndyScreenPosX;
				m->o20->yPos += _mstAndyScreenPosY;
			}
			m->o20->dataPtr = 0;
			setLvlObjectPosRelativeToObject(m->o16, 6, m->o20, 6);
		}
		m->o_flags2 = o_flags2 & 0xFFFF;
		m->lut4Index = _mstLut5[o_flags2 & 0x1F];
		o->dataPtr = m;
	} else {
		for (int i = 0; i < kMaxMonsterObjects2; ++i) {
			if (!_monsterObjects2Table[i].monster2Info) {
				mo = &_monsterObjects2Table[i];
				break;
			}
		}
		if (!mo) {
			warning("mstOp67 no free monster2");
			return;
		}
		assert(arg24 >= 0 && arg24 < _res->_mstHdr.infoMonster2Count);
		mo->monster2Info = &_res->_mstInfoMonster2Data[arg24];
		if (currentTask->monster1) {
			mo->monster1 = currentTask->monster1;
		} else if (currentTask->monster2) {
			mo->monster1 = currentTask->monster2->monster1;
		} else {
			mo->monster1 = 0;
		}

		mo->flags24 = 0;

		uint8_t _cl  = mo->monster2Info->type;
		uint16_t anim = mo->monster2Info->anim;

		o = addLvlObject((_cl >> 7) & 1, x1, y1, objScreen, (_cl & 0x7F), anim, o_flags1, o_flags2, 0, 0);
		if (!o) {
			mo->monster2Info = 0;
			if (mo->o) {
				mo->o->dataPtr = 0;
			}
			return;
		}
		mo->o = o;
		o->dataPtr = mo;
	}
	if (screen < 0) {
		o->xPos += _mstAndyScreenPosX;
		o->yPos += _mstAndyScreenPosY;
	}
	setLvlObjectPosInScreenGrid(o, 7);
	if (mo) {
		Task *t = findFreeTask();
		if (!t) {
			mo->monster2Info = 0;
			if (mo->o) {
				mo->o->dataPtr = 0;
			}
			removeLvlObject2(o);
			return;
		}
		memset(t, 0, sizeof(Task));
		resetTask(t, kUndefinedMonsterByteCode);
		t->monster2 = mo;
		t->monster1 = 0;
		mo->task = t;
		t->codeData = 0;
		appendTask(&_monsterObjects2TasksList, t);
		t->codeData = kUndefinedMonsterByteCode;
		mstTaskSetMonster2ScreenPosition(t);
		const uint32_t codeData = mo->monster2Info->codeData;
		assert(codeData != kNone);
		resetTask(t, _res->_mstCodeData + codeData * 4);
		if (_currentLevel == kLvl_fort && mo->monster2Info->type == 27) {
			mstMonster2InitFirefly(mo);
		}
	} else {
		Task *t = findFreeTask();
		if (!t) {
			mstMonster1ResetData(m);
			removeLvlObject2(o);
			return;
		}
		memset(t, 0, sizeof(Task));
		resetTask(t, kUndefinedMonsterByteCode);
		t->monster1 = m;
		t->monster2 = 0;
		m->task = t;
		t->codeData = 0;
		appendTask(&_monsterObjects1TasksList, t);
		t->codeData = kUndefinedMonsterByteCode;
		_rnd.resetMst(m->rnd_m35);
		_rnd.resetMst(m->rnd_m49);

		m->levelPosBounds_x1 = -1;
		MstBehaviorState *behaviorState = m->behaviorState;
		m->walkNode = _res->_mstWalkPathData[behaviorState->walkPath].data;

		if (m->monsterInfos[946] & 4) {
			m->bboxNum[0] = 0xFF;
			m->bboxNum[1] = 0xFF;
		}
		mstTaskUpdateScreenPosition(t);
		switch (type) {
		case 1:
			mstTaskInitMonster1Type1(t);
			assert(t->run != &Game::mstTask_main || (t->codeData && t->codeData != kUndefinedMonsterByteCode));
			break;
		case 2:
			if (m) {
				m->flagsA6 |= 1;
			}
			mstTaskInitMonster1Type2(t, 0);
			assert(t->run != &Game::mstTask_main || (t->codeData && t->codeData != kUndefinedMonsterByteCode));
			break;
		default:
			m->flagsA5 = 1;
			if (!mstMonster1UpdateWalkPath(m)) {
				mstMonster1ResetWalkPath(m);
			}
			mstTaskSetNextWalkCode(t);
			break;
		}
	}
	currentTask->flags &= ~0x80;
}

void Game::mstOp68_addMonsterGroup(Task *t, const uint8_t *p, int a, int b, int c, int d) {
	const MstBehaviorIndex *m42 = &_res->_mstBehaviorIndexData[d];
	struct {
		int m42Index;
		int m46Index;
	} data[16];
	int count = 0;
	for (uint32_t i = 0; i < m42->count1; ++i) {
		MstBehavior *m46 = &_res->_mstBehaviorData[m42->behavior[i]];
		for (uint32_t j = 0; j < m46->count; ++j) {
			MstBehaviorState *behaviorState = &m46->data[j];
			uint32_t indexMonsterInfo = p - _res->_mstMonsterInfos;
			assert((indexMonsterInfo % kMonsterInfoDataSize) == 0);
			indexMonsterInfo /= kMonsterInfoDataSize;
			if (behaviorState->indexMonsterInfo == indexMonsterInfo) {
				assert(count < 16);
				data[count].m42Index = i;
				data[count].m46Index = j;
				++count;
			}
		}
	}
	if (count == 0) {
		return;
	}
	int j = 0;
	for (int i = 0; i < a; ++i) {
		mstOp67_addMonster(t, _mstOp67_x1, _mstOp67_x2, _mstOp67_y1, _mstOp67_y2, _mstOp67_screenNum, _mstOp67_type, _mstOp67_flags1, _mstOp68_flags1, data[j].m42Index, data[j].m46Index, d);
		if (--c == 0) {
			return;
		}
		if (++j >= count) {
			j = 0;
		}
	}
	for (int i = 0; i < b; ++i) {
		mstOp67_addMonster(t, _mstOp68_x1, _mstOp68_x2, _mstOp68_y1, _mstOp68_y2, _mstOp68_screenNum, _mstOp68_type, _mstOp68_arg9, _mstOp68_flags1, data[j].m42Index, data[j].m46Index, d);
		if (--c == 0) {
			return;
		}
		if (++j >= count) {
			j = 0;
		}
	}
}

int Game::mstTask_wait1(Task *t) {
	debug(kDebug_MONSTER, "mstTask_wait1 t %p count %d", t, t->arg1);
	--t->arg1;
	if (t->arg1 == 0) {
		t->run = &Game::mstTask_main;
		return 0;
	}
	return 1;
}

int Game::mstTask_wait2(Task *t) {
	debug(kDebug_MONSTER, "mstTask_wait2 t %p count %d", t, t->arg1);
	--t->arg1;
	if (t->arg1 == 0) {
		mstTaskRestart(t);
		return 0;
	}
	return 1;
}

int Game::mstTask_wait3(Task *t) {
	debug(kDebug_MONSTER, "mstTask_wait3 t %p type %d flag %d", t, t->arg1, t->arg2);
	if (getTaskFlag(t, t->arg2, t->arg1) == 0) {
		return 1;
	}
	mstTaskRestart(t);
	return 0;
}

int Game::mstTask_idle(Task *t) {
	debug(kDebug_MONSTER, "mstTask_idle t %p", t);
	return 1;
}

int Game::mstTask_mstOp231(Task *t) {
	debug(kDebug_MONSTER, "mstTask_mstOp231 t %p", t);
	const MstOp234Data *m = &_res->_mstOp234Data[t->arg2];
	const int a = getTaskFlag(t, m->indexVar1, m->maskVars & 15);
	const int b = getTaskFlag(t, m->indexVar2, m->maskVars >> 4);
	if (!compareOp(m->compare, a, b)) {
		mstTaskRestart(t);
		return 0;
	}
	return 1;
}

int Game::mstTask_mstOp232(Task *t) {
	warning("mstTask_mstOp232 unimplemented");
	t->run = &Game::mstTask_main;
	return 0;
}

int Game::mstTask_mstOp233(Task *t) {
	debug(kDebug_MONSTER, "mstTask_mstOp233 t %p", t);
	const MstOp234Data *m = &_res->_mstOp234Data[t->arg2];
	const int a = getTaskVar(t, m->indexVar1, m->maskVars & 15);
	const int b = getTaskVar(t, m->indexVar2, m->maskVars >> 4);
	if (!compareOp(m->compare, a, b)) {
		mstTaskRestart(t);
		return 0;
	}
	return 1;
}

int Game::mstTask_mstOp234(Task *t) {
	debug(kDebug_MONSTER, "mstTask_mstOp234 t %p", t);
	const MstOp234Data *m = &_res->_mstOp234Data[t->arg2];
	const int a = getTaskVar(t, m->indexVar1, m->maskVars & 15);
	const int b = getTaskVar(t, m->indexVar2, m->maskVars >> 4);
	if (compareOp(m->compare, a, b)) {
		mstTaskRestart(t);
		return 0;
	}
	return 1;
}

int Game::mstTask_monsterWait1(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait1 t %p", t);
	if (t->arg1 == 0) {
		mstMonster1UpdateWalkPath(t->monster1);
		mstTaskRestart(t);
		return 0;
	}
	--t->arg1;
	return 1;
}

int Game::mstTask_monsterWait2(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait2 t %p", t);
	MonsterObject1 *m = t->monster1;
	const uint16_t flags0 = m->o16->flags0;
	if ((flags0 & 0x100) != 0 && (flags0 & 0xFF) == m->o_flags0) {
		mstMonster1UpdateWalkPath(t->monster1);
		mstTaskRestart(t);
		return 0;
	}
	return 1;
}

int Game::mstTask_monsterWait3(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait3 t %p", t);
	MonsterObject1 *m = t->monster1;
	const uint16_t flags0 = m->o16->flags0;
	if ((flags0 & 0xFF) == m->o_flags0) {
		if (t->arg1 > 0) {
			t->run = &Game::mstTask_monsterWait1;
		} else {
			t->run = &Game::mstTask_monsterWait2;
		}
		return (this->*(t->run))(t);
	}
	return 1;
}

int Game::mstTask_monsterWait4(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait4 t %p", t);
	MonsterObject1 *m = t->monster1;
	const uint32_t offset = m->monsterInfos - _res->_mstMonsterInfos;
	assert(offset % kMonsterInfoDataSize == 0);
	const uint32_t num = offset / kMonsterInfoDataSize;
	if (t->arg2 != num) {
		mstMonster1UpdateWalkPath(m);
		mstTaskRestart(t);
		return 0;
	}
	return 1;
}

int Game::mstTask_monsterWait5(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait5 t %p", t);
	// horizontal move
	MonsterObject1 *m = t->monster1;
	mstMonster1SetGoalHorizontal(m);
	if (_xMstPos2 < m->m49Unk1->unk8) {
		if (_xMstPos2 > 0) {
			while (--m->indexUnk49Unk1 >= 0) {
				m->m49Unk1 = &m->m49->data1[m->indexUnk49Unk1];
				if (_xMstPos2 >= m->m49Unk1->unkC) {
					goto set_am;
				}
			}
		}
		return mstTaskStopMonster1(t, m);
	}
set_am:
	const uint8_t *ptr = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
	mstLvlObjectSetActionDirection(m->o16, ptr, ptr[3], m->goalDirectionMask);
	return 1;
}

int Game::mstTask_monsterWait6(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait6 t %p", t);
	MonsterObject1 *m = t->monster1;
	// horizontal move with goal
	if (m->goalScreenNum == 0xFD && m->xMstPos < _mstAndyLevelPosX) {
		m->goalPos_x1 = _mstAndyLevelPosX - m->goalDistance_x2;
		m->goalPos_x2 = _mstAndyLevelPosX - m->goalDistance_x1;
	} else {
		m->goalPos_x1 = _mstAndyLevelPosX + m->goalDistance_x1;
		m->goalPos_x2 = _mstAndyLevelPosX + m->goalDistance_x2;
	}
	mstMonster1SetGoalHorizontal(m);
	if (_xMstPos2 < m->m49Unk1->unk8) {
		if (_xMstPos2 > 0) {
			while (--m->indexUnk49Unk1 >= 0) {
				m->m49Unk1 = &m->m49->data1[m->indexUnk49Unk1];
				if (_xMstPos2 >= m->m49Unk1->unkC) {
					goto set_am;
				}
			}
		}
		return mstTaskStopMonster1(t, m);
	}
set_am:
	const uint8_t *ptr = _res->_mstMonsterInfos + m->m49Unk1->offsetMonsterInfo;
	mstLvlObjectSetActionDirection(m->o16, ptr, ptr[3], m->goalDirectionMask);
	return 1;
}

int Game::mstTask_monsterWait7(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait7 t %p", t);
	MonsterObject1 *m = t->monster1;
	mstMonster1MoveTowardsGoal1(m);
	return mstTaskUpdatePositionActionDirection(t, m);
}

int Game::mstTask_monsterWait8(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait8 t %p", t);
	MonsterObject1 *m = t->monster1;
	mstMonster1UpdateGoalPosition(m);
	mstMonster1MoveTowardsGoal1(m);
	return mstTaskUpdatePositionActionDirection(t, m);
}

int Game::mstTask_monsterWait9(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait9 t %p", t);
	MonsterObject1 *m = t->monster1;
	mstMonster1MoveTowardsGoal2(m);
	return mstTaskUpdatePositionActionDirection(t, m);
}

int Game::mstTask_monsterWait10(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait10 t %p", t);
	MonsterObject1 *m = t->monster1;
	mstMonster1UpdateGoalPosition(m);
	mstMonster1MoveTowardsGoal2(m);
	return mstTaskUpdatePositionActionDirection(t, m);
}

int Game::mstTask_monsterWait11(Task *t) {
	debug(kDebug_MONSTER, "mstTask_monsterWait11 t %p", t);
	MonsterObject1 *m = t->monster1;
	const int num = m->o16->flags0 & 0xFF;
	if (m->monsterInfos[num * 28] == 0) {
		mstTaskResetMonster1Direction(t);
	}
	return 1;
}
