
// lava_hod - "river of fire"

#include "game.h"
#include "level.h"
#include "paf.h"
#include "util.h"
#include "video.h"

static const CheckpointData _lava_checkpointData[6] = {
	{ 114,  54, 0x300c,   2,  0,  2 },
	{  12, 121, 0x300c, 232,  3,  2 },
	{  -5,  96, 0x300c,  39,  5,  2 },
	{ 131, 112, 0x300c,  39,  7,  2 },
	{   8, 105, 0x300c, 232, 11,  2 },
	{  19, 112, 0x700c,  39, 13,  2 }
};

static const uint8_t _lava_screenStartData[40] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct Level_lava: Level {
	virtual const CheckpointData *getCheckpointData(int num) const { return &_lava_checkpointData[num]; }
	virtual const uint8_t *getScreenRestartData() const { return _lava_screenStartData; }
	virtual void initialize();
	virtual void terminate();
	virtual void tick();
	virtual void preScreenUpdate(int screenNum);
	virtual void postScreenUpdate(int screenNum);
	virtual void setupScreenCheckpoint(int screenNum);

	void postScreenUpdate_lava_screen0();
	void postScreenUpdate_lava_screen1();
	void postScreenUpdate_lava_screen2_updateMask(int x, int y, int h, int unk, int screenNum, const uint8_t *p);
	void postScreenUpdate_lava_screen2_addLvlObjects(const uint8_t *p);
	void postScreenUpdate_lava_screen2();
	void postScreenUpdate_lava_screen3_updateMask(int x, int y, int w, int h, int screenNum, uint8_t mask);
	void postScreenUpdate_lava_screen3_updatePlatform(LvlObject *o, BoundingBox *b);
	void postScreenUpdate_lava_screen3();
	void postScreenUpdate_lava_screen4();
	void postScreenUpdate_lava_screen5();
	void postScreenUpdate_lava_screen6();
	void postScreenUpdate_lava_screen7();
	void postScreenUpdate_lava_screen8();
	void postScreenUpdate_lava_screen10();
	void postScreenUpdate_lava_screen11();
	void postScreenUpdate_lava_screen12();
	void postScreenUpdate_lava_screen13();
	void postScreenUpdate_lava_screen14();
	void postScreenUpdate_lava_screen15();

	void preScreenUpdate_lava_screen0();
	void preScreenUpdate_lava_screen3();
	void preScreenUpdate_lava_screen6();
	void preScreenUpdate_lava_screen10();
	void preScreenUpdate_lava_screen13();
	void preScreenUpdate_lava_screen15();

	void setupScreenCheckpoint_lava_screen3();

	uint8_t _screen1Counter;
	uint8_t _screen2Counter;
};

Level *Level_lava_create() {
	return new Level_lava;
}

static LvlObject *findLvlObject_lava(LvlObject *o) {
	LvlObject *cur = o->nextPtr;
	while (cur) {
		if (o->type == cur->type && o->spriteNum == cur->spriteNum && o->screenNum == cur->screenNum) {
			return cur;
		}
		cur = cur->nextPtr;
	}
	return 0;
}

void Level_lava::postScreenUpdate_lava_screen0() {
	switch (_res->_screensState[0].s0) {
	case 2:
		++_screenCounterTable[0];
		if (_screenCounterTable[0] >= 11) {
			_res->_screensState[0].s0 = 1;
		}
		break;
	case 0:
		if (_andyObject->anim == 2 && _andyObject->frame > 2) {
			_res->_screensState[0].s0 = 2;
		}
		break;
	}
}

void Level_lava::postScreenUpdate_lava_screen1() {
	BoundingBox b;
	b.x1 = _andyObject->xPos + _andyObject->posTable[4].x;
	b.y1 = _andyObject->yPos + _andyObject->posTable[4].y;
	b.x2 = _andyObject->xPos + _andyObject->posTable[5].x;
	b.y2 = _andyObject->yPos + _andyObject->posTable[5].y;
	BoundingBox b2 = { 48, 156, 79, 167 };
	if (_res->_currentScreenResourceNum == 1 && (_andyObject->flags0 & 0x1F) != 2 && _g->clipBoundingBox(&b, &b2)) {
		LvlObject *o = _g->findLvlObject2(0, 0, 1);
		if (o) {
			o->objectUpdateType = 7;
		}
		_screen1Counter = 51;
	} else {
		if (_screen1Counter != 0) {
			--_screen1Counter;
		}
	}
}

void Level_lava::postScreenUpdate_lava_screen2_updateMask(int x, int y, int h, int flag, int screenNum, const uint8_t *p) {
	if (x < 0) {
		x = 0;
	}
	if (y < 0) {
		y = 0;
	}
	uint32_t maskOffset = Game::screenMaskOffset(_res->_screensBasePos[screenNum].u + x, _res->_screensBasePos[screenNum].v + y);
	uint8_t *dst = _g->_screenMaskBuffer + maskOffset;
	if (flag < 0) {
		h >>= 3;
		const int count = -flag;
		for (int i = 0; i < h; ++i) {
			memset(dst + (int8_t)p[0], 0, count); p += 2;
			dst -= 512;
		}
	} else {
		h >>= 3;
		const int count = flag;
		for (int i = 0; i < h; ++i) {
			memset(dst + (int8_t)p[0], p[1], count); p += 2;
			dst -= 512;
		}
	}
}

void Level_lava::postScreenUpdate_lava_screen2_addLvlObjects(const uint8_t *p) {
	do {
		LvlObject *ptr = 0;
		if (_g->_declaredLvlObjectsListCount < Game::kMaxLvlObjects) {
			ptr = _g->_declaredLvlObjectsNextPtr;
			_g->_declaredLvlObjectsNextPtr = _g->_declaredLvlObjectsNextPtr->nextPtr;
			++_g->_declaredLvlObjectsListCount;
			ptr->spriteNum = 22;
			ptr->type = 8;
			_res->incLvlSpriteDataRefCounter(ptr);
			_g->lvlObjectTypeCallback(ptr);
			ptr->currentSprite = 0;
			ptr->sssObject = 0;
			ptr->nextPtr = 0;
		}
		if (ptr) {
			ptr->nextPtr = _g->_lvlObjectsList3;
			_g->_lvlObjectsList3 = ptr;
			ptr->callbackFuncPtr = &Game::lvlObjectList3Callback;
			ptr->screenNum = _res->_currentScreenResourceNum;
			const uint16_t flags2 = READ_LE_UINT16(p + 4);
			ptr->flags2 = flags2;
			ptr->flags1 = (ptr->flags1 & ~0x30) | ((flags2 >> 10) & 0x30);
			ptr->anim = READ_LE_UINT16(p + 2);
			ptr->frame = 0;
			_g->setupLvlObjectBitmap(ptr);
			_g->setLvlObjectPosRelativeToPoint(ptr, 7, p[0], p[1]);
		}
		p += 6;
	} while (READ_LE_UINT16(p + 4) != 0xFFFF);
}

void Level_lava::postScreenUpdate_lava_screen2() {
	BoundingBox b;
	b.x1 = _andyObject->xPos + _andyObject->posTable[4].x;
	b.y1 = _andyObject->yPos + _andyObject->posTable[4].y;
	b.x2 = _andyObject->xPos + _andyObject->posTable[5].x;
	b.y2 = _andyObject->yPos + _andyObject->posTable[5].y;
	BoundingBox b2 = { 40, 156, 72, 165 };
	if (_res->_currentScreenResourceNum == 2 && (_andyObject->flags0 & 0x1F) != 2 && _g->clipBoundingBox(&b, &b2)) {
		LvlObject *o = _g->findLvlObject2(0, 0, 2);
		if (o) {
			o->objectUpdateType = 7;
		}
		_screen2Counter = 13;
	} else {
		if (_screen2Counter != 0) {
			--_screen2Counter;
		}
	}
	LvlObject *o = _g->findLvlObject(2, 0, 2);
	assert(o);
	if (_screen2Counter == 0) {
		o->directionKeyMask = 4;
	} else {
		o->directionKeyMask = 1;
	}
	static const uint8_t maskData1[3 * 8] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02,
		0x00, 0x02, 0x00, 0x02, 0x01, 0x02, 0x01, 0x02,
		0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
	};
	postScreenUpdate_lava_screen2_updateMask(o->xPos + o->posTable[7].x, o->yPos + o->height - 1, o->height - 1, -3, o->screenNum, maskData1);
	if (_res->_currentScreenResourceNum <= 2 && (o->flags0 & 0x1F) == 2) {
		_g->setShakeScreen(2, 2);
	}
	if (o->levelData0x2988) {
		_g->updateAndyObject(o);
	}
	postScreenUpdate_lava_screen2_updateMask(o->xPos + o->posTable[7].x, o->yPos + o->height - 1, o->height - 1, 3, o->screenNum, maskData1);
	if (_res->_currentScreenResourceNum == 2 && (o->flags0 & 0x1F) == 1) {
		if ((o->flags0 & 0xE0) == 0x20) {
			static const uint8_t data[] = {
				0x75, 0xAA, 0x06, 0, 0x0F, 0x90,
				0x65, 0x9E, 0x09, 0, 0x0F, 0x10,
				0x80, 0x9E, 0x08, 0, 0x09, 0x10,
				0xFF, 0xFF, 0xFF, 0, 0xFF, 0xFF
			};
			postScreenUpdate_lava_screen2_addLvlObjects(data);
		}
	}
	o = _g->findLvlObject(2, 1, 2);
	assert(o);
	if (_screen1Counter == 0) {
		o->directionKeyMask = 1;
	} else {
		o->directionKeyMask = 4;
	}
	static const uint8_t maskData2[5 * 8] = {
		0x00, 0x00, 0x00, 0x01, 0xFF, 0x02, 0xFF, 0x02,
		0xFF, 0x02, 0xFF, 0x02, 0xFF, 0x02, 0xFE, 0x02,
		0xFE, 0x02, 0xFE, 0x02, 0xFE, 0x02, 0xFE, 0x02,
		0xFE, 0x02, 0xFD, 0x02, 0xFD, 0x02, 0xFD, 0x02,
		0xFD, 0x02, 0xFD, 0x02, 0xFD, 0x02, 0x00, 0x00
	};
	postScreenUpdate_lava_screen2_updateMask(o->xPos + o->posTable[7].x, o->yPos + o->height - 1, o->height - 1, -5, o->screenNum, maskData2);
	if (_res->_currentScreenResourceNum <= 2 && (o->flags0 & 0x1F) == 2) {
		_g->setShakeScreen(2, 2);
	}
	if (o->levelData0x2988) {
		_g->updateAndyObject(o);
	}
	postScreenUpdate_lava_screen2_updateMask(o->xPos + o->posTable[7].x, o->yPos + o->height - 1, o->height - 1, 5, o->screenNum, maskData2);
	if ((o->flags0 & 0x1F) == 1) {
		if (_res->_currentScreenResourceNum == 2) {
			if ((o->flags0 & 0xE0) == 0x20) {
				static const uint8_t data[] = {
					0xC8, 0xAC, 0x06, 0, 0x0F, 0x90,
					0xB9, 0xA5, 0x08, 0, 0x0F, 0x10,
					0xDC, 0xA5, 0x09, 0, 0x09, 0x50,
					0xAB, 0xA0, 0x09, 0, 0x0F, 0x10,
					0xFF, 0xFF, 0xFF, 0, 0xFF, 0xFF
				};
				postScreenUpdate_lava_screen2_addLvlObjects(data);
			}
		}
		if (_res->_currentScreenResourceNum <= 2) {
			_g->setShakeScreen(3, 2);
		}
	}
	if (_res->_currentScreenResourceNum == 2 && (o->flags0 & 0x1F) != 0 && _g->clipLvlObjectsBoundingBox(_andyObject, o, 0x44)) {
		const int x = o->xPos + o->width / 2;
		if (_andyObject->xPos > x) {
			_andyObject->xPos += 10;
			_g->setAndySpecialAnimation(0x10);
		} else {
			_andyObject->xPos -= 10;
			_g->setAndySpecialAnimation(0x11);
		}
	}
}

void Level_lava::postScreenUpdate_lava_screen3_updateMask(int x, int y, int w, int h, int screenNum, uint8_t mask) {
	if (x < 0) {
		x = 0;
	}
	if (y < 0) {
		y = 0;
	}
	if (x + w >= 256) {
		w = 256 - x;
	}
	if (y + h >= 192) {
		h = 192 - y;
	}
	uint32_t maskOffset = Game::screenMaskOffset(_res->_screensBasePos[screenNum].u + x, _res->_screensBasePos[screenNum].v + y);
	uint32_t gridOffset = Game::screenGridOffset(x, y);
	w >>= 3;
	h >>= 3;
	for (int y = 0; y < h; ++y) {
		memset(_g->_screenMaskBuffer + maskOffset, mask, w);
		maskOffset += w;
		memset(_g->_screenPosTable[4] + gridOffset, 1, w);
		gridOffset += w;
	}
}

void Level_lava::postScreenUpdate_lava_screen3_updatePlatform(LvlObject *o, BoundingBox *b2) {
	BoundingBox b;
	b.x1 = o->xPos + o->posTable[1].x;
	b.y1 = o->yPos + o->posTable[1].y - 2;
	b.x2 = o->xPos + o->posTable[2].x;
	b.y2 = o->yPos + o->posTable[1].y + 4;
	postScreenUpdate_lava_screen3_updateMask(b.x1, b.y2, 32, 8, o->screenNum, 0);
	if ((o->flags0 & 0x1F) != 0xB) {
		if ((o->flags0 & 0x1F) != 2 && _g->clipBoundingBox(&b, b2)) {
			o->directionKeyMask = 4;
		}
		_g->updateAndyObject(o);
		const int x = o->xPos + o->posTable[1].x + 6;
		const int y = o->yPos + o->posTable[1].y + 4;
		postScreenUpdate_lava_screen3_updateMask(x, y, 32, 8, o->screenNum, 1);
	}
}

void Level_lava::postScreenUpdate_lava_screen3() {
	if (_res->_currentScreenResourceNum == 3) {
		BoundingBox b;
		b.x1 = _andyObject->xPos + _andyObject->posTable[4].x;
		b.y1 = _andyObject->yPos + _andyObject->posTable[4].y;
		b.x2 = _andyObject->xPos + _andyObject->posTable[5].x;
		b.y2 = _andyObject->yPos + _andyObject->posTable[5].y;
		LvlObject *o = _g->findLvlObject(2, 0, 3);
		postScreenUpdate_lava_screen3_updatePlatform(o, &b);
		o = findLvlObject_lava(o);
		postScreenUpdate_lava_screen3_updatePlatform(o, &b);
		_g->setLavaAndyAnimation(175);
	}
}

void Level_lava::postScreenUpdate_lava_screen4() {
	if (_res->_currentScreenResourceNum == 4) {
		_g->setLavaAndyAnimation(175);
	}
}

void Level_lava::postScreenUpdate_lava_screen5() {
	if (_res->_currentScreenResourceNum == 5) {
		_g->setLavaAndyAnimation(175);
	}
}

void Level_lava::postScreenUpdate_lava_screen6() {
	if (_res->_currentScreenResourceNum == 6) {
		_g->setLavaAndyAnimation(175);
	}
}

void Level_lava::postScreenUpdate_lava_screen7() {
	if (_res->_currentScreenResourceNum == 7) {
		if (_checkpoint == 2) {
			BoundingBox b = { 104, 0, 239, 50 };
                        AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
                        if (_g->clipBoundingBox(&b, &data->boundingBox)) {
				_checkpoint = 3;
			}
		}
		_g->setLavaAndyAnimation(175);
	}
}

void Level_lava::postScreenUpdate_lava_screen8() {
	if (_res->_currentScreenResourceNum == 8) {
		if (_andyObject->xPos + _andyObject->posTable[5].x < 72 || _andyObject->xPos + _andyObject->posTable[4].x < 72) {
			const uint8_t flags = _andyObject->flags0 & 0x1F;
			if (flags != 3 && flags != 7 && flags != 4) {
				_g->setLavaAndyAnimation(175);
			}
		}
	}
}

void Level_lava::postScreenUpdate_lava_screen10() {
	if (_res->_currentScreenResourceNum == 10) {
		if (_screenCounterTable[10] < 37) {
			if (_andyObject->yPos + _andyObject->posTable[3].y < 142) {
				_andyObject->actionKeyMask = 0x40;
				_andyObject->directionKeyMask = 0;
				if (_checkpoint == 3) {
					_checkpoint = 4;
					_res->_screensState[10].s0 = 1;
					_res->_resLvlScreenBackgroundDataTable[10].currentMaskId = 1;
					_g->setupScreenMask(10);
				}
				++_screenCounterTable[10];
				if (_screenCounterTable[10] == 13) {
					_g->_levelRestartCounter = 12;
				} else {
					++_screenCounterTable[10];
					if (_screenCounterTable[10] == 37) {
						if (!_paf->_skipCutscenes) {
							_paf->play(7);
							_paf->unload(7);
							_video->clearPalette();
							_g->updateScreen(_andyObject->screenNum);
						}
					}
				}
			}
		}
	}
}

void Level_lava::postScreenUpdate_lava_screen11() {
	if (_res->_currentScreenResourceNum == 11) {
		_g->setLavaAndyAnimation(175);
	}
}

void Level_lava::postScreenUpdate_lava_screen12() {
	if (_res->_currentScreenResourceNum == 12) {
		_g->setLavaAndyAnimation(175);
	}
}

void Level_lava::postScreenUpdate_lava_screen13() {
	if (_res->_currentScreenResourceNum == 13) {
		_g->setLavaAndyAnimation(175);
	}
}

void Level_lava::postScreenUpdate_lava_screen14() {
	if (_res->_currentScreenResourceNum == 14) {
		const int x = _andyObject->xPos;
		const Point16_t *pos = _andyObject->posTable;
		if (x + pos[5].x < 114 || x + pos[4].x < 114 || x + pos[3].x < 114 || x + pos[0].x < 114) {
			_g->setLavaAndyAnimation(175);
		}
	}
}

void Level_lava::postScreenUpdate_lava_screen15() {
	if (_res->_screensState[15].s0 != 0) {
		if (!_paf->_skipCutscenes) {
			_paf->play(8);
			_paf->unload(8);
		}
		_video->clearPalette();
		_g->_endLevel = true;
	}
}

void Level_lava::postScreenUpdate(int num) {
	switch (num) {
	case 0:
		postScreenUpdate_lava_screen0();
		break;
	case 1:
		postScreenUpdate_lava_screen1();
		break;
	case 2:
		postScreenUpdate_lava_screen2();
		break;
	case 3:
		postScreenUpdate_lava_screen3();
		break;
	case 4:
		postScreenUpdate_lava_screen4();
		break;
	case 5:
		postScreenUpdate_lava_screen5();
		break;
	case 6:
		postScreenUpdate_lava_screen6();
		break;
	case 7:
		postScreenUpdate_lava_screen7();
		break;
	case 8:
		postScreenUpdate_lava_screen8();
		break;
	case 10:
		postScreenUpdate_lava_screen10();
		break;
	case 11:
		postScreenUpdate_lava_screen11();
		break;
	case 12:
		postScreenUpdate_lava_screen12();
		break;
	case 13:
		postScreenUpdate_lava_screen13();
		break;
	case 14:
		postScreenUpdate_lava_screen14();
		break;
	case 15:
		postScreenUpdate_lava_screen15();
		break;
	}
}

void Level_lava::preScreenUpdate_lava_screen0() {
	if (_res->_screensState[0].s0 != 0) {
		_res->_screensState[0].s0 = 1;
	}
}

void Level_lava::preScreenUpdate_lava_screen3() {
	if (_res->_currentScreenResourceNum == 3) {
		if (_checkpoint == 0) {
			_checkpoint = 1;
		}
	}
}

void Level_lava::preScreenUpdate_lava_screen6() {
	if (_res->_currentScreenResourceNum == 6) {
		if (_checkpoint == 1) {
			_checkpoint = 2;
		}
	}
}

void Level_lava::preScreenUpdate_lava_screen10() {
	const int num = (_res->_screensState[10].s0 == 0) ? 0 : 1;
	if (_res->_screensState[10].s0 != 0 && _res->_screensState[10].s0 != 1) {
		_res->_screensState[10].s0 = 1;
	}
	_res->_resLvlScreenBackgroundDataTable[10].currentMaskId = num;
	if (_res->_currentScreenResourceNum == 10) {
		if (!_paf->_skipCutscenes) {
			_paf->preload(7);
		}
	}
}

void Level_lava::preScreenUpdate_lava_screen13() {
	if (_res->_currentScreenResourceNum == 13) {
		if (_checkpoint == 4) {
			_checkpoint = 5;
		}
	}
}

void Level_lava::preScreenUpdate_lava_screen15() {
	if (_res->_screensState[15].s0 == 0) {
		if (!_paf->_skipCutscenes) {
			_paf->preload(8);
		}
	}
}

void Level_lava::preScreenUpdate(int num) {
	switch (num) {
	case 1:
		preScreenUpdate_lava_screen0();
		break;
	case 3:
		preScreenUpdate_lava_screen3();
		break;
	case 6:
		preScreenUpdate_lava_screen6();
		break;
	case 10:
		preScreenUpdate_lava_screen10();
		break;
	case 13:
		preScreenUpdate_lava_screen13();
		break;
	case 15:
		preScreenUpdate_lava_screen15();
		break;
	}
}

void Level_lava::initialize() {
	_g->loadTransformLayerData(Game::_pwr2_screenTransformData);
	_g->resetWormHoleSprites();
	_screen1Counter = 0;
	_screen2Counter = 0;
}

void Level_lava::terminate() {
	_g->unloadTransformLayerData();
}

const uint8_t Game::_lava_screenTransformLut[] = {
	0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0,
	0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void Level_lava::tick() {
	_video->_displayShadowLayer = Game::_lava_screenTransformLut[_res->_currentScreenResourceNum * 2] != 0;
	assert(Game::_lava_screenTransformLut[_res->_currentScreenResourceNum * 2 + 1] == 0);
	_g->restoreAndyCollidesLava();
	_g->updateWormHoleSprites();
}

void Level_lava::setupScreenCheckpoint_lava_screen3() {
	LvlObject *ptr = _g->findLvlObject(2, 0, 3);
	assert(ptr);
	ptr->flags0 &= 0xFC00;
	ptr->xPos = 138;
	ptr->yPos = 157;
	ptr->anim = 0;
	ptr->frame = 0;
	ptr->directionKeyMask = 0;
	ptr = findLvlObject_lava(ptr);
	assert(ptr);
	ptr->flags0 &= 0xFC00;
	ptr->anim = 0;
	ptr->frame = 0;
	ptr->directionKeyMask = 0;
	ptr->xPos = 66;
	ptr->yPos = 157;
}

void Level_lava::setupScreenCheckpoint(int num) {
	switch (num) {
	case 3:
		setupScreenCheckpoint_lava_screen3();
		break;
	}
}
