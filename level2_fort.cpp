/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

// fort_hod - "swamp lands"

#include "game.h"
#include "level.h"
#include "paf.h"
#include "util.h"
#include "video.h"

static const CheckpointData _fort_checkpointData[5] = {
	{ 105,  46, 0x300c, 232,  0,  2 },
	{   1, 103, 0x300c, 232,  2,  2 },
	{  43,  96, 0x300c, 232,  6,  2 },
	{  32,  97, 0x300c, 232,  9,  2 },
	{ 105,  52, 0x300c, 232, 17,  2 }
};

static const uint8_t _fort_screenStartData[56] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct Level_fort: Level {
	virtual const CheckpointData *getCheckpointData(int num) const { return &_fort_checkpointData[num]; }
	virtual const uint8_t *getScreenRestartData() const { return _fort_screenStartData; }
	//virtual void initialize();
	//virtual void terminate();
	virtual void tick();
	virtual void preScreenUpdate(int screenNum);
	virtual void postScreenUpdate(int screenNum);
	virtual void setupScreenCheckpoint(int screenNum);

	void postScreenUpdate_fort_screen1();
	void postScreenUpdate_fort_screen6();
	void postScreenUpdate_fort_screen7();
	void postScreenUpdate_fort_screen8();
	void postScreenUpdate_fort_screen16();
	void postScreenUpdate_fort_screen17();
	void postScreenUpdate_fort_screen21();

	void preScreenUpdate_fort_screen1();
	void preScreenUpdate_fort_screen2();
	void preScreenUpdate_fort_screen6();
	void preScreenUpdate_fort_screen9();
	void preScreenUpdate_fort_screen14();
	void preScreenUpdate_fort_screen16();
	void preScreenUpdate_fort_screen17();
	void preScreenUpdate_fort_screen21();

	void setupScreenCheckpoint_fort_screen1();
};

Level *Level_fort_create() {
	return new Level_fort;
}

void Level_fort::postScreenUpdate_fort_screen1() {
	if (_res->_screensState[1].s0 == 2) {
		LvlObject *o = _g->findLvlObject(2, 0, 1);
		if (o) {
			o->actionKeyMask = 1;
		}
		++_screenCounterTable[1];
		if (_screenCounterTable[1] == 25) {
			_res->_resLvlScreenBackgroundDataTable[1].currentMaskId = 1;
			_g->setupScreenMask(1);
		} else if (_screenCounterTable[1] == 59) {
			_res->_screensState[1].s0 = 1;
			_g->updateBackgroundPsx(1);
			_res->_resLvlScreenBackgroundDataTable[1].currentBackgroundId = 1;
		}
	}
}

void Level_fort::postScreenUpdate_fort_screen6() {
	if (_res->_currentScreenResourceNum == 6) {
		AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		BoundingBox b = { 0, 0, 43, 191 };
		if (!_g->clipBoundingBox(&b, &data->boundingBox)) {
			_andyObject->actionKeyMask &= ~3;
			_andyObject->actionKeyMask |= 8;
			if (_andyObject->directionKeyMask & 4) {
				_andyObject->directionKeyMask &= ~10;
				++_screenCounterTable[6];
				if (_screenCounterTable[6] >= 188) {
					_andyObject->directionKeyMask &= ~4;
					_screenCounterTable[7] = 0;
				}
			}
			if (_screenCounterTable[7] < 25) {
				++_screenCounterTable[7];
				_andyObject->directionKeyMask &= ~4;
			} else if ((_andyObject->directionKeyMask & 4) == 0) {
				_screenCounterTable[6] = 0;
			}
		}
	}
}

void Level_fort::postScreenUpdate_fort_screen7() {
	if (_res->_currentScreenResourceNum == 7) {
		_andyObject->actionKeyMask &= ~3;
		_andyObject->actionKeyMask |= 8;
		if (_andyObject->directionKeyMask & 4) {
			_andyObject->directionKeyMask &= ~0xA;
			++_screenCounterTable[6];
			if (_screenCounterTable[6] >= 188) {
				_andyObject->directionKeyMask &= ~4;
				_screenCounterTable[7] = 0;
			}
		}
		if (_screenCounterTable[7] < 25) {
			++_screenCounterTable[7];
			_andyObject->directionKeyMask &= ~4;
		} else if ((_andyObject->directionKeyMask & 4) == 0) {
			_screenCounterTable[6] = 0;
		}
	}
}

void Level_fort::postScreenUpdate_fort_screen8() {
	if (_res->_currentScreenResourceNum == 8) {
		AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		BoundingBox b = { 138, 0, 255, 191 };
		if (!_g->clipBoundingBox(&b, &data->boundingBox)) {
			_andyObject->actionKeyMask &= ~3;
			_andyObject->actionKeyMask |= 8;
			if (_andyObject->directionKeyMask & 4) {
				_andyObject->directionKeyMask &= ~0xA;
				++_screenCounterTable[6];
				if (_screenCounterTable[6] >= 188) {
					_andyObject->directionKeyMask &= ~4;
					_screenCounterTable[7] = 0;
				}
			}
			if (_screenCounterTable[7] < 25) {
				++_screenCounterTable[7];
				_andyObject->directionKeyMask &= ~4;
			} else if ((_andyObject->directionKeyMask & 4) == 0) {
				_screenCounterTable[6] = 0;
			}
		}
	}
}

void Level_fort::postScreenUpdate_fort_screen16() {
	if (_res->_currentScreenResourceNum == 16) {
		if (_res->_screensState[16].s0 != 1) {
			AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			BoundingBox b = { 150, 0, 187, 60 };
                	if (!_g->clipBoundingBox(&b, &data->boundingBox)) {
				return;
			}
			if ((_andyObject->flags0 & 0x1F) == 0 && (_andyObject->flags0 & 0xE0) == 0xE0) {
				_res->_screensState[16].s0 = 1;
				_screenCounterTable[16] = 0;
			} else {
				_g->setAndySpecialAnimation(3);
			}
		} else {
			++_screenCounterTable[16];
			if (_screenCounterTable[16] == 5) {
				if (_checkpoint == 3) {
					_checkpoint = 4;
				}
				if (!_paf->_skipCutscenes) {
					_paf->play(3);
					_paf->unload(3);
				}
				_video->clearPalette();
				_g->restartLevel();
			}
		}
	}
}

void Level_fort::postScreenUpdate_fort_screen17() {
	if (_res->_screensState[17].s0 == 1) {
		++_screenCounterTable[17];
		if (_screenCounterTable[17] == 68) {
			_res->_screensState[17].s0 = 0;
		}
	}
}

void Level_fort::postScreenUpdate_fort_screen21() {
	if (_res->_currentScreenResourceNum == 21) {
		switch (_res->_screensState[21].s0) {
		case 1:
			++_screenCounterTable[21];
			if (_screenCounterTable[21] == 22) {
				_res->_screensState[21].s0 = 2;
			}
			break;
		case 2:
			if (!_paf->_skipCutscenes) {
				_paf->play(4);
				_paf->unload(4);
			}
			_video->clearPalette();
			_g->_endLevel = true;
			break;
		}
	}
}

void Level_fort::postScreenUpdate(int num) {
	switch (num) {
	case 1:
		postScreenUpdate_fort_screen1();
		break;
	case 6:
		postScreenUpdate_fort_screen6();
		break;
	case 7:
		postScreenUpdate_fort_screen7();
		break;
	case 8:
		postScreenUpdate_fort_screen8();
		break;
	case 16:
		postScreenUpdate_fort_screen16();
		break;
	case 17:
		postScreenUpdate_fort_screen17();
		break;
	case 21:
		postScreenUpdate_fort_screen21();
		break;
	}
}

void Level_fort::preScreenUpdate_fort_screen1() {
	if (_res->_currentScreenResourceNum == 1 && _checkpoint >= 1) {
		_res->_screensState[1].s0 = 1;
	}
	const int num = _res->_screensState[1].s0 == 0 ? 0 : 1;
	_res->_resLvlScreenBackgroundDataTable[1].currentBackgroundId = num;
	_res->_resLvlScreenBackgroundDataTable[1].currentMaskId = num;
}

void Level_fort::preScreenUpdate_fort_screen2() {
	if (_res->_currentScreenResourceNum == 2 && _checkpoint == 0) {
		_checkpoint = 1;
	}
}

void Level_fort::preScreenUpdate_fort_screen6() {
	if (_res->_currentScreenResourceNum == 6 && _checkpoint == 1) {
		_checkpoint = 2;
	}
}

void Level_fort::preScreenUpdate_fort_screen9() {
	if (_res->_currentScreenResourceNum == 9 && _checkpoint == 2) {
		_checkpoint = 3;
	}
}

void Level_fort::preScreenUpdate_fort_screen14() {
	if (_res->_currentScreenResourceNum == 14) {
		_res->_resLvlScreenBackgroundDataTable[14].currentBackgroundId = _res->_screensState[14].s0 != 0 ? 1 : 0;
	}
}

void Level_fort::preScreenUpdate_fort_screen16() {
	if (_res->_currentScreenResourceNum == 16) {
		_res->_screensState[16].s0 = 0;
		_andyObject->xPos += 9;
		if (!_paf->_skipCutscenes) {
			_paf->preload(3);
		}
	}
}

void Level_fort::preScreenUpdate_fort_screen17() {
	if (_res->_currentScreenResourceNum == 17) {
		_res->_screensState[17].s0 = _screenCounterTable[17] == 0 ? 1 : 0;
	}
}

void Level_fort::preScreenUpdate_fort_screen21() {
	if (_res->_currentScreenResourceNum == 21) {
		_res->_screensState[21].s0 = 0;
		if (!_paf->_skipCutscenes) {
			_paf->preload(4);
		}
	}
}

void Level_fort::tick() {
	_g->_plasmaCannonFlags |= 2;
}

void Level_fort::preScreenUpdate(int num) {
	switch (num) {
	case 1:
		preScreenUpdate_fort_screen1();
		break;
	case 2:
		preScreenUpdate_fort_screen2();
		break;
	case 6:
		preScreenUpdate_fort_screen6();
		break;
	case 9:
		preScreenUpdate_fort_screen9();
		break;
	case 14:
		preScreenUpdate_fort_screen14();
		break;
	case 16:
		preScreenUpdate_fort_screen16();
		break;
	case 17:
		preScreenUpdate_fort_screen17();
		break;
	case 21:
		preScreenUpdate_fort_screen21();
		break;
	}
}

void Level_fort::setupScreenCheckpoint_fort_screen1() {
	LvlObject *ptr = _g->findLvlObject(2, 0, 1);
	if (ptr) {
		ptr->xPos = 129;
		ptr->yPos = 97;
		ptr->anim = 0;
		ptr->frame = 0;
		ptr->directionKeyMask = 0;
		ptr->actionKeyMask = 0;
	}
}

void Level_fort::setupScreenCheckpoint(int num) {
	switch (num) {
	case 1:
		setupScreenCheckpoint_fort_screen1();
		break;
	}
}
