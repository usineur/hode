/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

// rock_hod - "canyon of death"

#include "game.h"
#include "level.h"
#include "paf.h"
#include "util.h"
#include "video.h"

static const CheckpointData _rock_checkpointData[8] = {
	{  96,  34, 0x300c,  22,  0,  0 },
	{  10, 126, 0x300c,  48,  2,  0 },
	{   6, 128, 0x300c,  48,  4,  0 },
	{  18, 125, 0x300c,  48,  5,  0 },
	{  21, 124, 0x300c,  48,  7,  0 },
	{ 105,  52, 0x3004, 232,  9,  2 },
	{  18, 121, 0x3004, 232, 13,  2 },
	{  43,  96, 0x3004,  39, 17,  2 }
};

static const uint8_t _rock_screenStartData[56] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x01, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x30, 0x00, 0x00, 0x01, 0x40, 0x02, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct Level_rock: Level {
	virtual const CheckpointData *getCheckpointData(int num) const { return &_rock_checkpointData[num]; }
	virtual const uint8_t *getScreenRestartData() const { return _rock_screenStartData; }
	virtual void initialize();
	virtual void terminate();
	virtual void tick();
	virtual void preScreenUpdate(int screenNum);
	virtual void postScreenUpdate(int screenNum);
	virtual void setupScreenCheckpoint(int screenNum);

	void postScreenUpdate_rock_screen0();
	void postScreenUpdate_rock_screen4();
	void postScreenUpdate_rock_screen8();
	void postScreenUpdate_rock_screen9();
	void postScreenUpdate_rock_screen10();
	void postScreenUpdate_rock_screen11();
	void postScreenUpdate_rock_screen13();
	void postScreenUpdate_rock_screen15();
	void postScreenUpdate_rock_screen16();
	void postScreenUpdate_rock_screen18();
	void postScreenUpdate_rock_screen19();

	void preScreenUpdate_rock_screen0();
	void preScreenUpdate_rock_screen1();
	void preScreenUpdate_rock_screen2();
	void preScreenUpdate_rock_screen3();
	void preScreenUpdate_rock_screen4();
	void preScreenUpdate_rock_screen5();
	void preScreenUpdate_rock_screen7();
	void preScreenUpdate_rock_screen9();
	void preScreenUpdate_rock_screen10();
	void preScreenUpdate_rock_screen13();
	void preScreenUpdate_rock_screen14();
	void preScreenUpdate_rock_screen15();
	void preScreenUpdate_rock_screen16();
	void preScreenUpdate_rock_screen17();
	void preScreenUpdate_rock_screen18();
	void preScreenUpdate_rock_screen19();

	void setupScreenCheckpoint_rock_screen2();
	void setupScreenCheckpoint_rock_screen3();
	void setupScreenCheckpoint_rock_screen18();
};

Level *Level_rock_create() {
	return new Level_rock;
}

void Level_rock::postScreenUpdate_rock_screen0() {
	switch (_res->_screensState[0].s0) {
	case 0:
		if ((_andyObject->flags0 & 0x1F) == 0 && ((_andyObject->flags0 >> 5) & 7) == 6) {
			_res->_screensState[0].s0 = 2;
		}
		break;
	case 2:
		++_screenCounterTable[0];
		if (_screenCounterTable[0] > 25) {
			_res->_screensState[0].s0 = 1;
		}
		if (_screenCounterTable[0] == 2) {
			_g->setShakeScreen(3, 12);
		}
		break;
	}
}

void Level_rock::postScreenUpdate_rock_screen4() {
	switch (_res->_screensState[4].s0) {
	case 0:
		if (_g->_plasmaCannonDirection != 0 && _res->_currentScreenResourceNum == 4) {
			if (_g->testPlasmaCannonPointsDirection(162, 24, 224, 68) == 0) {
				if (_g->testPlasmaCannonPointsDirection(202, 0, 255, 48) == 0) {
					if (_g->testPlasmaCannonPointsDirection(173, 8, 201, 23) == 0) {
						_g->_plasmaCannonLastIndex1 = 0;
						return;
					}
				}
			}
			++_screenCounterTable[4];
			if (_screenCounterTable[4] >= 20) {
				_res->_screensState[4].s0 = 2;
			}
			assert(_g->_plasmaExplosionObject);
			_g->_plasmaExplosionObject->screenNum = _res->_currentScreenResourceNum;
			_g->_plasmaCannonExplodeFlag = true;
			if (_g->_shakeScreenDuration == 0) {
				_g->setShakeScreen(3, 2);
			} else {
				++_g->_shakeScreenDuration;
			}
		}
		break;
	case 2:
		++_screenCounterTable[4];
		if (_screenCounterTable[4] == 33) {
			_res->_resLvlScreenBackgroundDataTable[4].currentMaskId = 1;
			_g->setupScreenMask(4);
		} else if (_screenCounterTable[4] > 46) {
			_res->_screensState[4].s0 = 1;
		}
		if (_screenCounterTable[4] == 31) {
			_g->setShakeScreen(2, 12);
		}
		break;
	}
}

void Level_rock::postScreenUpdate_rock_screen8() {
	if (_res->_currentScreenResourceNum == 8) {
		if ((_andyObject->flags0 & 0x1F) == 3) {
			_andyObject->flags2 = 0x3008;
		} else if (_andyObject->yPos + _andyObject->height / 2 < 89) {
			_andyObject->flags2 = 0x3004;
		} else {
			_andyObject->flags2 = 0x300C;
		}
	}
}

void Level_rock::postScreenUpdate_rock_screen9() {
	int xPos;
	if (_res->_currentScreenResourceNum == 9) {
		switch (_res->_screensState[9].s0) {
		case 0:
			xPos = 68;
			if ((_andyObject->flags0 & 0xE0) != 0) {
				xPos -= 14;
			}
			if (_andyObject->xPos > xPos && _andyObject->yPos < 86) {
				if (!_paf->_skipCutscenes) {
					_paf->play(1);
					_res->_resLvlScreenBackgroundDataTable[9].currentBackgroundId = 1;
					_video->_paletteNeedRefresh = true;
				}
				if (_checkpoint == 4) {
					_checkpoint = 5;
				}
				_res->_screensState[9].s0 = 1;
				_g->setAndySprite(2);
				_andyObject->xPos = 105;
				_andyObject->yPos = 52;
				_andyObject->anim = 232;
				_andyObject->frame = 0;
				_g->setupLvlObjectBitmap(_andyObject);
				_g->updateScreen(_andyObject->screenNum);
			}
			break;
		case 1:
			_g->_plasmaCannonFlags |= 2;
			break;
		}
	}
}

void Level_rock::postScreenUpdate_rock_screen10() {
	if (_res->_currentScreenResourceNum == 10) {
		BoundingBox box = { 64, 0, 267, 191 };
		_g->setAndyAnimationForArea(&box, 12);
	}
}

void Level_rock::postScreenUpdate_rock_screen11() {
	if (_res->_currentScreenResourceNum == 11) {
		BoundingBox box = { -12, 0, 162, 191 };
		_g->setAndyAnimationForArea(&box, 12);
	}
}

void Level_rock::postScreenUpdate_rock_screen13() {
	if (_res->_currentScreenResourceNum == 13) {
		_g->_plasmaCannonFlags &= ~1;
	}
}

void Level_rock::postScreenUpdate_rock_screen15() {
	if (_res->_currentScreenResourceNum == 15) {
		_g->_plasmaCannonFlags &= ~1;
		postScreenUpdate_rock_screen16();
	}
}

void Level_rock::postScreenUpdate_rock_screen16() {
	switch (_res->_screensState[16].s0) {
	case 0:
		if (_screenCounterTable[16] < 2) {
			if ((_andyObject->flags0 & 0x1F) == 2) {
				break;
			}
			AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			if (data->unk4 != 2 || _andyObject->xPos <= 155 || _andyObject->yPos >= 87) {
				break;
			}
			++_screenCounterTable[16];
			_g->setShakeScreen(3, 4);
		} else {
			_res->_screensState[16].s0 = 2;
			_g->_plasmaCannonFlags |= 1;
			_g->setShakeScreen(1, 18);
		}
		break;
	case 2:
		++_screenCounterTable[16];
		if (_screenCounterTable[16] == 5) {
			_res->_resLvlScreenBackgroundDataTable[16].currentMaskId = 1;
			_g->setupScreenMask(16);
		} else if (_screenCounterTable[16] == 23) {
			_andyObject->flags1 &= ~0x30;
			_andyObject->xPos = 131;
			_andyObject->yPos = 177;
			_andyObject->anim = 4;
			_andyObject->frame = 0;
			_g->_plasmaCannonFlags &= ~1;
		} else if (_screenCounterTable[16] == 37) {
			_res->_screensState[16].s0 = 3;
			_res->_resLvlScreenBackgroundDataTable[16].currentBackgroundId = 1;
		}
		break;
	case 3:
		++_screenCounterTable[16];
		if (_screenCounterTable[16] == 55) {
			_res->_screensState[16].s0 = 1;
		}
		break;
	}
}

void Level_rock::postScreenUpdate_rock_screen18() {
	switch (_res->_screensState[18].s0) {
	case 0:
		if (_andyObject->yPos + _andyObject->height < 162) {
			if ((_andyObject->flags0 & 0x1F) == 0 && _screenCounterTable[18] == 0) {
				_screenCounterTable[18] = 1;
			} else {
				++_screenCounterTable[18];
				if (_screenCounterTable[18] == 24) {
					_res->_screensState[18].s0 = 2;
					_res->_resLvlScreenBackgroundDataTable[18].currentMaskId = 2;
					_g->setupScreenMask(18);
				}
			}
		}
		break;
	case 2:
		++_screenCounterTable[18];
		if (_screenCounterTable[18] == 29) {
			LvlObject *o = _g->findLvlObject(2, 0, 18);
			o->actionKeyMask = 1;
		} else if (_screenCounterTable[18] == 43) {
			_g->setShakeScreen(2, 5);
			_res->_resLvlScreenBackgroundDataTable[18].currentMaskId = 1;
			_g->setupScreenMask(18);
		} else if (_screenCounterTable[18] < 57) {
			LvlObject *o = _g->findLvlObject(2, 0, 18);
			if ((o->flags0 & 0x1F) != 11 || (_andyObject->flags0 & 0x1F) == 11) {
				break;
			}
			BoundingBox box = { 24, 98, 108, 165 };
			AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			if (_g->clipBoundingBox(&box, &data->boundingBox)) {
				_andyObject->anim = 155;
				_andyObject->xPos = 59;
				_andyObject->yPos = 150;
				_andyObject->frame = 0;
				_andyObject->flags2 = 0x300C;
				_g->setupLvlObjectBitmap(_andyObject);
			}
		} else {
			_res->_screensState[18].s0 = 1;
			_res->_resLvlScreenBackgroundDataTable[18].currentBackgroundId = 1;
		}
		break;
	}
}

void Level_rock::postScreenUpdate_rock_screen19() {
	int fl;
	switch (_res->_screensState[19].s0) {
	case 0: {
			BoundingBox box = { 155, 69, 210, 88 };
			AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			if (!_g->clipBoundingBox(&box, &data->boundingBox)) {
				return;
			}
			_res->_screensState[19].s0 = 3;
		}
		break;
	case 1:
		if (_andyObject->xPos <= 139) {
			_andyObject->actionKeyMask = 0x80;
			_andyObject->directionKeyMask = 0;
		}
		fl = _andyObject->flags0 & 0x1F;
		if (fl == 0 || fl == 2) {
			fl = (_andyObject->flags0 >> 5) & 7;
			if (fl == 7 || fl == 2) {
				_res->_screensState[19].s0 = 4;
			}
		}
		break;
	case 2:
		if (_andyObject->yPos < 1) {
			if (!_paf->_skipCutscenes) {
				_paf->play(2);
				_paf->unload(2);
				if (false /* _isDemo */ && !_paf->_skipCutscenes) {
					_paf->play(21);
				}
			}
			_video->clearPalette();
			_g->_endLevel = true;
		}
		break;
	case 3:
		++_screenCounterTable[19];
		if (_screenCounterTable[19] == 1) {
			_res->_resLvlScreenBackgroundDataTable[19].currentMaskId = 1;
			_g->setupScreenMask(19);
			_g->setAndySpecialAnimation(0x12);
		} else if (_screenCounterTable[19] > 12) {
			_res->_screensState[19].s0 = 1;
			_res->_resLvlScreenBackgroundDataTable[19].currentBackgroundId = 1;
		}
		break;
	case 4:
		++_screenCounterTable[19];
		if (_screenCounterTable[19] == 25) {
			_g->setShakeScreen(2, 5);
			_res->_resLvlScreenBackgroundDataTable[19].currentMaskId = 2;
			_g->setupScreenMask(19);
		} else if (_screenCounterTable[19] == 33) {
			_res->_screensState[19].s0 = 2;
			_res->_resLvlScreenBackgroundDataTable[19].currentBackgroundId = 2;
		}
		break;
	}
}

void Level_rock::postScreenUpdate(int num) {
	switch (num) {
	case 0:
		postScreenUpdate_rock_screen0();
		break;
	case 4:
		postScreenUpdate_rock_screen4();
		break;
	case 8:
		postScreenUpdate_rock_screen8();
		break;
	case 9:
		postScreenUpdate_rock_screen9();
		break;
	case 10:
		postScreenUpdate_rock_screen10();
		break;
	case 11:
		postScreenUpdate_rock_screen11();
		break;
	case 13:
		postScreenUpdate_rock_screen13();
		break;
	case 15:
		postScreenUpdate_rock_screen15();
		break;
	case 16:
		postScreenUpdate_rock_screen16();
		break;
	case 18:
		postScreenUpdate_rock_screen18();
		break;
	case 19:
		postScreenUpdate_rock_screen19();
		break;
	}
}

int Game::objectUpdate_rock_case0(LvlObject *o) {
	return 1;
}

static const uint8_t _level1OpHelper1KeyMaskTable[112] = {
	8, 0, 8, 0, 8, 0, 8, 0, 8, 4, 9, 0, 9, 0, 9, 4,
	9, 0, 9, 0, 8, 4, 9, 0, 9, 4, 8, 0, 8, 4, 8, 4,
	8, 4, 8, 4, 8, 4, 8, 4, 8, 4, 8, 4, 8, 4, 8, 4,
	2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4,
	3, 0, 2, 4, 3, 0, 2, 4, 3, 4, 2, 0, 2, 4, 2, 4,
	8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 4, 8, 4, 4, 0,
	8, 4, 8, 4, 8, 4, 8, 4, 8, 4, 8, 0, 8, 0, 4, 0
};

void Game::objectUpdate_rock_helper(LvlObject *ptr, uint8_t *p) {
	const bool sameScreen = (_andyObject->screenNum == ptr->screenNum);
	int i = (_andyObject->width / 2 + _andyObject->xPos + (_andyObject->xPos & 7)) / 8;
	if (i < 0 || ptr->screenNum != _res->_currentScreenResourceNum) {
		i = 0;
	} else if (i > 31) {
		i = 31;
	}
	LvlObject *o = ptr->childPtr;
	assert(o);
	ptr->directionKeyMask = p[32 + i];
	if (ptr->directionKeyMask != 0x80) {
		p[67] = 0;
		ptr->actionKeyMask = p[i];
	} else {
		if (p[67] != 0) {
			--p[67];
		} else {
			p[67] = (_rnd.getNextNumber() >> 4) + 4;
			p[66] = _rnd.getNextNumber() & 7;
		}
		const int index = p[i] * 8 + p[66];
		ptr->directionKeyMask = _level1OpHelper1KeyMaskTable[index * 2];
		ptr->actionKeyMask = _level1OpHelper1KeyMaskTable[index * 2 + 1];
	}
	if ((ptr->actionKeyMask & 4) != 0 && sameScreen && (ptr->flags0 & 0x300) == 0x300) {
		if (clipLvlObjectsBoundingBox(_andyObject, ptr, 20)) {
			_mstFlags |= 0x80000000;
			setAndySpecialAnimation(0x80);
		}
	}
	if ((ptr->directionKeyMask & 4) != 0) {
		if (ptr->anim != 1 || ptr->frame != p[64]) {
			ptr->directionKeyMask &= ~4;
		}
	}
	if (_plasmaCannonDirection && (ptr->flags0 & 0x1F) != 0xB) {
		if (plasmaCannonHit(ptr->childPtr)) {
			ptr->actionKeyMask |= 0x20;
			++ptr->hitCount;
			if (ptr->hitCount > p[65] || (_cheats & kCheatOneHitPlasmaCannon) != 0) {
				ptr->hitCount = 0;
				ptr->actionKeyMask |= 7;
			}
			_plasmaCannonExplodeFlag = true;
			_plasmaCannonObject = ptr->childPtr;
		}
	}
	o->directionKeyMask = ptr->directionKeyMask;
	o->actionKeyMask = ptr->actionKeyMask;
	updateAndyObject(ptr);
	updateAndyObject(o);
}

bool Game::plasmaCannonHit(LvlObject *ptr) {
	assert(ptr);
	if (ptr->bitmapBits) {
		int dx = 0;
		if (ptr->screenNum == _currentLeftScreen) {
			dx = -256;
		} else if (ptr->screenNum == _currentRightScreen) {
			dx = 256;
		} else if (ptr->screenNum != _currentScreen) {
			return false;
		}
		if (testPlasmaCannonPointsDirection(ptr->xPos + dx, ptr->yPos, ptr->xPos + dx + ptr->width, ptr->yPos + ptr->height)) {
			return true;
		}
		assert(_plasmaExplosionObject);
		_plasmaExplosionObject->screenNum = ptr->screenNum;
	}
	return false;
}

// shadow screen2
int Game::objectUpdate_rock_case1(LvlObject *o) {
	static uint8_t data[68] = {
		0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
		0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
		0x0C, 0x07, 0x00, 0x00
	};
	if (_level->_screenCounterTable[2] == 0) {
		objectUpdate_rock_helper(o, data);
		if ((o->flags0 & 0x3FF) == 0x4B) {
			_level->_screenCounterTable[2] = 1;
		}
		return 1;
	}
	return 0;
}

// shadow screen3
int Game::objectUpdate_rock_case2(LvlObject *o) {
	static uint8_t data[68] = {
		0x00, 0x00, 0x00, 0x00, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
		0x06, 0x06, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x04, 0x04, 0x04, 0x04, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x04, 0x04, 0x04, 0x04, 0x04,
		0x06, 0x0B, 0x00, 0x00
	};
	if (_level->_screenCounterTable[3] == 0) {
		objectUpdate_rock_helper(o, data);
		if ((o->flags0 & 0x3FF) == 0x4B) {
			_level->_screenCounterTable[3] = 1;
		}
	} else {
		o->bitmapBits = 0;
		o = o->childPtr;
		assert(o);
		o->anim = 0;
		o->frame = 0;
		o->xPos = 204;
		o->yPos = 0;
		setupLvlObjectBitmap(o);
	}
	return 1;
}

int Game::objectUpdate_rock_case3(LvlObject *o) {
	updateAndyObject(o);
	return 1;
}

int Game::objectUpdate_rock_case4(LvlObject *o) {
	updateAndyObject(o);
	updateAndyObject(o->childPtr);
	return 1;
}

void Level_rock::preScreenUpdate_rock_screen0() {
	switch (_res->_screensState[0].s0) {
	case 0:
		_res->_resLvlScreenBackgroundDataTable[0].currentBackgroundId = 0;
		break;
	default:
		_res->_screensState[0].s0 = 1;
		_res->_resLvlScreenBackgroundDataTable[0].currentBackgroundId = 1;
		break;
	}
}

void Level_rock::preScreenUpdate_rock_screen1() {
	if (_checkpoint != 0) {
		_res->_screensState[0].s0 = 1;
	}
}

void Level_rock::preScreenUpdate_rock_screen2() {
	if (_res->_currentScreenResourceNum == 2 && _checkpoint == 0) {
		_checkpoint = 1;
	} else if (_checkpoint > 1) {
		LvlObject *ptr = _g->findLvlObject(2, 0, 2);
		if (ptr) {
			ptr->anim = 0;
			ptr->frame = 0;
		}
		_g->setupLvlObjectBitmap(ptr);
		ptr = _g->findLvlObject(2, 1, 2);
		if (ptr) {
			ptr->anim = 0;
			ptr->frame = 0;
		}
		_g->setupLvlObjectBitmap(ptr);
	}
}

void Level_rock::preScreenUpdate_rock_screen3() {
	if (_checkpoint > 1) {
		LvlObject *ptr = _g->findLvlObject(2, 0, 3);
		if (ptr) {
			ptr->anim = 0;
			ptr->frame = 0;
		}
		_g->setupLvlObjectBitmap(ptr);
		ptr = _g->findLvlObject(2, 1, 3);
		if (ptr) {
			ptr->anim = 0;
			ptr->frame = 0;
		}
		_g->setupLvlObjectBitmap(ptr);
	}
}

void Level_rock::preScreenUpdate_rock_screen4() {
	int num;
	switch (_res->_screensState[4].s0) {
	case 0:
		num = 0;
		break;
	case 1:
		num = 1;
		break;
	default:
		_res->_screensState[4].s0 = 1;
		num = 1;
		break;
	}
	_res->_resLvlScreenBackgroundDataTable[4].currentBackgroundId = num;
	_res->_resLvlScreenBackgroundDataTable[4].currentShadowId = num;
	_res->_resLvlScreenBackgroundDataTable[4].currentMaskId = num;
	if (_res->_currentScreenResourceNum == 4 && _checkpoint == 1) {
		_checkpoint = 2;
	}
}

void Level_rock::preScreenUpdate_rock_screen5() {
	if (_res->_currentScreenResourceNum == 5 && _checkpoint == 2) {
		_checkpoint = 3;
	}
}

void Level_rock::preScreenUpdate_rock_screen7() {
	if (_res->_currentScreenResourceNum == 7 && _checkpoint == 3) {
		_checkpoint = 4;
	}
}

void Level_rock::preScreenUpdate_rock_screen9() {
	switch (_res->_screensState[9].s0) {
	case 0:
		if (!_paf->_skipCutscenes) {
			_paf->preload(1);
		}
		_res->_resLvlScreenBackgroundDataTable[9].currentBackgroundId = 0;
		break;
	default:
		_res->_screensState[9].s0 = 1;
		_res->_resLvlScreenBackgroundDataTable[9].currentBackgroundId = 1;
		break;
	}
}

void Level_rock::preScreenUpdate_rock_screen10() {
	if (_res->_currentScreenResourceNum == 10) {
		if (_checkpoint == 4) {
			_checkpoint = 5;
		}
		if (!_paf->_skipCutscenes) {
			_paf->unload(22);
			_paf->preload(23);
		}
	}
}

void Level_rock::preScreenUpdate_rock_screen13() {
	if (_res->_currentScreenResourceNum == 13) {
		if (_checkpoint == 5) {
			_checkpoint = 6;
		}
		if (!_paf->_skipCutscenes) {
			_paf->unload(1);
		}
	}
}

void Level_rock::preScreenUpdate_rock_screen14() {
	if (_res->_currentScreenResourceNum == 14) {
		_res->_screensState[16].s0 = 0;
		_screenCounterTable[16] = 0;
	}
}

void Level_rock::preScreenUpdate_rock_screen15() {
	if (_res->_currentScreenResourceNum == 15) {
		if (_res->_screensState[16].s0 != 0) {
			_g->_fallingAndyCounter = 2;
			_g->_fallingAndyFlag = true;
		}
		_g->playAndyFallingCutscene(1);
	}
}

void Level_rock::preScreenUpdate_rock_screen16() {
	uint8_t _al;
	switch (_res->_screensState[16].s0) {
	case 0:
		_al = 0;
		break;
	default:
		_res->_screensState[16].s0 = 1;
		_g->_plasmaCannonFlags &= ~1;
		_al = 1;
		break;
	}
	_res->_resLvlScreenBackgroundDataTable[16].currentBackgroundId = _al;
	_res->_resLvlScreenBackgroundDataTable[16].currentMaskId = _al;
	if (_res->_currentScreenResourceNum == 16) {
		_g->playAndyFallingCutscene(1);
	}
}

void Level_rock::preScreenUpdate_rock_screen17() {
	if (_res->_currentScreenResourceNum == 17) {
		if (_checkpoint == 6) {
			_checkpoint = 7;
		}
		_g->playAndyFallingCutscene(1);
	}
}

void Level_rock::preScreenUpdate_rock_screen18() {
	uint8_t _al;
	switch (_res->_screensState[18].s0) {
	case 0:
		_al = 0;
		break;
	default:
		_res->_screensState[18].s0 = 1;
		_al = 1;
		break;
	}
	_res->_resLvlScreenBackgroundDataTable[18].currentBackgroundId = _al;
	_res->_resLvlScreenBackgroundDataTable[18].currentMaskId = _al;
	if (_res->_currentScreenResourceNum == 18) {
		_g->playAndyFallingCutscene(1);
	}
}

void Level_rock::preScreenUpdate_rock_screen19() {
	uint8_t _al;
	switch (_res->_screensState[19].s0) {
	case 0:
		_al = 0;
		break;
	case 1:
		_al = 1;
		break;
	default:
		_res->_screensState[19].s0 = 2;
		_al = 2;
		break;
	}
	_res->_resLvlScreenBackgroundDataTable[19].currentBackgroundId = _al;
	_res->_resLvlScreenBackgroundDataTable[19].currentMaskId = _al;
	if (_res->_currentScreenResourceNum == 19) {
		if (!_paf->_skipCutscenes) {
			_paf->preload(2);
		}
	}
}

void Level_rock::preScreenUpdate(int num) {
	switch (num) {
	case 0:
		preScreenUpdate_rock_screen0();
		break;
	case 1:
		preScreenUpdate_rock_screen1();
		break;
	case 2:
		preScreenUpdate_rock_screen2();
		break;
	case 3:
		preScreenUpdate_rock_screen3();
		break;
	case 4:
		preScreenUpdate_rock_screen4();
		break;
	case 5:
		preScreenUpdate_rock_screen5();
		break;
	case 7:
		preScreenUpdate_rock_screen7();
		break;
	case 9:
		preScreenUpdate_rock_screen9();
		break;
	case 10:
		preScreenUpdate_rock_screen10();
		break;
	case 13:
		preScreenUpdate_rock_screen13();
		break;
	case 14:
		preScreenUpdate_rock_screen14();
		break;
	case 15:
		preScreenUpdate_rock_screen15();
		break;
	case 16:
		preScreenUpdate_rock_screen16();
		break;
	case 17:
		preScreenUpdate_rock_screen17();
		break;
	case 18:
		preScreenUpdate_rock_screen18();
		break;
	case 19:
		preScreenUpdate_rock_screen19();
		break;
	}
}

void Level_rock::initialize() {
	if (!_paf->_skipCutscenes) {
		if (_andyObject->spriteNum == 0) {
			_paf->preload(22);
		} else {
			_paf->preload(23);
		}
	}
}

void Level_rock::tick() {
	if (_res->_currentScreenResourceNum > 9) {
		_g->_plasmaCannonFlags |= 2;
	}
}

void Level_rock::terminate() {
	if (!_paf->_skipCutscenes) {
		if (_andyObject->spriteNum == 0) {
			_paf->unload(22);
		} else {
			_paf->unload(23);
		}
	}
}

void Level_rock::setupScreenCheckpoint_rock_screen2() {
	LvlObject *ptr = _g->findLvlObject(2, 0, 2);
	if (ptr) {
		ptr->xPos = 146;
		ptr->yPos = 0;
		ptr->anim = 1;
		ptr->frame = 0;
		ptr->directionKeyMask = 0;
		ptr->actionKeyMask = 0;
	}
	ptr = _g->findLvlObject(2, 1, 2);
	if (ptr) {
		ptr->xPos = 88;
		ptr->yPos = 0;
		ptr->anim = 1;
		ptr->frame = 0;
		ptr->directionKeyMask = 0;
		ptr->actionKeyMask = 0;
	}
}

void Level_rock::setupScreenCheckpoint_rock_screen3() {
	LvlObject *ptr = _g->findLvlObject(2, 0, 3);
	if (ptr) {
		ptr->xPos = 198;
		ptr->yPos = 0;
		ptr->anim = 1;
		ptr->frame = 0;
		ptr->directionKeyMask = 0;
		ptr->actionKeyMask = 0;
	}
	ptr = _g->findLvlObject(2, 1, 3);
	if (ptr) {
		ptr->xPos = 116;
		ptr->yPos = 0;
		ptr->anim = 1;
		ptr->frame = 0;
		ptr->directionKeyMask = 0;
		ptr->actionKeyMask = 0;
	}
}

void Level_rock::setupScreenCheckpoint_rock_screen18() {
	LvlObject *ptr = _g->findLvlObject(2, 0, 18);
	if (ptr) {
		ptr->xPos = 16;
		ptr->yPos = 78;
		ptr->anim = 0;
		ptr->frame = 0;
		ptr->directionKeyMask = 0;
		ptr->actionKeyMask = 0;
	}
}

void Level_rock::setupScreenCheckpoint(int num) {
	switch (num) {
	case 2:
		setupScreenCheckpoint_rock_screen2();
		break;
	case 3:
		setupScreenCheckpoint_rock_screen3();
		break;
	case 18:
		setupScreenCheckpoint_rock_screen18();
		break;
	}
}
