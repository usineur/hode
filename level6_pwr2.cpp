
// pwr2_hod - "caverns of doom"

#include "game.h"
#include "level.h"
#include "paf.h"
#include "video.h"

static const CheckpointData _pwr2_checkpointData[3] = {
	{ 127, 121, 0x300c, 232,  0,  2 },
	{  10, 132, 0x300c, 232,  3,  2 },
	{ 104, 129, 0x300c, 232,  7,  2 }
};

static const uint8_t _pwr2_screenStartData[32] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct Level_pwr2: Level {
	virtual const CheckpointData *getCheckpointData(int num) const { return &_pwr2_checkpointData[num]; }
	virtual const uint8_t *getScreenRestartData() const { return _pwr2_screenStartData; }
	virtual void initialize();
	virtual void terminate();
	virtual void tick();
	virtual void preScreenUpdate(int screenNum);
	virtual void postScreenUpdate(int screenNum);
	//virtual void setupScreenCheckpoint(int screenNum);

	void postScreenUpdate_pwr2_screen2();
	void postScreenUpdate_pwr2_screen3();
	void postScreenUpdate_pwr2_screen5();
	void postScreenUpdate_pwr2_screen8();

	void preScreenUpdate_pwr2_screen2();
	void preScreenUpdate_pwr2_screen3();
	void preScreenUpdate_pwr2_screen5();
	void preScreenUpdate_pwr2_screen7();
};

Level *Level_pwr2_create() {
	return new Level_pwr2;
}

void Level_pwr2::postScreenUpdate_pwr2_screen2() {
	uint8_t mask;
	switch (_res->_screensState[2].s0) {
	case 1:
		mask = 1;
		break;
	case 2:
		mask = 2;
		break;
	default:
		mask = 0;
		break;
	}
	_res->_resLvlScreenBackgroundDataTable[2].currentMaskId = mask;
	if (_res->_screensState[2].s3 != mask) {
		_g->setupScreenMask(2);
	}
}

void Level_pwr2::postScreenUpdate_pwr2_screen3() {
	if (_res->_currentScreenResourceNum == 3) {
		if ((_andyObject->flags1 & 0x30) == 0) {
			AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			BoundingBox b = { 115, 27, 127, 55 };
			if (_g->clipBoundingBox(&b, &data->boundingBox)) {
				const uint8_t flags = _andyObject->flags0;
				if ((flags & 0x1F) == 0 && (flags & 0xE0) == 0xE0) {
					LvlObject *o = _g->findLvlObject2(0, 0, 3);
					if (o) {
						o->objectUpdateType = 7;
					}
				} else {
					LvlObject *o = _g->findLvlObject2(0, 0, 3);
					if (o) {
						if (o->objectUpdateType == 4) {
							_res->_screensState[2].s0 = 2;
						} else {
							_g->setAndySpecialAnimation(3);
						}
					}
				}
			}
		}
	}
}

void Level_pwr2::postScreenUpdate_pwr2_screen5() {
	if (_res->_screensState[5].s0 == 1) {
		_res->_screensState[5].s0 = 2;
		if (_checkpoint == 1) {
			_checkpoint = 2;
		}
		if (!_paf->_skipCutscenes) {
			_paf->play(9);
			_paf->unload(9);
		}
		_video->clearPalette();
		_g->restartLevel();
	}
}

void Level_pwr2::postScreenUpdate_pwr2_screen8() {
	if (_res->_screensState[8].s0 != 0) {
		if (!_paf->_skipCutscenes) {
			_paf->play(10);
			_paf->unload(10);
		}
		_video->clearPalette();
		_g->_endLevel = true;
	}
}

void Level_pwr2::postScreenUpdate(int num) {
	switch (num) {
	case 2:
		postScreenUpdate_pwr2_screen2();
		break;
	case 3:
		postScreenUpdate_pwr2_screen3();
		break;
	case 5:
		postScreenUpdate_pwr2_screen5();
		break;
	case 8:
	case 9:
	case 10:
	case 11:
		postScreenUpdate_pwr2_screen8();
		break;
	}
}

void Level_pwr2::preScreenUpdate_pwr2_screen2() {
	if (_checkpoint == 1) {
		if (_res->_screensState[2].s0 == 0) {
			_res->_screensState[2].s0 = 2;
			_res->_resLvlScreenBackgroundDataTable[2].currentMaskId = 2;
		}
	}
}

void Level_pwr2::preScreenUpdate_pwr2_screen3() {
	if (_res->_currentScreenResourceNum == 3 && _checkpoint == 0) {
		AndyLvlObjectData *data = (AndyLvlObjectData *)_g->getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		BoundingBox b = { 0, 103, 122, 191 };
		if (_g->clipBoundingBox(&b, &data->boundingBox)) {
			_checkpoint = 1;
		}
	}
}

void Level_pwr2::preScreenUpdate_pwr2_screen5() {
	if (_res->_currentScreenResourceNum == 5) {
		if (!_paf->_skipCutscenes) {
			_paf->preload(9);
		}
	}
}

void Level_pwr2::preScreenUpdate_pwr2_screen7() {
	if (_res->_currentScreenResourceNum == 7) {
		_res->_screensState[5].s0 = 2;
		if (!_paf->_skipCutscenes) {
			_paf->preload(10);
		}
	}
}

void Level_pwr2::preScreenUpdate(int num) {
	switch (num) {
	case 2:
		preScreenUpdate_pwr2_screen2();
		break;
	case 3:
		preScreenUpdate_pwr2_screen3();
		break;
	case 5:
		preScreenUpdate_pwr2_screen5();
		break;
	case 7:
		preScreenUpdate_pwr2_screen7();
		break;
	}
}

void Level_pwr2::initialize() {
	_g->loadTransformLayerData(Game::_pwr1_screenTransformData);
}

void Level_pwr2::terminate() {
	_g->unloadTransformLayerData();
}

const uint8_t Game::_pwr2_screenTransformLut[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 9, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void Level_pwr2::tick() {
	_video->_displayShadowLayer = Game::_pwr2_screenTransformLut[_res->_currentScreenResourceNum * 2] != 0;
}
