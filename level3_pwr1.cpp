/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

// pwr1_hod - "magic lake"

#include "game.h"
#include "level.h"
#include "paf.h"
#include "util.h"
#include "video.h"

static const CheckpointData _pwr1_checkpointData[10] = {
	{ 190,  48, 0x300c, 196,  1,  2 },
	{ 158,  89, 0x300c, 232,  6,  2 },
	{  71, 128, 0x300c, 196,  9,  2 },
	{   7, 145, 0x300c, 196, 15,  2 },
	{  50,  62, 0x300c, 196, 18,  2 },
	{   9,  25, 0x300c, 232, 21,  2 },
	{  19,  73, 0x300c, 232, 27,  2 },
	{ 225,  73, 0x300c, 232, 26,  2 },
	{ 100, 121, 0x300c, 232, 29,  2 },
	{  80, 121, 0x700c, 232, 31,  2 }
};

static const uint8_t _pwr1_screenStartData[80] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct Level_pwr1: Level {
	virtual const CheckpointData *getCheckpointData(int num) const { return &_pwr1_checkpointData[num]; }
	virtual const uint8_t *getScreenRestartData() const { return _pwr1_screenStartData; }
	virtual void initialize();
	virtual void terminate();
	virtual void tick();
	virtual void preScreenUpdate(int screenNum);
	virtual void postScreenUpdate(int screenNum);
	//virtual void setupScreenCheckpoint(int screenNum);

	void postScreenUpdate_pwr1_helper(BoundingBox *b, int dx, int dy);
	void postScreenUpdate_pwr1_screen6();
	void postScreenUpdate_pwr1_screen10();
	void postScreenUpdate_pwr1_screen12();
	void postScreenUpdate_pwr1_screen14();
	void postScreenUpdate_pwr1_screen16();
	void postScreenUpdate_pwr1_screen18();
	void postScreenUpdate_pwr1_screen23();
	void postScreenUpdate_pwr1_screen27();
	void postScreenUpdate_pwr1_screen35();

	void preScreenUpdate_pwr1_screen4();
	void preScreenUpdate_pwr1_screen6();
	void preScreenUpdate_pwr1_screen9();
	void preScreenUpdate_pwr1_screen15();
	void preScreenUpdate_pwr1_screen21();
	void preScreenUpdate_pwr1_screen23();
	void preScreenUpdate_pwr1_screen24();
	void preScreenUpdate_pwr1_screen26();
	void preScreenUpdate_pwr1_screen27();
	void preScreenUpdate_pwr1_screen29();
	void preScreenUpdate_pwr1_screen31();
	void preScreenUpdate_pwr1_screen35();
};

Level *Level_pwr1_create() {
	return new Level_pwr1;
}

void Level_pwr1::postScreenUpdate_pwr1_helper(BoundingBox *b, int dx, int dy) {
	LvlObject *o = _g->_lvlObjectsList3;
	while (o) {
		if (o->spriteNum == 4 && o->anim <= 9) {
			BoundingBox b2;
			b2.x1 = o->posTable[3].x + o->xPos - 1;
			b2.x2 = o->posTable[3].x + o->xPos + 1;
			b2.y1 = o->posTable[3].y + o->yPos - 1;
			b2.y2 = o->posTable[3].y + o->yPos + 1;
			if (_g->clipBoundingBox(b, &b2)) {
				o->xPos += dx;
				o->yPos += dy;
			}
		}
		o = o->nextPtr;
	}
	BoundingBox b2;
	b2.x1 = _andyObject->posTable[3].x + _andyObject->xPos - 1;
	b2.x2 = _andyObject->posTable[3].x + _andyObject->xPos + 1;
	b2.y1 = _andyObject->posTable[3].y + _andyObject->yPos - 1;
	b2.y2 = _andyObject->posTable[3].y + _andyObject->yPos + 1;
	if (_g->clipBoundingBox(b, &b2)) {
		AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		data->dxPos += dx;
		data->dyPos += dy;
	}
}

void Level_pwr1::postScreenUpdate_pwr1_screen6() {
	LvlBackgroundData *dat = &_res->_resLvlScreenBackgroundDataTable[6];
	switch (_res->_screensState[6].s0) {
	case 1: {
			BoundingBox b = { 185, 78, 225, 136 };
			LvlObject *o = _g->findLvlObjectBoundingBox(&b);
			if (o) {
				ShootLvlObjectData *oosd = (ShootLvlObjectData *)_g->getLvlObjectDataPtr(o, kObjectDataTypeShoot);
				if (oosd->type == 6) {
					_res->_screensState[6].s0 = 4;
					dat->currentMaskId = 2;
				}
			}
		}
		break;
	case 2:
		break;
	case 3:
		++_screenCounterTable[6];
		if (_screenCounterTable[6] >= 41) {
			_res->_screensState[6].s0 = 1;
			dat->currentMaskId = 1;
			dat->currentBackgroundId = 1;
			if (_checkpoint == 0) {
				_checkpoint = 1;
			}
		}
		break;
	case 4:
		++_screenCounterTable[6];
		if (_screenCounterTable[6] >= 54) {
			_res->_screensState[6].s0 = 2;
			dat->currentBackgroundId = 1;
		}
		break;
	default: {
			BoundingBox b = { 93, 17, 255, 156 };
			AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			if (_g->clipBoundingBox(&b, &data->boundingBox)) {
				_res->_screensState[6].s0 = 3;
				_g->setShakeScreen(3, 31);
			}
		}
		break;
	}
	if (_res->_screensState[6].s3 != dat->currentMaskId) {
		_g->setupScreenMask(6);
	}
}

void Level_pwr1::postScreenUpdate_pwr1_screen10() {
	if (_res->_currentScreenResourceNum == 10) {
		BoundingBox b1 = { 50, 0, 100, 122 };
		postScreenUpdate_pwr1_helper(&b1, 0, 2);
		BoundingBox b2 = { 58, 50, 84, 132 };
		postScreenUpdate_pwr1_helper(&b2, 0, 2);
		BoundingBox b3 = { 42, 80, 72, 142 };
		postScreenUpdate_pwr1_helper(&b3, 4, 0);
		BoundingBox b4 = { 73, 100, 94, 142 };
		postScreenUpdate_pwr1_helper(&b4, -2, 0);
	}
}

void Level_pwr1::postScreenUpdate_pwr1_screen12() {
	if (_res->_currentScreenResourceNum == 12) {
		BoundingBox b1 = { 64, 124, 255, 166 };
		postScreenUpdate_pwr1_helper(&b1, -1, 0);
		BoundingBox b2 = { 56, 134, 160, 160 };
		postScreenUpdate_pwr1_helper(&b2, -2, 0);
		BoundingBox b3 = { 55, 124, 82, 146 };
		postScreenUpdate_pwr1_helper(&b3, 0, 2);
		BoundingBox b4 = { 55, 147, 82, 166 };
		postScreenUpdate_pwr1_helper(&b4, 0, -2);
		BoundingBox b5 = { 64, 46, 255, 90 };
		postScreenUpdate_pwr1_helper(&b5, -2, 0);
		BoundingBox b6 = { 56, 56, 255, 80 };
		postScreenUpdate_pwr1_helper(&b6, -3, 0);
		BoundingBox b7 = { 55, 46, 82, 68 };
		postScreenUpdate_pwr1_helper(&b7, 0, 2);
		BoundingBox b8 = { 55, 69, 82, 90 };
		postScreenUpdate_pwr1_helper(&b8, 0, -2);
		BoundingBox b9 = { 154, 52, 198, 191 };
		postScreenUpdate_pwr1_helper(&b9, 0, -2);
		BoundingBox bA = { 164, 44, 188, 191 };
		postScreenUpdate_pwr1_helper(&bA, 0, -2);
	}
}

void Level_pwr1::postScreenUpdate_pwr1_screen14() {
	if (_res->_currentScreenResourceNum == 14) {
		BoundingBox b1 = { 0, 136, 104, 191 };
		postScreenUpdate_pwr1_helper(&b1, 2, 0);
		BoundingBox b2 = { 0, 152, 112, 178 };
		postScreenUpdate_pwr1_helper(&b2, 2, 0);
		BoundingBox b3 = { 76, 148, 116, 164 };
		postScreenUpdate_pwr1_helper(&b3, 0, 2);
		BoundingBox b4 = { 76, 164, 116, 186 };
		postScreenUpdate_pwr1_helper(&b4, 0, -2);
	}
}

void Level_pwr1::postScreenUpdate_pwr1_screen16() {
	if (_res->_currentScreenResourceNum == 16) {
		BoundingBox b1 = { 100, 130, 160, 176 };
		postScreenUpdate_pwr1_helper(&b1, 2, 0);
		BoundingBox b2 = { 88, 140, 180, 167 };
		postScreenUpdate_pwr1_helper(&b2, 2, 0);
		BoundingBox b3 = { 88, 82, 185, 120 };
		postScreenUpdate_pwr1_helper(&b3, 2, 0);
		BoundingBox b4 = { 120, 92, 190, 114 };
		postScreenUpdate_pwr1_helper(&b4, 2, 0);
		BoundingBox b5 = { 104, 40, 147, 120 };
		postScreenUpdate_pwr1_helper(&b5, 0, -1);
		BoundingBox b6 = { 115, 24, 138, 90 };
		postScreenUpdate_pwr1_helper(&b6, 0, -1);
	}
}

void Level_pwr1::postScreenUpdate_pwr1_screen18() {
	if (_res->_currentScreenResourceNum == 18) {
		if (_checkpoint == 3) {
			BoundingBox b = { 0, 0, 123, 125 };
			AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			if (_g->clipBoundingBox(&b, &data->boundingBox)) {
				++_checkpoint;
			}
		}
		BoundingBox b1 = { 156, 80, 204, 144 };
		postScreenUpdate_pwr1_helper(&b1, 0, -2);
		BoundingBox b2 = { 166, 88, 194, 152 };
		postScreenUpdate_pwr1_helper(&b2, 0, -2);
	}
}

void Level_pwr1::postScreenUpdate_pwr1_screen23() {
	switch (_res->_screensState[23].s0) {
	case 2:
		++_screenCounterTable[23];
		if (_screenCounterTable[23] == 26) {
			_res->_screensState[23].s0 = 1;
			_res->_resLvlScreenBackgroundDataTable[23].currentMaskId = 1;
			_res->_resLvlScreenBackgroundDataTable[23].currentBackgroundId = 1;
			_g->setupScreenMask(23);
		}
		break;
	case 0:
		if (_res->_currentScreenResourceNum == 23) {
			BoundingBox b = { 26, 94, 63, 127 };
			AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			if (_g->clipBoundingBox(&b, &data->boundingBox)) {
				uint8_t flags = _andyObject->flags0;
				if ((flags & 0x1F) == 0 && (flags & 0xE0) == 0xE0) {
					_res->_screensState[23].s0 = 2;
				} else {
					_g->setAndySpecialAnimation(3);
				}
			}
		}
		break;
	}
}

void Level_pwr1::postScreenUpdate_pwr1_screen27() {
	switch (_res->_screensState[27].s0) {
	case 2:
		++_screenCounterTable[27];
		if (_screenCounterTable[27] == 37) {
			_res->_screensState[27].s0 = 1;
			_res->_resLvlScreenBackgroundDataTable[27].currentMaskId = 1;
			_res->_resLvlScreenBackgroundDataTable[27].currentBackgroundId = 1;
			_g->setupScreenMask(27);
		}
		break;
	case 0:
		if (_res->_currentScreenResourceNum == 27 && (_andyObject->flags0 & 0x1F) == 6) {
			BoundingBox b1;
			b1.x1 = _andyObject->xPos + _andyObject->posTable[7].x - 3;
			b1.x2 = _andyObject->xPos + _andyObject->posTable[7].x + 4;
			b1.y1 = _andyObject->yPos + _andyObject->posTable[7].y - 2;
			b1.y2 = _andyObject->yPos + _andyObject->posTable[7].y + 2;
			BoundingBox b2 = { 25, 163, 31, 188 };
			if (_g->clipBoundingBox(&b2, &b1)) {
				++_screenCounterTable[27];
				if (_screenCounterTable[27] == 9) {
					_res->_screensState[27].s0 = 2;
				}
			}
		}
		break;
	}
}

void Level_pwr1::postScreenUpdate_pwr1_screen35() {
	if (_res->_currentScreenResourceNum == 35) {
		AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		BoundingBox b = { 0, 0, 193, 88 };
		if (_g->clipBoundingBox(&b, &data->boundingBox)) {
			_andyObject->actionKeyMask &= ~3;
			_andyObject->directionKeyMask &= ~4;
			_andyObject->actionKeyMask |= 8;
		}
		if (_res->_screensState[35].s0 != 0) {
			++_screenCounterTable[35];
			if (_screenCounterTable[35] == 46) {
				if (!_paf->_skipCutscenes) {
					_paf->play(5);
					_paf->unload(5);
				}
				_video->clearPalette();
				_g->_endLevel = true;
			}
		}
	}
}

void Level_pwr1::postScreenUpdate(int num) {
	switch (num) {
	case 6:
		postScreenUpdate_pwr1_screen6();
		break;
	case 10:
		postScreenUpdate_pwr1_screen10();
		break;
	case 12:
		postScreenUpdate_pwr1_screen12();
		break;
	case 14:
		postScreenUpdate_pwr1_screen14();
		break;
	case 16:
		postScreenUpdate_pwr1_screen16();
		break;
	case 18:
		postScreenUpdate_pwr1_screen18();
		break;
	case 23:
		postScreenUpdate_pwr1_screen23();
		break;
	case 27:
		postScreenUpdate_pwr1_screen27();
		break;
	case 35:
		postScreenUpdate_pwr1_screen35();
		break;
	}
}

void Level_pwr1::preScreenUpdate_pwr1_screen4() {
        if (_res->_currentScreenResourceNum == 4) {
		const uint8_t num = (_res->_screensState[4].s0 == 0) ? 0 : 1;
		_res->_resLvlScreenBackgroundDataTable[4].currentBackgroundId = num;
		_res->_resLvlScreenBackgroundDataTable[4].currentMaskId = num;
	}
}

void Level_pwr1::preScreenUpdate_pwr1_screen6() {
        if (_res->_currentScreenResourceNum == 6 || _res->_currentScreenResourceNum == 5) {
		if (_res->_screensState[6].s0 == 0) {
			if (_checkpoint != 1) {
				_screenCounterTable[6] = 0;
				_res->_resLvlScreenBackgroundDataTable[6].currentBackgroundId = 0;
				_res->_resLvlScreenBackgroundDataTable[6].currentMaskId = 0;
				_res->_screensState[6].s0 = 0;
			} else {
				_screenCounterTable[6] = 41;
				_res->_resLvlScreenBackgroundDataTable[6].currentBackgroundId = 1;
				_res->_resLvlScreenBackgroundDataTable[6].currentMaskId = 1;
				_res->_screensState[6].s0 = 1;
			}
		} else {
			_screenCounterTable[6] = 54;
			_res->_resLvlScreenBackgroundDataTable[6].currentBackgroundId = 2;
			_res->_resLvlScreenBackgroundDataTable[6].currentMaskId = 2;
			_res->_screensState[6].s0 = 2;
		}
        }
}

void Level_pwr1::preScreenUpdate_pwr1_screen9() {
	if (_res->_currentScreenResourceNum == 9) {
		if (_checkpoint == 1) {
			_checkpoint = 2;
		}
	}
}

void Level_pwr1::preScreenUpdate_pwr1_screen15() {
	if (_res->_currentScreenResourceNum == 15) {
		if (_checkpoint == 2) {
			_checkpoint = 3;
		}
	}
}

void Level_pwr1::preScreenUpdate_pwr1_screen21() {
	if (_res->_currentScreenResourceNum == 15) {
		if (_checkpoint == 4) {
			_checkpoint = 5;
		}
	}
}

void Level_pwr1::preScreenUpdate_pwr1_screen23() {
	if (_res->_currentScreenResourceNum == 23 || _res->_currentScreenResourceNum == 26) {
		const uint8_t num = _res->_screensState[23].s0 != 0 ? 1 : 0;
		_res->_resLvlScreenBackgroundDataTable[23].currentBackgroundId = num;
		_res->_resLvlScreenBackgroundDataTable[23].currentMaskId = num;
	}
}

void Level_pwr1::preScreenUpdate_pwr1_screen24() {
	if (_res->_currentScreenResourceNum == 24) {
		if (_res->_screensState[27].s0 != 0) {
			if (_checkpoint == 6) {
				_checkpoint = 7;
			}
		}
	}
}

void Level_pwr1::preScreenUpdate_pwr1_screen26() {
	if (_checkpoint >= 7) {
		_res->_screensState[23].s0 = 1;
	}
	preScreenUpdate_pwr1_screen23();
}

void Level_pwr1::preScreenUpdate_pwr1_screen27() {
	if (_res->_currentScreenResourceNum == 27) {
		uint8_t num = 0;
		if (_res->_screensState[27].s0 != 0 || _checkpoint >= 7) {
			num = 1;
			_screenCounterTable[27] = 37;
		} else if (_checkpoint <= 5) {
			_screenCounterTable[27] = 0;
			if (_checkpoint == 5) {
				_checkpoint = 6;
			}
		}
		_res->_screensState[27].s0 = num;
		_res->_resLvlScreenBackgroundDataTable[27].currentBackgroundId = num;
		_res->_resLvlScreenBackgroundDataTable[27].currentMaskId = num;
	}
}

void Level_pwr1::preScreenUpdate_pwr1_screen29() {
	if (_res->_currentScreenResourceNum == 29) {
		if (_checkpoint == 7) {
			_checkpoint = 8;
		}
	}
}

void Level_pwr1::preScreenUpdate_pwr1_screen31() {
	if (_res->_currentScreenResourceNum == 31) {
		if (_checkpoint == 8) {
			_checkpoint = 9;
		}
	}
}

void Level_pwr1::preScreenUpdate_pwr1_screen35() {
	if (_res->_currentScreenResourceNum == 35) {
		_screenCounterTable[35] = 0;
		if (!_paf->_skipCutscenes) {
			_paf->preload(5);
		}
	}
}

void Level_pwr1::preScreenUpdate(int num) {
	switch (num) {
	case 4:
		preScreenUpdate_pwr1_screen4();
		break;
	case 6:
		preScreenUpdate_pwr1_screen6();
		break;
	case 9:
		preScreenUpdate_pwr1_screen9();
		break;
	case 15:
		preScreenUpdate_pwr1_screen15();
		break;
	case 21:
		preScreenUpdate_pwr1_screen21();
		break;
	case 23:
		preScreenUpdate_pwr1_screen23();
		break;
	case 24:
		preScreenUpdate_pwr1_screen24();
		break;
	case 26:
		preScreenUpdate_pwr1_screen26();
		break;
	case 27:
		preScreenUpdate_pwr1_screen27();
		break;
	case 29:
		preScreenUpdate_pwr1_screen29();
		break;
	case 31:
		preScreenUpdate_pwr1_screen31();
		break;
	case 35:
		preScreenUpdate_pwr1_screen35();
		break;
	}
}

void Level_pwr1::initialize() {
	_g->loadTransformLayerData(Game::_pwr1_screenTransformData);
	_g->resetWormHoleSprites();
}

void Level_pwr1::terminate() {
	_g->unloadTransformLayerData();
}

const uint8_t Game::_pwr1_screenTransformLut[] = {
	1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0,
	1, 2, 1, 0, 1, 3, 1, 0, 1, 0, 1, 4, 1, 0, 1, 0, 1, 0,
	1, 5, 1, 0, 1, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0,
	0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void Level_pwr1::tick() {
	_video->_displayShadowLayer = Game::_pwr1_screenTransformLut[_res->_currentScreenResourceNum * 2] != 0;
	_g->updateWormHoleSprites();
}
