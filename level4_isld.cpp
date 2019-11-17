
// isld_hod - "space island"

#include "game.h"
#include "level.h"
#include "paf.h"
#include "video.h"

static const CheckpointData _isld_checkpointData[5] = {
	{  46,  84, 0x300c, 207,  0,  2 },
	{  99,  48, 0x300c,  39,  3,  2 },
	{  27, 112, 0x300c,  39,  9,  2 },
	{ 171, 144, 0x300c,  39, 14,  2 },
	{ 195, 128, 0x300c,  39, 16,  2 }
};

static const uint8_t _isld_screenStartData[56] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct Level_isld: Level {
	virtual const CheckpointData *getCheckpointData(int num) const { return &_isld_checkpointData[num]; }
	virtual const uint8_t *getScreenRestartData() const { return _isld_screenStartData; }
	virtual void initialize();
	virtual void terminate();
	virtual void tick();
	virtual void preScreenUpdate(int screenNum);
	virtual void postScreenUpdate(int screenNum);
	//virtual void setupScreenCheckpoint(int screenNum);

	void postScreenUpdate_isld_screen0();
	void postScreenUpdate_isld_screen1();
	void postScreenUpdate_isld_screen2();
	void postScreenUpdate_isld_screen3();
	void postScreenUpdate_isld_screen4();
	void postScreenUpdate_isld_screen8();
	void postScreenUpdate_isld_screen9();
	void postScreenUpdate_isld_screen13();
	void postScreenUpdate_isld_screen15();
	void postScreenUpdate_isld_screen19();
	void postScreenUpdate_isld_screen20();
	void postScreenUpdate_isld_screen21();

	void preScreenUpdate_isld_screen1();
	void preScreenUpdate_isld_screen2();
	void preScreenUpdate_isld_screen3();
	void preScreenUpdate_isld_screen9();
	void preScreenUpdate_isld_screen14();
	void preScreenUpdate_isld_screen15();
	void preScreenUpdate_isld_screen16();
	void preScreenUpdate_isld_screen21();
};

Level *Level_isld_create() {
	return new Level_isld;
}

static const uint8_t _isld_stairsDxTable[] = {
	0, 1, 2, 3, 5, 7, 9, 10, 11, 12, 12, 12, 12, 11, 10, 9, 7, 5, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void Level_isld::postScreenUpdate_isld_screen0() {
	if (_res->_currentScreenResourceNum == 0) {
		if (_screenCounterTable[0] < 5) {
			++_screenCounterTable[0];
			_andyObject->actionKeyMask |= 1;
			_andyObject->directionKeyMask |= 2;
		}
	}
}

void Level_isld::postScreenUpdate_isld_screen1() {
	if (_res->_screensState[1].s0 != 2) {
		_screenCounterTable[1] = 0;
	} else {
		++_screenCounterTable[1];
		if (_screenCounterTable[1] > 21) {
			_res->_screensState[1].s0 = 1;
			_res->_resLvlScreenBackgroundDataTable[1].currentBackgroundId = 1;
		}
	}
}

void Level_isld::postScreenUpdate_isld_screen2() {
	if (_res->_screensState[2].s0 == 1) {
		if (_res->_currentScreenResourceNum == 2) {
			++_screenCounterTable[2];
			if (_screenCounterTable[2] == 1) {
				LvlObject *o = _g->findLvlObject(2, 0, 2);
				if (o) {
					o->actionKeyMask = 1;
				}
			} else if (_screenCounterTable[2] == 6) {
				_res->_resLvlScreenBackgroundDataTable[2].currentMaskId = 1;
				_g->setupScreenMask(2);
				_g->_plasmaCannonFlags |= 1;
			} else if (_screenCounterTable[2] == 38 && !_g->_fadePalette) {
				if ((_andyObject->flags0 & 0x1F) != 0xB) {
					_checkpoint = 1;
				}
				_g->_levelRestartCounter = 6;
			}
		}
	}
}

void Level_isld::postScreenUpdate_isld_screen3() {
	if (_res->_currentScreenResourceNum == 3) {
		if (_andyObject->xPos < 150) {
			LvlObject *o = _g->findLvlObject2(0, 1, 3);
			if (o) {
				AnimBackgroundData *backgroundData = (AnimBackgroundData *)_g->getLvlObjectDataPtr(o, kObjectDataTypeAnimBackgroundData);
				AndyLvlObjectData *andyData = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
				andyData->dxPos += _isld_stairsDxTable[backgroundData->currentFrame];
			}
		}
	}
}

void Level_isld::postScreenUpdate_isld_screen4() {
	if (_res->_currentScreenResourceNum == 4) {
		if (_andyObject->xPos < 150) {
			LvlObject *o = _g->findLvlObject2(0, 1, 4);
			if (o) {
				AnimBackgroundData *backgroundData = (AnimBackgroundData *)_g->getLvlObjectDataPtr(o, kObjectDataTypeAnimBackgroundData);
				AndyLvlObjectData *andyData = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
				andyData->dxPos += _isld_stairsDxTable[backgroundData->currentFrame];
			}
		}
	}
}

void Level_isld::postScreenUpdate_isld_screen8() {
	if (_res->_currentScreenResourceNum == 8) {
		const int xPos = _andyObject->xPos + _andyObject->posTable[3].x;
		if (xPos > 246) {
			_g->_fallingAndyCounter = 2;
			_g->_fallingAndyFlag = true;
			_g->playAndyFallingCutscene(1);
		}
	}
}

void Level_isld::postScreenUpdate_isld_screen9() {
	if (_res->_currentScreenResourceNum == 9) {
		const int xPos = _andyObject->xPos + _andyObject->posTable[3].x;
		if (xPos < 10 || xPos > 246) {
			_g->_fallingAndyCounter = 2;
			_g->_fallingAndyFlag = true;
			_g->playAndyFallingCutscene(1);
		}
	}
}

void Level_isld::postScreenUpdate_isld_screen13() {
	if (_res->_currentScreenResourceNum == 13) {
		postScreenUpdate_isld_screen15();
	}
}

void Level_isld::postScreenUpdate_isld_screen15() {
	switch (_res->_screensState[15].s0) {
	case 2:
		++_screenCounterTable[15];
		if (_screenCounterTable[15] >= 60) {
			_res->_screensState[15].s0 = 1;
		}
		break;
	case 0:
		if (_res->_currentScreenResourceNum == 15 && (_andyObject->flags0 & 0x1F) == 1 && (_andyObject->flags0 & 0x30) == 0) {
			BoundingBox b = { 72, 108, 110, 162 };
			AndyLvlObjectData *andyData = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			if (_g->clipBoundingBox(&b, &andyData->boundingBox)) {
				_g->setAndySpecialAnimation(3);
				_res->_screensState[15].s0 = 2;
				_res->_screensState[14].s0 = 1;
			}
		}
		break;
	}
}

void Level_isld::postScreenUpdate_isld_screen19() {
	if (_res->_currentScreenResourceNum == 19) {
		const int xPos = _andyObject->xPos + _andyObject->posTable[3].x;
		if (xPos < 10 || xPos > 246) {
			_g->_fallingAndyCounter = 2;
			_g->_fallingAndyFlag = true;
			_g->playAndyFallingCutscene(1);
		}
	}
}

void Level_isld::postScreenUpdate_isld_screen20() {
	if (_res->_currentScreenResourceNum == 20) {
		const int xPos = _andyObject->xPos + _andyObject->posTable[3].x;
		if (xPos < 10 || xPos > 246) {
			_g->_fallingAndyCounter = 2;
			_g->_fallingAndyFlag = true;
			_g->playAndyFallingCutscene(1);
		}
	}
}

void Level_isld::postScreenUpdate_isld_screen21() {
	if (_res->_currentScreenResourceNum == 21) {
		AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		BoundingBox b1 = { 64, 0, 166, 120 };
		if (_g->clipBoundingBox(&b1, &data->boundingBox)) {
			const uint8_t _cl = _andyObject->flags0 & 0x1F;
			const uint8_t _al = (_andyObject->flags0 >> 5) & 7;
			if (_cl == 3 && _al == 4) {
				_andyObject->directionKeyMask &= ~0xA;
				BoundingBox b2 = { 64, 0, 166, 75 };
				if (_g->clipBoundingBox(&b2, &data->boundingBox)) {
					_g->setAndySpecialAnimation(4);
				}
			} else if (_cl == 8 && _al == 1) {
				if (!_paf->_skipCutscenes) {
					_paf->play(6);
					_paf->unload(6);
					_paf->unload(kPafAnimation_IslandAndyFalling);
				}
				_video->clearPalette();
				_g->_endLevel = true;
			}
		}
	}
}

void Level_isld::postScreenUpdate(int num) {
	switch (num) {
	case 0:
		postScreenUpdate_isld_screen0();
		break;
	case 1:
		postScreenUpdate_isld_screen1();
		break;
	case 2:
		postScreenUpdate_isld_screen2();
		break;
	case 3:
		postScreenUpdate_isld_screen3();
		break;
	case 4:
		postScreenUpdate_isld_screen4();
		break;
	case 8:
		postScreenUpdate_isld_screen8();
		break;
	case 9:
		postScreenUpdate_isld_screen9();
		break;
	case 13:
		postScreenUpdate_isld_screen13();
		break;
	case 15:
		postScreenUpdate_isld_screen15();
		break;
	case 19:
		postScreenUpdate_isld_screen19();
		break;
	case 20:
		postScreenUpdate_isld_screen20();
		break;
	case 21:
		postScreenUpdate_isld_screen21();
		break;
	}
}

void Level_isld::preScreenUpdate_isld_screen1() {
	if (_res->_currentScreenResourceNum == 1) {
		switch (_res->_screensState[1].s0) {
		case 1:
		case 2:
			_res->_resLvlScreenBackgroundDataTable[1].currentBackgroundId = 1;
			_res->_screensState[1].s0 = 1;
			break;
		default:
			_res->_resLvlScreenBackgroundDataTable[1].currentBackgroundId = 0;
			_res->_screensState[1].s0 = 0;
			break;
		}
	}
}

void Level_isld::preScreenUpdate_isld_screen2() {
	if (_res->_currentScreenResourceNum == 2) {
		_res->_screensState[2].s0 = 0;
		_res->_resLvlScreenBackgroundDataTable[2].currentMaskId = 0;
		_screenCounterTable[2] = 0;
		LvlObject *o = _g->findLvlObject(2, 0, 2);
		if (o) {
			o->actionKeyMask = 0;
			o->xPos = 30;
			o->yPos = 83;
			o->frame = 0;
			_g->setupLvlObjectBitmap(o);
		}
	}
}

void Level_isld::preScreenUpdate_isld_screen3() {
	if (_res->_currentScreenResourceNum == 3) {
		LvlObject *o = _g->findLvlObject(2, 0, 3);
		if (o) {
			o->xPos = 70;
			o->yPos = 0;
			o->frame = 22;
			_g->setupLvlObjectBitmap(o);
		}
	}
}

void Level_isld::preScreenUpdate_isld_screen9() {
	if (_res->_currentScreenResourceNum == 9) {
		if (_checkpoint == 1) {
			_checkpoint = 2;
		}
	}
}

void Level_isld::preScreenUpdate_isld_screen14() {
	if (_res->_screensState[14].s0 != 0) {
		_res->_resLvlScreenBackgroundDataTable[14].currentBackgroundId = 1;
		_res->_resLvlScreenBackgroundDataTable[14].currentMaskId = 1;
	} else {
		_res->_resLvlScreenBackgroundDataTable[14].currentBackgroundId = 0;
		_res->_resLvlScreenBackgroundDataTable[14].currentMaskId = 0;
	}
	if (_res->_currentScreenResourceNum == 14) {
		if (_checkpoint == 2) {
			_checkpoint = 3;
		}
	}
}

void Level_isld::preScreenUpdate_isld_screen15() {
	if (_res->_screensState[15].s0 != 0) {
		_res->_resLvlScreenBackgroundDataTable[15].currentBackgroundId = 1;
		_res->_resLvlScreenBackgroundDataTable[15].currentSoundId = 1;
		_res->_screensState[14].s0 = 1;
	} else {
		_res->_resLvlScreenBackgroundDataTable[15].currentBackgroundId = 0;
		_res->_resLvlScreenBackgroundDataTable[15].currentSoundId = 0;
	}
}

void Level_isld::preScreenUpdate_isld_screen16() {
	if (_res->_currentScreenResourceNum == 16) {
		if (_checkpoint == 3) {
			_checkpoint = 4;
		}
		if (_res->_screensState[14].s0 == 0) {
			_res->_screensState[14].s0 = 1;
			_res->_screensState[15].s0 = 1;
		}
	}
}

void Level_isld::preScreenUpdate_isld_screen21() {
	if (_res->_currentScreenResourceNum == 21) {
		if (!_paf->_skipCutscenes) {
			_paf->preload(6);
		}
	}
}

void Level_isld::preScreenUpdate(int num) {
	switch (num) {
	case 1:
		preScreenUpdate_isld_screen1();
		break;
	case 2:
		preScreenUpdate_isld_screen2();
		break;
	case 3:
		preScreenUpdate_isld_screen3();
		break;
	case 9:
		preScreenUpdate_isld_screen9();
		break;
	case 14:
		preScreenUpdate_isld_screen14();
		break;
	case 15:
		preScreenUpdate_isld_screen15();
		break;
	case 16:
		preScreenUpdate_isld_screen16();
		break;
	case 21:
		preScreenUpdate_isld_screen21();
		break;
	}
}

void Level_isld::initialize() {
	if (!_paf->_skipCutscenes) {
		_paf->preload(kPafAnimation_IslandAndyFalling);
	}
	_g->resetWormHoleSprites();
}

void Level_isld::tick() {
	_g->updateWormHoleSprites();
}

void Level_isld::terminate() {
	if (!_paf->_skipCutscenes) {
		// bugfix: original calls preload()
		_paf->unload(kPafAnimation_IslandAndyFalling);
	}
}
