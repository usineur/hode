
// lar2_hod - "heart of darkness"

#include "game.h"
#include "level.h"
#include "paf.h"
#include "util.h"
#include "video.h"

static const CheckpointData _lar2_checkpointData[12] = {
	{ 111, 123, 0x300c,  48,  0,  0 },
	{  11, 123, 0x300c,  48,  2,  0 },
	{  17,  51, 0x300c,  48,  4,  0 },
	{  69,  91, 0x300c,  48,  6,  0 },
	{ 154,  91, 0x700c,  48,  6,  0 },
	{ 154,  91, 0x700c,  48,  6,  0 },
	{ 137,  35, 0x300c,  48, 11,  0 },
	{ 154,  91, 0x700c,  48,  6,  0 },
	{  70, 139, 0x700c,  48,  5,  0 },
	{ 106, 139, 0x700c,  48,  4,  0 },
	{  75, 107, 0x300c,  48, 15,  0 },
	{  75, 107, 0x300c,  48, 15,  0 }
};

static const uint8_t _lar2_screenStartData[64] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct Level_lar2: Level {
	virtual const CheckpointData *getCheckpointData(int num) const { return &_lar2_checkpointData[num]; }
	virtual const uint8_t *getScreenRestartData() const { return _lar2_screenStartData; }
	//virtual void initialize();
	//virtual void terminate();
	virtual void tick();
	virtual void preScreenUpdate(int screenNum);
	virtual void postScreenUpdate(int screenNum);
	virtual void setupScreenCheckpoint(int screenNum);

	bool postScreenUpdate_lar2_screen2_updateGateSwitches(BoundingBox *b);

	void postScreenUpdate_lar2_screen2();
	void postScreenUpdate_lar2_screen3();
	void postScreenUpdate_lar2_screen4();
	void postScreenUpdate_lar2_screen5();
	void postScreenUpdate_lar2_screen6();
	void postScreenUpdate_lar2_screen7();
	void postScreenUpdate_lar2_screen8();
	void postScreenUpdate_lar2_screen10();
	void postScreenUpdate_lar2_screen11();
	void postScreenUpdate_lar2_screen12();
	void postScreenUpdate_lar2_screen13();
	void postScreenUpdate_lar2_screen19();

	void preScreenUpdate_lar2_screen2();
	void preScreenUpdate_lar2_screen4();
	void preScreenUpdate_lar2_screen5();
	void preScreenUpdate_lar2_screen6();
	void preScreenUpdate_lar2_screen7();
	void preScreenUpdate_lar2_screen8();
	void preScreenUpdate_lar2_screen9();
	void preScreenUpdate_lar2_screen15();
	void preScreenUpdate_lar2_screen19();

	void setupScreenCheckpoint_lar2_screen19();
};

Level *Level_lar2_create() {
	return new Level_lar2;
}

static uint8_t _lar2_gatesData[10 * 4] = {
	0x02, 0x00, 0x00, 0x00, // screen 2
	0x02, 0x00, 0x00, 0x00, // screen 3
	0x02, 0x00, 0x00, 0x00, // screen 4
	0x12, 0x00, 0x00, 0x00, // screen 5
	0x02, 0x00, 0x00, 0x00, // screen 10
	0x02, 0x00, 0x00, 0x00, // screen 11
	0x02, 0x00, 0x00, 0x00, // screen 11
	0x02, 0x00, 0x00, 0x00, // screen 8
	0x12, 0x00, 0x00, 0x00, // screen 12
	0x02, 0x00, 0x00, 0x00  // screen 12
};

static BoundingBox _lar2_switchesBbox[13] = {
	{ 210, 155, 220, 159 },
	{ 224, 146, 234, 150 },
	{ 193,  84, 203,  88 },
	{ 209,  83, 219,  87 },
	{  65, 179,  75, 183 },
	{  65, 179,  75, 183 },
	{ 145, 179, 155, 183 },
	{ 145, 179, 155, 183 },
	{ 224, 179, 234, 183 },
	{  16,  84,  26,  88 },
	{  16,  84,  26,  88 },
	{  16, 164,  26, 168 },
	{  16, 164,  26, 168 }
};

static uint8_t _lar2_switchesData[13 * 4] = { // screenNum,state,spriteNum,gateNum
	0x03, 0x07, 0x00, 0x01, 0x0A, 0x07, 0x00, 0x04, 0x09, 0x03, 0x00, 0x07, 0x08, 0x03, 0x00, 0x07,
	0x0B, 0x17, 0x00, 0x06, 0x0B, 0x15, 0xFF, 0x05, 0x0B, 0x15, 0x01, 0x06, 0x0B, 0x17, 0xFF, 0x05,
	0x0B, 0x17, 0x02, 0x06, 0x0D, 0x05, 0x00, 0x09, 0x0D, 0x07, 0xFF, 0x08, 0x0D, 0x05, 0x01, 0x08,
	0x0D, 0x07, 0xFF, 0x09
};

static const uint8_t _lar2_setupScreen19Data[13 * 3] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x02,
	0x00, 0x00, 0x05, 0x02, 0x00, 0x02, 0x00, 0x00, 0x03, 0x00, 0x00, 0x09, 0x0C, 0x00, 0x0A, 0x0C,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

bool Level_lar2::postScreenUpdate_lar2_screen2_updateGateSwitches(BoundingBox *b) {
	bool ret = false;
	BoundingBox b1 = { 121, 158, 131, 162 };
	if (_g->clipBoundingBox(&b1, b)) {
		ret = true;
		_lar2_gatesData[0] &= 0xF;
		LvlObject *o = _g->findLvlObject2(0, 0, 2);
		if (o) {
			o->objectUpdateType = 7;
		}
	}
	BoundingBox b2 = { 170, 158, 180, 162 };
	if (_g->clipBoundingBox(&b2, b)) {
		ret = true;
		_lar2_gatesData[0] &= 0xF;
		LvlObject *o = _g->findLvlObject2(0, 1, 2);
		if (o) {
			o->objectUpdateType = 7;
		}
	}
	BoundingBox b3 = { 215, 158, 225, 162 };
	if (_g->clipBoundingBox(&b3, b)) {
		ret = true;
		_lar2_gatesData[0] &= 0xF;
		LvlObject *o = _g->findLvlObject2(0, 2, 2);
		if (o) {
			o->objectUpdateType = 7;
		}
	}
	return ret;
}

void Level_lar2::postScreenUpdate_lar2_screen2() {
	LvlObject *o = _g->findLvlObject(2, 0, 2);
	_g->updateGatesLar(o, _lar2_gatesData, 0);
	if (_res->_currentScreenResourceNum == 2) {
		bool ret = false;
		for (LvlObject *o = _g->_lvlObjectsList1; o; o = o->nextPtr) {
			if (o->spriteNum == 16 && o->screenNum == _res->_currentScreenResourceNum) {
				BoundingBox b;
				b.x1 = o->xPos;
				b.y1 = o->yPos;
				b.x2 = o->xPos + o->width  - 1;
				b.y2 = o->yPos + o->height - 1;
				if (postScreenUpdate_lar2_screen2_updateGateSwitches(&b)) {
					ret = true;
				}
			}
		}
		AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		if (postScreenUpdate_lar2_screen2_updateGateSwitches(&data->boundingBox) == 0) {
			BoundingBox b = { 107, 77, 117, 81 };
			if (_g->clipBoundingBox(&b, &data->boundingBox)) {
				LvlObject *o = _g->findLvlObject2(0, 3, 2);
				if (o) {
					o->objectUpdateType = 7;
				}
				if (!ret) {
					_lar2_gatesData[0] = (_lar2_gatesData[0] & 0xF) | 0x10;
				}
			}
		}
	}
}

void Level_lar2::postScreenUpdate_lar2_screen3() {
	LvlObject *o = _g->findLvlObject(2, 0, 3);
	_g->updateGatesLar(o, _lar2_gatesData, 1);
}

void Level_lar2::postScreenUpdate_lar2_screen4() {
	if (_g->_currentLevelCheckpoint == 8 && _checkpoint == 9) {
		_lar2_gatesData[4 * 2] = (_lar2_gatesData[4 * 2] & 0xF) | 0x10;
		if (!_paf->_skipCutscenes) {
			_paf->play(18);
			_paf->unload(18);
			_video->clearPalette();
		}
		_g->_currentLevelCheckpoint = _checkpoint; // bugfix: conditioned with _pafSkipCutscenes
		_g->setupScreen(_andyObject->screenNum);
	}
	LvlObject *o = _g->findLvlObject(2, 0, 4);
	_g->updateGatesLar(o, _lar2_gatesData, 2);
}

void Level_lar2::postScreenUpdate_lar2_screen5() {
	if (_g->_currentLevelCheckpoint == 7 && _checkpoint == 8) {
		_lar2_gatesData[4 * 3] &= 0xF;
	}
	LvlObject *o = _g->findLvlObject(2, 0, 5);
	_g->updateGatesLar(o, _lar2_gatesData, 3);
}

void Level_lar2::postScreenUpdate_lar2_screen6() {
	if (_res->_currentScreenResourceNum == 6) {
		if (_checkpoint == 7) {
			if (_g->_currentLevelCheckpoint == 6) {
				if (!_paf->_skipCutscenes) {
					_paf->play(17);
					_paf->unload(17);
					_video->clearPalette();
				}
				_g->_currentLevelCheckpoint = _checkpoint; // bugfix: conditioned with _pafSkipCutscenes
				_g->setupScreen(_andyObject->screenNum);
			}
		} else if (_checkpoint == 3) {
			if (_res->_screensState[6].s0 != 0) {
				if (!_paf->_skipCutscenes) {
					_paf->play(15);
					_paf->unload(15);
					_video->clearPalette();
				}
				_res->_screensState[6].s0 = 0; // bugfix: conditioned with _pafSkipCutscenes
				_g->setupScreen(_andyObject->screenNum);
			}
		}
	}
}

void Level_lar2::postScreenUpdate_lar2_screen7() {
	if (_res->_currentScreenResourceNum == 7) {
		if (_checkpoint == 5 && _res->_screensState[7].s0 > 1) {
			if (!_paf->_skipCutscenes) {
				_paf->play(16);
				_paf->unload(16);
				_video->clearPalette();
			}
			_res->_screensState[7].s0 = 1; // bugfix: conditioned with _pafSkipCutscenes
			_g->setupScreen(_andyObject->screenNum);
		}
	}
}

void Level_lar2::postScreenUpdate_lar2_screen8() {
	LvlObject *o = _g->findLvlObject(2, 0, 8);
	_g->updateGatesLar(o, _lar2_gatesData, 7);
}

void Level_lar2::postScreenUpdate_lar2_screen10() {
	LvlObject *o = _g->findLvlObject(2, 0, 10);
	_g->updateGatesLar(o, _lar2_gatesData, 4);
}

void Level_lar2::postScreenUpdate_lar2_screen11() {
	LvlObject *o = _g->findLvlObject(2, 0, 11);
	_g->updateGatesLar(o, _lar2_gatesData, 5);
	o = _g->findLvlObject(2, 1, 11);
	_g->updateGatesLar(o, _lar2_gatesData, 6);
	int offset = 0x18;
	if ((_lar2_switchesData[0x11] & 1) == 0 && (_lar2_switchesData[0x11] & 0x40) != 0 && (_lar2_switchesData[0x19] & 1) == 0) {
		_lar2_switchesData[0x19] = (_lar2_switchesData[0x19] | 1) & ~0x40;
		offset = 0x1C;
		_lar2_switchesData[0x1D] = (_lar2_switchesData[0x1D] | 1) & ~0x40;
	}
	if ((_lar2_switchesData[offset + 1] & 1) == 0 && (_lar2_switchesData[offset + 1] & 0x40) != 0) {
		if ((_lar2_switchesData[0x21] & 1) != 0) {
			goto next;
		}
		_lar2_switchesData[0x21] = (_lar2_switchesData[0x21] | 1) & ~0x40;
	}
	if ((_lar2_switchesData[0x21] & 1) == 0 && (_lar2_switchesData[0x21] & 0x40) != 0 && (_lar2_switchesData[offset + 1] & 1) == 0) {
		_lar2_switchesData[offset + 1] = (_lar2_switchesData[offset + 1] | 1) & ~0x40;
		_lar2_switchesData[0x1D] = (_lar2_switchesData[0x1D] | 1) & ~0x40;
		offset = 0x1C;
	}
next:
	if ((_lar2_switchesData[offset + 1] & 1) == 0 && (_lar2_switchesData[offset + 1] & 0x40) != 0 && (_lar2_switchesData[0x11] & 1) == 0) {
		_lar2_switchesData[0x11] = (_lar2_switchesData[0x11] | 1) & ~0x40;
		_lar2_switchesData[0x15] = (_lar2_switchesData[0x15] | 1) & ~0X40;
	}
}

void Level_lar2::postScreenUpdate_lar2_screen12() {
	LvlObject *o = _g->findLvlObject(2, 0, 12);
	_g->updateGatesLar(o, _lar2_gatesData, 8);
	o = _g->findLvlObject(2, 1, 12);
	_g->updateGatesLar(o, _lar2_gatesData, 9);
	if (_res->_currentScreenResourceNum == 12) {
		BoundingBox b1 = { 65, 84, 75, 88 };
		AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		if (_g->clipBoundingBox(&b1, &data->boundingBox)) {
			_lar2_gatesData[4 * 8] &= 0xF;
			o = _g->findLvlObject2(0, 0, 12);
			if (o) {
				o->objectUpdateType = 7;
			}
		} else {
			BoundingBox b2 = { 65, 163, 75, 167 };
			if (_g->clipBoundingBox(&b2, &data->boundingBox)) {
				_lar2_gatesData[4 * 9] &= 0xF;
				o = _g->findLvlObject2(0, 1, 12);
				if (o) {
					o->objectUpdateType = 7;
				}
			}
		}
	}
}

void Level_lar2::postScreenUpdate_lar2_screen13() {
	if (_res->_currentScreenResourceNum == 13) {
		int offset = 0x2C;
		if ((_lar2_switchesData[0x25] & 1) == 0 && (_lar2_switchesData[0x25] & 0x40) != 0 && (_lar2_switchesData[0x2D] & 1) == 0) {
			offset = 0x30;
			_lar2_switchesData[0x2D] = (_lar2_switchesData[0x2D] | 1) & ~0x40;
			_lar2_switchesData[0x31] = (_lar2_switchesData[0x31] | 1) & ~0x40;
		}
		if ((_lar2_switchesData[offset + 1] & 1) == 0 && (_lar2_switchesData[offset + 1] & 0x40) != 0 && (_lar2_switchesData[0x25] & 1) == 0) {
			_lar2_switchesData[0x25] = (_lar2_switchesData[0x25] | 1) & ~0x40;
			_lar2_switchesData[0x29] = (_lar2_switchesData[0x29] | 1) & ~0x40;
		}
	}
}

void Level_lar2::postScreenUpdate_lar2_screen19() {
	if (_res->_currentScreenResourceNum == 19) {
		if (_g->_currentLevelCheckpoint == 10 && _checkpoint == 11) {
			if (!_paf->_skipCutscenes) {
				_paf->play(19);
				_paf->unload(19);
			}
			_video->clearPalette();
			_g->_endLevel = true;
		}
	}
}

void Level_lar2::postScreenUpdate(int num) {
	switch (num) {
	case 2:
		postScreenUpdate_lar2_screen2();
		break;
	case 3:
		postScreenUpdate_lar2_screen3();
		break;
	case 4:
		postScreenUpdate_lar2_screen4();
		break;
	case 5:
		postScreenUpdate_lar2_screen5();
		break;
	case 6:
		postScreenUpdate_lar2_screen6();
		break;
	case 7:
		postScreenUpdate_lar2_screen7();
		break;
	case 8:
		postScreenUpdate_lar2_screen8();
		break;
	case 10:
		postScreenUpdate_lar2_screen10();
		break;
	case 11:
		postScreenUpdate_lar2_screen11();
		break;
	case 12:
		postScreenUpdate_lar2_screen12();
		break;
	case 13:
		postScreenUpdate_lar2_screen13();
		break;
	case 19:
		postScreenUpdate_lar2_screen19();
		break;
	}
}

void Level_lar2::preScreenUpdate_lar2_screen2() {
	LvlObject *o = _g->findLvlObject(2, 0, 2);
	_g->updateGatesLar(o, _lar2_gatesData, 0);
	if (_res->_currentScreenResourceNum == 2) {
		if (_checkpoint == 0) {
			_checkpoint = 1;
		}
	}
}

void Level_lar2::preScreenUpdate_lar2_screen4() {
	if (_res->_currentScreenResourceNum == 4) {
		if (_checkpoint == 1) {
			_checkpoint = 2;
		}
		if (_checkpoint >= 2) {
			_lar2_gatesData[4 * 1] &= 0xF;
			if (_checkpoint == 8) {
				_lar2_gatesData[4 * 2] &= 0xF;
				if (!_paf->_skipCutscenes) {
					_paf->preload(18);
				}
			}
		}
	}
}

void Level_lar2::preScreenUpdate_lar2_screen5() {
	if (_res->_currentScreenResourceNum == 5) {
		if (_checkpoint == 7) {
			_lar2_gatesData[4 * 3] = (_lar2_gatesData[4 * 3] & 0xF) | 0x10;
		} else if (_checkpoint >= 3) {
			_lar2_gatesData[4 * 3] &= 0xF;
		}
	}
}

void Level_lar2::preScreenUpdate_lar2_screen6() {
	if (_res->_currentScreenResourceNum == 6) {
		if (_checkpoint == 2) {
			_checkpoint = 3;
		}
		if (_checkpoint == 3) {
			if (!_paf->_skipCutscenes) {
				_paf->preload(15);
			}
			_lar2_gatesData[4 * 3] &= 0xF; // bugfix: conditioned with _pafSkipCutscenes
		} else if (_checkpoint == 6) {
			if (!_paf->_skipCutscenes) {
				_paf->preload(17);
			}
		}
	}
}

void Level_lar2::preScreenUpdate_lar2_screen7() {
	if (_res->_currentScreenResourceNum == 7) {
		if (_checkpoint >= 4 && _checkpoint < 7) {
			_res->_screensState[7].s0 = 1;
			_g->updateBackgroundPsx(1);
		} else {
			_res->_screensState[7].s0 = 0;
			_g->updateBackgroundPsx(0);
		}
		if (_checkpoint == 5) {
			if (!_paf->_skipCutscenes) {
				_paf->preload(16);
			}
		}
		if (_res->_screensState[7].s0 != 0) {
			_res->_resLvlScreenBackgroundDataTable[7].currentBackgroundId = 1;
			_res->_resLvlScreenBackgroundDataTable[7].currentMaskId = 1;
		} else {
			_res->_resLvlScreenBackgroundDataTable[7].currentBackgroundId = 0;
			_res->_resLvlScreenBackgroundDataTable[7].currentMaskId = 0;
		}
	}
}

void Level_lar2::preScreenUpdate_lar2_screen8() {
	if (_res->_currentScreenResourceNum == 8 && _lar2_gatesData[4 * 7 + 2] > 1) {
		_lar2_gatesData[4 * 7 + 2] = 1;
	}
	LvlObject *o = _g->findLvlObject(2, 0, 8);
	_g->updateGatesLar(o, _lar2_gatesData, 7);
}

void Level_lar2::preScreenUpdate_lar2_screen9() {
	if (_res->_currentScreenResourceNum == 9) {
		_lar2_gatesData[4 * 7 + 2] = 36; // gate closing countdown
	}
}

void Level_lar2::preScreenUpdate_lar2_screen15() {
	if (_res->_currentScreenResourceNum == 15) {
		if (_checkpoint == 9) {
			_checkpoint = 10;
		}
	}
}

void Level_lar2::preScreenUpdate_lar2_screen19() {
	if (_res->_currentScreenResourceNum == 19) {
		if (_checkpoint == 10) {
			if (!_paf->_skipCutscenes) {
				_paf->preload(19);
			}
		}
	}
}

void Level_lar2::preScreenUpdate(int num) {
	switch (num) {
	case 2:
		preScreenUpdate_lar2_screen2();
		break;
	case 4:
		preScreenUpdate_lar2_screen4();
		break;
	case 5:
		preScreenUpdate_lar2_screen5();
		break;
	case 6:
		preScreenUpdate_lar2_screen6();
		break;
	case 7:
		preScreenUpdate_lar2_screen7();
		break;
	case 8:
	case 11:
		preScreenUpdate_lar2_screen8();
		break;
	case 9:
		preScreenUpdate_lar2_screen9();
		break;
	case 15:
		preScreenUpdate_lar2_screen15();
		break;
	case 19:
		preScreenUpdate_lar2_screen19();
		break;
	}
}

void Level_lar2::tick() {
	_g->updateSwitchesLar(13, _lar2_switchesData, _lar2_switchesBbox, _lar2_gatesData);
}

void Level_lar2::setupScreenCheckpoint_lar2_screen19() {
	const int switchIndex = _lar2_setupScreen19Data[_checkpoint * 3 + 1];
	for (int i = switchIndex; i < 13; ++i) {
		const int offset = i * 4 + 1;
		_lar2_switchesData[offset] = (_lar2_switchesData[offset] & ~0x40) | 1;
	}
	for (int i = switchIndex; i != 0; --i) {
		const int offset = (i - 1) * 4 + 1;
		_lar2_switchesData[offset] = (_lar2_switchesData[offset] & ~1) | 0x40;
	}
	static const uint8_t data[10] = { 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
	const int gateIndex = _lar2_setupScreen19Data[_checkpoint * 3];
	for (int i = gateIndex; i < 10; ++i) {
		const int num = i;
		_lar2_gatesData[num * 4] = (data[num] << 4) | 2;
		const uint32_t mask = 1 << num;
		if (_lar2_gatesData[num * 4] & 0xF0) { // bugfix: original uses _lar1_gatesData
			_g->_mstAndyVarMask &= ~mask;
		} else {
			_g->_mstAndyVarMask |= mask;
		}
		_g->_mstLevelGatesMask |= mask;
	}
	for (int i = gateIndex; i != 0; --i) {
		const int num = i - 1;
		_lar2_gatesData[num * 4] = (((data[num] == 0) ? 1 : 0) << 4) | 2;
		const uint32_t mask = 1 << num;
		if (_lar2_gatesData[num * 4] & 0xF0) { // bugfix: original uses _lar1_gatesData
			_g->_mstAndyVarMask &= ~mask;
		} else {
			_g->_mstAndyVarMask |= mask;
		}
		_g->_mstLevelGatesMask |= mask;
	}
	if (g_debugMask & kDebug_SWITCHES) {
		_g->dumpSwitchesLar(13, _lar2_switchesData, _lar2_switchesBbox, 10, _lar2_gatesData);
	}
}

void Level_lar2::setupScreenCheckpoint(int num) {
	switch (num) {
	case 19:
		setupScreenCheckpoint_lar2_screen19();
		break;
	}
}
