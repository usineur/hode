/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "game.h"
#include "fileio.h"
#include "level.h"
#include "lzw.h"
#include "paf.h"
#include "screenshot.h"
#include "system.h"
#include "util.h"
#include "video.h"

// starting level cutscene number
static const uint8_t _cutscenes[] = { 0, 2, 4, 5, 6, 8, 10, 14, 19 };

Game::Game(const char *dataPath, const char *savePath, uint32_t cheats)
	: _fs(dataPath, savePath) {

	_level = 0;
	_res = new Resource(&_fs);
	_paf = new PafPlayer(&_fs);
	_rnd.setSeed();
	_video = new Video();
	_cheats = cheats;

	_frameMs = kFrameTimeStamp;
	_difficulty = 1;
	_loadingScreenEnabled = true;

	memset(_screenLvlObjectsList, 0, sizeof(_screenLvlObjectsList));
	_andyObject = 0;
	_plasmaExplosionObject = 0;
	_plasmaCannonObject = 0;
	memset(_spritesTable, 0, sizeof(_spritesTable));
	memset(_typeSpritesList, 0, sizeof(_typeSpritesList));

	_directionKeyMask = 0;
	_actionKeyMask = 0;

	_currentScreen = 0;

	_lvlObjectsList0 = 0;
	_lvlObjectsList1 = 0;
	_lvlObjectsList2 = 0;
	_lvlObjectsList3 = 0;
	memset(_screenMaskBuffer, 0, sizeof(_screenMaskBuffer));
	memset(_shootLvlObjectDataTable, 0, sizeof(_shootLvlObjectDataTable));
	_mstAndyCurrentScreenNum = -1;
	_plasmaCannonDirection = 0;
	_andyActionKeyMaskAnd = 0xFF;
	_andyActionKeyMaskOr = 0;
	_andyDirectionKeyMaskAnd = 0xFF;
	_andyDirectionKeyMaskOr = 0;

	_mstDisabled = false;
	_specialAnimMask = 0; // original only clears ~0x30
	_mstCurrentAnim = 0;
	_mstOriginPosX = Video::W / 2;
	_mstOriginPosY = Video::H / 2;
	memset(_declaredLvlObjectsList, 0, sizeof(_declaredLvlObjectsList));
	_declaredLvlObjectsNextPtr = 0;
	_declaredLvlObjectsListCount = 0;

	memset(_animBackgroundDataTable, 0, sizeof(_animBackgroundDataTable));
	_animBackgroundDataCount = 0;

	memset(_monsterObjects1Table, 0, sizeof(_monsterObjects1Table));
	memset(_monsterObjects2Table, 0, sizeof(_monsterObjects2Table));

	_sssDisabled = false;
	_snd_muted = false;
	_snd_masterPanning = kDefaultSoundPanning;
	_snd_masterVolume = kDefaultSoundVolume;
	_sssObjectsCount = 0;
	_sssObjectsList1 = 0;
	_sssObjectsList2 = 0;
	_playingSssObjectsMax = 16; // 10 if (lowMemory || slowCPU)
}

Game::~Game() {
	delete _paf;
	delete _res;
	delete _video;
}

void Game::resetShootLvlObjectDataTable() {
	_shootLvlObjectDataNextPtr = &_shootLvlObjectDataTable[0];
	for (int i = 0; i < kMaxShootLvlObjectData - 1; ++i) {
		_shootLvlObjectDataTable[i].nextPtr = &_shootLvlObjectDataTable[i + 1];
	}
	_shootLvlObjectDataTable[kMaxShootLvlObjectData - 1].nextPtr = 0;
}

void Game::clearShootLvlObjectData(LvlObject *ptr) {
	ShootLvlObjectData *dat = (ShootLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeShoot);
	dat->nextPtr = _shootLvlObjectDataNextPtr;
	_shootLvlObjectDataNextPtr = dat;
	ptr->dataPtr = 0;
}

void Game::setShakeScreen(int type, int counter) {
	static const uint8_t table1[] = { 1, 2, 3, 4 };
	static const uint8_t table2[] = { 2, 3, 4, 5 };
	static const uint8_t table3[] = { 3, 4, 5, 8 };
	switch (type) {
	case 1:
		_shakeScreenTable = table1;
		break;
	case 2:
		_shakeScreenTable = table2;
		break;
	case 3:
		_shakeScreenTable = table3;
		break;
	default:
		return;
	}
	_shakeScreenDuration = counter;
}

void Game::fadeScreenPalette() {
	if (!_fadePalette) {
		assert(_levelRestartCounter != 0);
		for (int i = 0; i < 256 * 3; ++i) {
			_video->_fadePaletteBuffer[i] = _video->_displayPaletteBuffer[i] / _levelRestartCounter;
		}
		_fadePalette = true;
	} else {
		if (_levelRestartCounter != 0) {
			_snd_masterVolume -= _snd_masterVolume / _levelRestartCounter;
			if (_snd_masterVolume < 0) {
				_snd_masterVolume = 0;
			}
		}
		--_levelRestartCounter;
	}
	for (int i = 0; i < 256 * 3; ++i) {
		int color = _video->_displayPaletteBuffer[i] - _video->_fadePaletteBuffer[i];
		if (color < 0) {
			 color = 0;
		}
		_video->_displayPaletteBuffer[i] = color;
	}
	_video->_paletteNeedRefresh = true;
}

void Game::shakeScreen() {
	if (_video->_displayShadowLayer) {
		const int num = (_currentLevel == kLvl_lava || _currentLevel == kLvl_lar1) ? 1 : 4;
		transformShadowLayer(num);
	}
	if (_shakeScreenDuration != 0) {
		--_shakeScreenDuration;
		if (_shakeScreenDuration != 0) {
			const int r1 = _rnd.update() & 3;
			const int r2 = _rnd.update() & 3;
			int dx = _shakeScreenTable[r2] & ~3;
			if (r1 & 1) {
				dx = -dx;
			}
			int dy = _shakeScreenTable[r1];
			if (_shakeScreenDuration & 1) {
				dy = -dy;
			}
			g_system->shakeScreen(dx, dy);
		}
	}
	if (_levelRestartCounter != 0) {
		fadeScreenPalette();
	}
}

static BoundingBox _screenTransformRects[] = {
	{  0,  0,   0,   0 },
	{  0,  0, 255, 128 },
	{  0, 10, 154, 126 },
	{  0,  0, 107,  36 },
	{  0,  1,  78,  29 },
	{ 14,  7, 249,  72 },
	{ 14,  0, 255,  72 },
	{  0,  0, 255, 144 },
	{  0,  0, 255, 144 },
	{  0, 69, 255, 191 }
};

void Game::transformShadowLayer(int delta) {
	const uint8_t *src = _video->_transformShadowBuffer + _video->_transformShadowLayerDelta; // vg
	uint8_t *dst = _video->_shadowLayer; // va
	_video->_transformShadowLayerDelta += delta; // overflow/wrap at 255
	for (int y = 0; y < Video::H; ++y) {
		if (0) {
			for (int x = 0; x < Video::W - 6; ++x) {
				const int offset = x + *src++;
				*dst++ = _video->_frontLayer[y * Video::W + offset];
			}
			memset(dst, 0xC4, 6);
			dst += 6;
			src += 6;
		} else {
			for (int x = 0; x < Video::W; ++x) {
				const int offset = MIN(255, x + *src++);
				*dst++ = _video->_frontLayer[y * Video::W + offset];
			}
		}
	}
	uint8_t r = 0;
	if (_currentLevel == kLvl_pwr1) {
		r = _pwr1_screenTransformLut[_res->_currentScreenResourceNum * 2 + 1];
	} else if (_currentLevel == kLvl_pwr2) {
		r = _pwr2_screenTransformLut[_res->_currentScreenResourceNum * 2 + 1];
	}
	if (r != 0) {
		assert(r < ARRAYSIZE(_screenTransformRects));
		const BoundingBox *b = &_screenTransformRects[r];
		const int offset = b->y1 * Video::W + b->x1;
		src = _video->_frontLayer + offset;
		dst = _video->_shadowLayer + offset;
		for (int y = b->y1; y < b->y2; ++y) {
			for (int x = b->x1; x < b->x2; ++x) {
				dst[x] = src[x];
			}
			dst += Video::W;
			src += Video::W;
		}
	}
}

void Game::loadTransformLayerData(const uint8_t *data) {
	assert(!_video->_transformShadowBuffer);
	_video->_transformShadowBuffer = (uint8_t *)malloc(256 * 192 + 256);
	const int size = decodeLZW(data, _video->_transformShadowBuffer);
	assert(size == 256 * 192);
	memcpy(_video->_transformShadowBuffer + 256 * 192, _video->_transformShadowBuffer, 256);
}

void Game::unloadTransformLayerData() {
	free(_video->_transformShadowBuffer);
	_video->_transformShadowBuffer = 0;
}

void Game::decodeShadowScreenMask(LvlBackgroundData *lvl) {
	uint8_t *dst = _video->_shadowScreenMaskBuffer;
	for (int i = lvl->currentShadowId; i < lvl->shadowCount; ++i) {
		uint8_t *src = lvl->backgroundMaskTable[i];
		if (src) {
			const int decodedSize = decodeLZW(src + 2, dst);

			_shadowScreenMasksTable[i].dataSize = READ_LE_UINT32(dst);

			// header : 20 bytes
			// projectionData : w * h * sizeof(uint16_t) - for a given (x, y) returns the casted (x, y)
			// paletteData : 256 (only the first 144 bytes are read)

			_shadowScreenMasksTable[i].projectionDataPtr = dst + 0x14 + READ_LE_UINT32(dst + 4);
			_shadowScreenMasksTable[i].shadowPalettePtr = dst + 0x14 + READ_LE_UINT32(dst + 8);
			const int x = _shadowScreenMasksTable[i].x = READ_LE_UINT16(dst + 0xC);
			const int y = _shadowScreenMasksTable[i].y = READ_LE_UINT16(dst + 0xE);
			const int w = _shadowScreenMasksTable[i].w = READ_LE_UINT16(dst + 0x10);
			const int h = _shadowScreenMasksTable[i].h = READ_LE_UINT16(dst + 0x12);

			debug(kDebug_GAME, "shadow screen mask #%d pos %d,%d dim %d,%d size %d", i, x, y, w, h, decodedSize);

			const int size = w * h;
			src = _shadowScreenMasksTable[i].projectionDataPtr + 2;
			for (int j = 1; j < size; ++j) {
				const int16_t offset = (int16_t)READ_LE_UINT16(src - 2) + (int16_t)READ_LE_UINT16(src);
				// fprintf(stdout, "shadow #%d offset #%d 0x%x 0x%x\n", i, j, READ_LE_UINT16(src), offset);
				WRITE_LE_UINT16(src, offset);
				src += 2;
			}

			const int shadowPaletteSize = decodedSize - 20 - w * h * sizeof(uint16_t);
			assert(shadowPaletteSize >= 144);

			_video->buildShadowColorLookupTable(_shadowScreenMasksTable[i].shadowPalettePtr, _video->_shadowColorLookupTable);
			dst += decodedSize;
		}
	}
}

// a: type/source (0, 1, 2) b: num/index (3, monster1Index, monster2.monster1Index)
void Game::playSound(int num, LvlObject *ptr, int a, int b) {
	MixerLock ml(&_mix);
	if (num < _res->_sssHdr.infosDataCount) {
		debug(kDebug_GAME, "playSound num %d/%d a=%d b=%d", num, _res->_sssHdr.infosDataCount, a, b);
		_currentSoundLvlObject = ptr;
		playSoundObject(&_res->_sssInfosData[num], a, b);
		_currentSoundLvlObject = 0;
	}
}

void Game::removeSound(LvlObject *ptr) {
	MixerLock ml(&_mix);
	for (int i = 0; i < _sssObjectsCount; ++i) {
		if (_sssObjectsTable[i].lvlObject == ptr) {
			_sssObjectsTable[i].lvlObject = 0;
		}
	}
	ptr->sssObject = 0;
}

void Game::setupBackgroundBitmap() {
	LvlBackgroundData *lvl = &_res->_resLvlScreenBackgroundDataTable[_res->_currentScreenResourceNum];
	const int num = lvl->currentBackgroundId;
	const uint8_t *pal = lvl->backgroundPaletteTable[num];
	lvl->backgroundPaletteId = READ_LE_UINT16(pal); pal += 2;
	const uint8_t *pic = lvl->backgroundBitmapTable[num];
	lvl->backgroundBitmapId = READ_LE_UINT16(pic); pic += 2;
	if (lvl->backgroundPaletteId != 0xFFFF) {
		playSound(lvl->backgroundPaletteId, 0, 0, 3);
	}
	if (lvl->backgroundBitmapId != 0xFFFF) {
		playSound(lvl->backgroundBitmapId, 0, 0, 3);
	}
	if (_res->_isPsx) {
		const int size = Video::W * Video::H * sizeof(uint16_t);
		_video->decodeBackgroundPsx(pic + 2, size, Video::W, Video::H);
	} else {
		decodeLZW(pic, _video->_backgroundLayer);
	}
	if (lvl->shadowCount != 0) {
		decodeShadowScreenMask(lvl);
	}
	for (int i = 0; i < 256 * 3; ++i) {
		_video->_displayPaletteBuffer[i] = pal[i] << 8;
	}
	_video->_paletteNeedRefresh = true;
}

void Game::addToSpriteList(LvlObject *ptr) {
	Sprite *spr = _spritesNextPtr;
	if (spr) {
		const uint8_t num = _res->_currentScreenResourceNum;
		const uint8_t *grid = _res->_screensGrid[num];

		LvlObjectData *dat = ptr->levelData0x2988;
		LvlAnimHeader *ah = (LvlAnimHeader *)(dat->animsInfoData + kLvlAnimHdrOffset) + ptr->anim;
		LvlAnimSeqHeader *ash = (LvlAnimSeqHeader *)(dat->animsInfoData + ah->seqOffset) + ptr->frame;

		spr->num = (((ash->flags1 ^ ptr->flags1) & 0xFFF0) << 10) | (ptr->flags2 & 0x3FFF);

		int index = ptr->screenNum;
		spr->xPos = ptr->xPos;
		spr->yPos = ptr->yPos;
		if (index == grid[kPosTopScreen]) {
			spr->yPos -= Video::H;
		} else if (index == grid[kPosBottomScreen]) {
			spr->yPos += Video::H;
		} else if (index == grid[kPosRightScreen]) {
			spr->xPos += Video::W;
		} else if (index == grid[kPosLeftScreen]) {
			spr->xPos -= Video::W;
		} else if (index != num) {
			return;
		}
		if (spr->xPos >= Video::W || spr->xPos + ptr->width < 0) {
			return;
		}
		if (spr->yPos >= Video::H || spr->yPos + ptr->height < 0) {
			return;
		}
		if (_currentLevel == kLvl_isld && ptr->spriteNum == 2) {
			AndyLvlObjectData *dataPtr = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
			spr->xPos += dataPtr->dxPos;
		}
		if (ptr->bitmapBits) {
			spr->w = ptr->width;
			spr->h = ptr->height;
			spr->bitmapBits = ptr->bitmapBits;
			_spritesNextPtr = spr->nextPtr;
			index = (ptr->flags2 & 31);
			spr->nextPtr = _typeSpritesList[index];
			_typeSpritesList[index] = spr;
		}
	}
}

int16_t Game::calcScreenMaskDy(int16_t xPos, int16_t yPos, int num) {
	if (xPos < 0) {
		xPos += Video::W;
		num = _res->_screensGrid[num][kPosLeftScreen];
	} else if (xPos >= Video::W) {
		xPos -= Video::W;
		num = _res->_screensGrid[num][kPosRightScreen];
	}
	if (num != kNoScreen) {
		if (yPos < 0) {
			yPos += Video::H;
			num = _res->_screensGrid[num][kPosTopScreen];
		} else if (yPos >= Video::H) {
			yPos -= Video::H;
			num = _res->_screensGrid[num][kPosBottomScreen];
		}
	}
	uint8_t var1 = 0xFF - (yPos & 7);
	if (num == kNoScreen) {
		return var1;
	}
	int vg = screenMaskOffset(_res->_screensBasePos[num].u + xPos, _res->_screensBasePos[num].v + yPos);
	int vf = screenGridOffset(xPos, yPos);
	if (_screenMaskBuffer[vg - 512] & 1) {
		vf -= 32;
		var1 -= 8;
	} else if (_screenMaskBuffer[vg] & 1) {
		/* nothing to do */
	} else if (_screenMaskBuffer[vg + 512] & 1) {
		vf += 32;
		var1 += 8;
	} else if (_screenMaskBuffer[vg + 1024] & 1) {
		vf += 64;
		var1 += 16;
	} else {
		return 0;
	}
	int _dl = _res->findScreenGridIndex(num);
	if (_dl < 0) {
		return (int8_t)(var1 + 4);
	}
	const uint8_t *p = _res->_resLevelData0x470CTablePtrData + (xPos & 7);
	return (int8_t)(var1 + p[_screenPosTable[_dl][vf] * 8]);
}

void Game::setupScreenPosTable(uint8_t num) {
	const uint8_t *src = _res->_screensGrid[num];
	for (int i = 0; i < 4; ++i) {
		if (src[i] != kNoScreen) {
			int index = _res->_resLvlScreenBackgroundDataTable[src[i]].currentMaskId;
			const uint8_t *p = _res->getLvlScreenPosDataPtr(src[i] * 4 + index);
			if (p) {
				Video::decodeRLE(p, _screenPosTable[i], 32 * 24);
				continue;
			}
		}
		memset(_screenPosTable[i], 0, 32 * 24);
	}
	int index = _res->_resLvlScreenBackgroundDataTable[num].currentMaskId;
	const uint8_t *p = _res->getLvlScreenPosDataPtr(num * 4 + index);
	if (p) {
		Video::decodeRLE(p, _screenPosTable[4], 32 * 24);
	} else {
		memset(_screenPosTable[4], 0, 32 * 24);
	}
}

void Game::setupScreenMask(uint8_t num) {
	if (num == kNoScreen) {
		return;
	}
	int mask = _res->_resLvlScreenBackgroundDataTable[num].currentMaskId;
	if (_res->_screensState[num].s3 != mask) {
		debug(kDebug_GAME, "setupScreenMask num %d mask %d", num, mask);
		_res->_screensState[num].s3 = mask;
		const uint8_t *maskData = _res->getLvlScreenMaskDataPtr(num * 4 + mask);
		if (maskData) {
			Video::decodeRLE(maskData, _screenTempMaskBuffer, 32 * 24);
		} else {
			memset(_screenTempMaskBuffer, 0, 32 * 24);
		}
		uint8_t *p = _screenMaskBuffer + screenMaskOffset(_res->_screensBasePos[num].u, _res->_screensBasePos[num].v);
		for (int i = 0; i < 24; ++i) {
			memcpy(p, _screenTempMaskBuffer + i * 32, 32);
			p += 512;
		}
		if (0) {
			fprintf(stdout, "screen %d mask %d\n", num, mask);
			for (int y = 0; y < 24; ++y) {
				for (int x = 0; x < 32; ++x) {
					fprintf(stdout, "%02d ", _screenTempMaskBuffer[y * 32 + x]);
				}
				fputc('\n', stdout);
			}
		}
	}
	if (_res->_currentScreenResourceNum == num) {
		setupScreenPosTable(num);
	}
}

void Game::resetScreenMask() {
	memset(_screenMaskBuffer, 0, sizeof(_screenMaskBuffer));
	for (int i = 0; i < _res->_lvlHdr.screensCount; ++i) {
		_res->_screensState[i].s3 = 0xFF;
		setupScreenMask(i);
	}
}

void Game::setScreenMaskRectHelper(int x1, int y1, int x2, int y2, int screenNum) {
	if (screenNum != kNoScreen) {

		int index = _res->_resLvlScreenBackgroundDataTable[screenNum].currentMaskId;
		const uint8_t *p = _res->getLvlScreenMaskDataPtr(screenNum * 4 + index);
		Video::decodeRLE(p, _screenTempMaskBuffer, 32 * 24);

		int h = (y2 - y1 + 7) >> 3;
		int w = (x2 - x1 + 7) >> 3;

		const int x = x1 - _res->_screensBasePos[screenNum].u;
		const int y = y1 - _res->_screensBasePos[screenNum].v;

		const int u = _res->_screensBasePos[screenNum].u + Video::W;
		if (x2 < u) {
			++w;
		}

		const uint8_t *src = _screenTempMaskBuffer + screenGridOffset(x, y);
		uint8_t *dst = _screenMaskBuffer + screenMaskOffset(x1, y1);
		for (int i = 0; i < h; ++i) {
			memcpy(dst, src, w);
			src += 32;
			dst += 512;
		}
	}
}

void Game::setScreenMaskRect(int x1, int y1, int x2, int y2, int screenNum) {
	const int u = _res->_screensBasePos[screenNum].u;
	const int v = _res->_screensBasePos[screenNum].v;
	const int topScreenNum = _res->_screensGrid[screenNum][kPosTopScreen];
	if (x1 < u || y1 < v || y2 >= v + Video::H) {
		if (topScreenNum != kNoScreen) {
			const int u2 = _res->_screensBasePos[topScreenNum].u;
			const int v2 = _res->_screensBasePos[topScreenNum].v;
			if (x1 >= u2 && y1 >= v2 && y2 < v + Video::H) {
				setScreenMaskRectHelper(x1, y1, x2, v2 + Video::H, topScreenNum);
			}
		}
	}
	setScreenMaskRectHelper(x1, v, x2, y2, screenNum);
}

void Game::updateScreenMaskBuffer(int x, int y, int type) {
	uint8_t *p = _screenMaskBuffer + screenMaskOffset(x, y);
	switch (type) {
	case 0:
		p[0] &= ~8;
		break;
	case 1:
		p[0] |= 8;
		break;
	case 2:
		p[0] |= 8;
		p[-1] = 0;
		p[-2] = 0;
		p[-3] = 0;
		break;
	case 3:
		p[0] |= 8;
		p[1] = 0;
		p[2] = 0;
		p[3] = 0;
		break;
	}
}

void Game::setupLvlObjectBitmap(LvlObject *ptr) {
	LvlObjectData *dat = ptr->levelData0x2988;
	if (!dat) {
		return;
	}
	LvlAnimHeader *ah = (LvlAnimHeader *)(dat->animsInfoData + kLvlAnimHdrOffset) + ptr->anim;
	LvlAnimSeqHeader *ash = (LvlAnimSeqHeader *)(dat->animsInfoData + ah->seqOffset) + ptr->frame;

	ptr->currentSound = ash->sound;
	ptr->flags0 = merge_bits(ptr->flags0, ash->flags0, 0x3FF);
	ptr->flags1 = merge_bits(ptr->flags1, ash->flags1, 6);
	ptr->flags1 = merge_bits(ptr->flags1, ash->flags1, 8);
	ptr->currentSprite = ash->firstFrame;

	ptr->bitmapBits = _res->getLvlSpriteFramePtr(dat, ash->firstFrame, &ptr->width, &ptr->height);

	const int w = ptr->width - 1;
	const int h = ptr->height - 1;

	if (ptr->type == 8 && (ptr->spriteNum == 2 || ptr->spriteNum == 0)) {
		AndyLvlObjectData *dataPtr = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
		dataPtr->boundingBox.x1 = ptr->xPos;
		dataPtr->boundingBox.y1 = ptr->yPos;
		dataPtr->boundingBox.x2 = ptr->xPos + w;
		dataPtr->boundingBox.y2 = ptr->yPos + h;
	}

	const LvlSprHotspotData *hotspot = ((LvlSprHotspotData *)dat->hotspotsData) + ash->firstFrame;
	const int type = (ptr->flags1 >> 4) & 3;
	for (int i = 0; i < 8; ++i) {
		switch (type) {
		case 0:
			ptr->posTable[i].x = hotspot->pts[i].x;
			ptr->posTable[i].y = hotspot->pts[i].y;
			break;
		case 1:
			ptr->posTable[i].x = w - hotspot->pts[i].x;
			ptr->posTable[i].y = hotspot->pts[i].y;
			break;
		case 2:
			ptr->posTable[i].x = hotspot->pts[i].x;
			ptr->posTable[i].y = h - hotspot->pts[i].y;
			break;
		case 3:
			ptr->posTable[i].x = w - hotspot->pts[i].x;
			ptr->posTable[i].y = h - hotspot->pts[i].y;
			break;
		}
	}
}

void Game::randomizeInterpolatePoints(int32_t *pts, int count) {
	int32_t rnd = _rnd.update();
	for (int i = 0; i < count; ++i) {
		const int index = _pointDstIndexTable[i];
		const int c1 = pts[_pointSrcIndex1Table[index]];
		const int c2 = pts[_pointSrcIndex2Table[index]];
		pts[index] = (c1 + c2 + (rnd >> _pointRandomizeShiftTable[i])) / 2;
		rnd *= 2;
	}
}

int Game::fixPlasmaCannonPointsScreenMask(int num) {
	uint8_t _dl = ((~_plasmaCannonDirection) & 1) | 6;
	int var1 = _plasmaCannonFirstIndex;
	int vf = _res->_screensBasePos[num].u;
	int ve = _res->_screensBasePos[num].v;
	uint8_t _al = 0;
	if (_andyObject->anim == 84) {
		int yPos, xPos;
		yPos = _plasmaCannonYPointsTable1[var1];
		if (yPos < 0) {
			yPos = 0;
		}
		yPos += ve;
		while ((xPos = _plasmaCannonXPointsTable1[var1]) >= 0) {
			xPos += vf;
			_al = _screenMaskBuffer[screenMaskOffset(xPos, yPos)];
			if ((_al & _dl) != 0) {
				_plasmaCannonLastIndex1 = var1;
				break;
			}
			var1 += 4;
			if (var1 >= _plasmaCannonLastIndex2) {
				break;
			}
		}
	} else {
		int xPos, yPos;
		while ((xPos = _plasmaCannonXPointsTable1[var1]) >= 0 && (yPos = _plasmaCannonYPointsTable1[var1]) >= 0) {
			yPos += ve;
			xPos += vf;
			_al = _screenMaskBuffer[screenMaskOffset(xPos, yPos)];
			if ((_al & _dl) != 0) {
				_plasmaCannonLastIndex1 = var1;
				break;
			}
			var1 += 4;
			if (var1 >= _plasmaCannonLastIndex2) {
				break;
			}
		}
	}
	return _al;
}

void Game::setupPlasmaCannonPointsHelper() {
	if (_plasmaCannonPointsSetupCounter == 0) {
		int xR = _rnd.update();
		for (int i = 0; i < 64; ++i) {
			const int index = _pointDstIndexTable[i];
			const int x1 = _plasmaCannonPosX[_pointSrcIndex1Table[index]];
			const int x2 = _plasmaCannonPosX[_pointSrcIndex2Table[index]];
			_plasmaCannonPosX[index] = (x1 + x2 + (xR >> _pointRandomizeShiftTable[i])) / 2;
			xR *= 2;
		}
		int yR = _rnd.update();
		for (int i = 0; i < 64; ++i) {
			const int index = _pointDstIndexTable[i];
			const int y1 = _plasmaCannonPosY[_pointSrcIndex1Table[index]];
			const int y2 = _plasmaCannonPosY[_pointSrcIndex2Table[index]];
			_plasmaCannonPosY[index] = (y1 + y2 + (yR >> _pointRandomizeShiftTable[i])) / 2;
			yR *= 2;
		}
		if (_andyObject->anim == 84) {
			for (int i = 0; i <= 496; i += 8) {
				const int index = i / 4;
				_plasmaCannonXPointsTable2[2 + index] = (_plasmaCannonPosX[4 + index] - _plasmaCannonXPointsTable1[4 + index]) / 2;
				_plasmaCannonYPointsTable2[2 + index] = (_plasmaCannonPosY[4 + index] - _plasmaCannonYPointsTable1[4 + index]) / 8;
			}
		} else {
			for (int i = 0; i <= 496; i += 8) {
				const int index = i / 4;
				_plasmaCannonXPointsTable2[2 + index] = (_plasmaCannonPosX[4 + index] - _plasmaCannonXPointsTable1[4 + index]) / 2;
				_plasmaCannonYPointsTable2[2 + index] = (_plasmaCannonPosY[4 + index] - _plasmaCannonYPointsTable1[4 + index]) / 2;
			}
		}
		for (int i = 0; i <= 504; i += 8) {
			const int index = i / 4;
			_plasmaCannonXPointsTable1[2 + index] = _plasmaCannonPosX[2 + index];
			_plasmaCannonYPointsTable1[2 + index] = _plasmaCannonPosY[2 + index];
		}
	} else {
		for (int i = 0; i <= 496; i += 8) {
			const int index = i / 4;
			_plasmaCannonXPointsTable1[4 + index] += _plasmaCannonXPointsTable2[2 + index];
			_plasmaCannonYPointsTable1[4 + index] += _plasmaCannonYPointsTable2[2 + index];
		}
	}
	++_plasmaCannonPointsSetupCounter;
	if (_plasmaCannonPointsSetupCounter >= 2) {
		_plasmaCannonPointsSetupCounter = 0;
	}
	_plasmaCannonLastIndex2 = 128;
	if (_plasmaCannonDirection == 0) {
		_plasmaCannonLastIndex1 = _plasmaCannonPointsMask = 0;
		_plasmaCannonFirstIndex = 128;
	} else {
		_plasmaCannonFirstIndex = 0;
		_plasmaCannonPointsMask = fixPlasmaCannonPointsScreenMask(_res->_currentScreenResourceNum);
	}
}

void Game::destroyLvlObjectPlasmaExplosion(LvlObject *o) {
	AndyLvlObjectData *l = (AndyLvlObjectData *)getLvlObjectDataPtr(o, kObjectDataTypeAndy);
	if (l->shootLvlObject) {
		l->shootLvlObject = 0;

		assert(_plasmaExplosionObject);

		LvlObject *ptr = _plasmaExplosionObject->nextPtr;
		removeLvlObjectFromList(&_plasmaExplosionObject, ptr);
		destroyLvlObject(ptr);

		ptr = _plasmaExplosionObject;
		removeLvlObjectFromList(&_plasmaExplosionObject, ptr);
		destroyLvlObject(ptr);
	}
}

void Game::shuffleArray(uint8_t *p, int count) {
	for (int i = 0; i < count * 2; ++i) {
		const int index1 = _rnd.update() % count;
		const int index2 = _rnd.update() % count;
		SWAP(p[index1], p[index2]);
	}
}

void Game::destroyLvlObject(LvlObject *o) {
	assert(o);
	if (o->type == 8) {
		_res->decLvlSpriteDataRefCounter(o);
		o->nextPtr = _declaredLvlObjectsNextPtr;
		--_declaredLvlObjectsListCount;
		_declaredLvlObjectsNextPtr = o;
		switch (o->spriteNum) {
		case 0:
		case 2:
			o->dataPtr = 0;
			break;
		case 3:
		case 7:
			if (o->dataPtr) {
				clearShootLvlObjectData(o);
			}
			break;
		}
	}
	if (o->sssObject) {
		removeSound(o);
	}
	o->sssObject = 0;
	o->bitmapBits = 0;
}

void Game::setupPlasmaCannonPoints(LvlObject *ptr) {
	_plasmaCannonDirection = 0;
	if ((ptr->flags0 & 0x1F) == 4) {
		if ((ptr->actionKeyMask & 4) == 0) { // not using plasma cannon
			destroyLvlObjectPlasmaExplosion(ptr);
		} else {
			_plasmaCannonPosX[0] = _plasmaCannonPosX[128] = ptr->xPos + ptr->posTable[6].x;
			_plasmaCannonPosY[0] = _plasmaCannonPosY[128] = ptr->yPos + ptr->posTable[6].y;
			const int num = ((ptr->flags0 >> 5) & 7) - 3;
			switch (num) {
			case 0:
				_plasmaCannonPosY[128] -= 176; // 192 - 16
				_plasmaCannonDirection = 3;
				break;
			case 1:
				_plasmaCannonPosY[128] += 176;
				_plasmaCannonDirection = 6;
				break;
			case 3:
				_plasmaCannonPosY[128] -= 176;
				_plasmaCannonDirection = 1;
				break;
			case 4:
				_plasmaCannonPosY[128] += 176;
				_plasmaCannonDirection = 4;
				break;
			default:
				_plasmaCannonDirection = 2;
				break;
			}
			if (ptr->flags1 & 0x10) {
				if (_plasmaCannonDirection != 1) {
					_plasmaCannonDirection = (_plasmaCannonDirection & ~2) | 8;
					_plasmaCannonPosX[128] -= 264; // 256 + 8
				}
			} else {
				if (_plasmaCannonDirection != 1) {
					_plasmaCannonPosX[128] += 264;
				}
			}
			if (_plasmaCannonPrevDirection != _plasmaCannonDirection) {
				_plasmaCannonXPointsTable1[0] = _plasmaCannonPosX[0];
				_plasmaCannonXPointsTable1[128] = _plasmaCannonPosX[128];
				randomizeInterpolatePoints(_plasmaCannonXPointsTable1, 64);
				_plasmaCannonYPointsTable1[0] = _plasmaCannonPosY[0];
				_plasmaCannonYPointsTable1[128] = _plasmaCannonPosY[128];
				randomizeInterpolatePoints(_plasmaCannonYPointsTable1, 64);
				_plasmaCannonPrevDirection = _plasmaCannonDirection;
			}
		}
	}
	if (_plasmaCannonPrevDirection != 0) {
		setupPlasmaCannonPointsHelper();
		if (_plasmaCannonFirstIndex >= _plasmaCannonLastIndex2) {
			_plasmaCannonPrevDirection = 0;
			_plasmaCannonLastIndex2 = 16;
		}
	}
}

int Game::testPlasmaCannonPointsDirection(int x1, int y1, int x2, int y2) {
	int index1 = _plasmaCannonFirstIndex;
	int vg = _plasmaCannonPosX[index1];
	int ve = _plasmaCannonPosY[index1];
	int index2 = _plasmaCannonLastIndex1;
	if (index2 == 0) {
		index2 = _plasmaCannonLastIndex2;
	}
	int va = _plasmaCannonXPointsTable1[index2];
	int vf = _plasmaCannonYPointsTable1[index2];
	if (vg > va) {
		if (ve > vf) {
			if (x1 > vg || x2 < va || y1 > ve || y2 < vf) {
				return 0;
			}
			index1 += 4;
			do {
				va = _plasmaCannonXPointsTable1[index1];
				vf = _plasmaCannonYPointsTable1[index1];
				if (x1 <= vg && x2 >= va && y1 <= ve && y2 >= vf) {
					goto endDir;
				}
				vg = va;
				ve = vf;
				index1 += 4;
			} while (index1 < index2);
			return 0;
		} else {
			if (x1 > vg || x2 < va || y1 > vf || y2 < ve) {
				return 0;
			}
			index1 += 4;
			do {
				va = _plasmaCannonXPointsTable1[index1];
				vf = _plasmaCannonYPointsTable1[index1];
				if (x1 <= vg && x2 >= va && y1 <= vf && y2 >= ve) {
					goto endDir;
				}
				vg = va;
				ve = vf;
				index1 += 4;
			} while (index1 < index2);
			return 0;
		}
	} else {
		if (ve > vf) {
			if (x1 > va || x2 < vg || y1 > ve || y2 < vf) {
				return 0;
			}
			index1 += 4;
			do {
				va = _plasmaCannonXPointsTable1[index1];
				vf = _plasmaCannonYPointsTable1[index1];
				if (x1 <= va && x2 >= vg && y1 <= ve && y2 >= vf) {
					goto endDir;
				}
				vg = va;
				ve = vf;
				index1 += 4;
			} while (index1 < index2);
			return 0;
		} else {
			if (x1 > va || x2 < vg || y1 > vf || y2 < ve) {
				return 0;
			}
			index1 += 4;
			do {
				va = _plasmaCannonXPointsTable1[index1];
				vf = _plasmaCannonYPointsTable1[index1];
				if (x1 <= va && x2 >= vg && y1 <= vf && y2 >= ve) {
					goto endDir;
				}
				vg = va;
				ve = vf;
				index1 += 4;
			} while (index1 < index2);
			return 0;
		}
	}
endDir:
	_plasmaCannonLastIndex1 = index1;
	_plasmaCannonPointsMask = 0;
	return 1;
}

void Game::preloadLevelScreenData(uint8_t num, uint8_t prev) {
	assert(num != kNoScreen);
	if (!_res->isLvlBackgroundDataLoaded(num)) {
		_res->loadLvlScreenBackgroundData(num);
	}
	if (num < _res->_sssPreloadInfosData.count) {
		const SssPreloadInfo *preloadInfo = &_res->_sssPreloadInfosData[num];
		for (unsigned int i = 0; i < preloadInfo->count; ++i) {
			const SssPreloadInfoData *preloadData = &preloadInfo->data[i];
			if (preloadData->screenNum == prev) {
				_res->preloadSssPcmList(preloadData);
				break;
			}
		}
	}
	if (0) {
		const uint8_t leftScreen = _res->_screensGrid[num][kPosLeftScreen];
		if (leftScreen != kNoScreen && !_res->isLvlBackgroundDataLoaded(leftScreen)) {
			_res->loadLvlScreenBackgroundData(leftScreen);
		}
		const uint8_t rightScreen = _res->_screensGrid[num][kPosRightScreen];
		if (rightScreen != kNoScreen && !_res->isLvlBackgroundDataLoaded(rightScreen)) {
			_res->loadLvlScreenBackgroundData(rightScreen);
		}
		for (unsigned int i = 0; i < kMaxScreens; ++i) {
			if (_res->_resLevelData0x2B88SizeTable[i] != 0) {
				if (i != num && i != leftScreen && i != rightScreen) {
					_res->unloadLvlScreenBackgroundData(i);
				}
			}
		}
	}
}

void Game::setLvlObjectPosRelativeToObject(LvlObject *ptr1, int num1, LvlObject *ptr2, int num2) {
	ptr1->xPos = ptr2->posTable[num2].x - ptr1->posTable[num1].x + ptr2->xPos;
	ptr1->yPos = ptr2->posTable[num2].y - ptr1->posTable[num1].y + ptr2->yPos;
}

void Game::setLvlObjectPosRelativeToPoint(LvlObject *ptr, int num, int x, int y) {
	assert(num >= 0 && num < 8);
	ptr->xPos = x - ptr->posTable[num].x;
	ptr->yPos = y - ptr->posTable[num].y;
}

void Game::clearLvlObjectsList0() {
	LvlObject *ptr = _lvlObjectsList0;
	while (ptr) {
		LvlObject *next = ptr->nextPtr;
		if (ptr->type == 8) {
			_res->decLvlSpriteDataRefCounter(ptr);
			ptr->nextPtr = _declaredLvlObjectsNextPtr;
			--_declaredLvlObjectsListCount;
			_declaredLvlObjectsNextPtr = ptr;
			switch (ptr->spriteNum) {
			case 0:
			case 2:
				ptr->dataPtr = 0;
				break;
			case 3:
			case 7:
				if (ptr->dataPtr) {
					clearShootLvlObjectData(ptr);
				}
				break;
			}
			if (ptr->sssObject) {
				removeSound(ptr);
			}
			ptr->sssObject = 0;
			ptr->bitmapBits = 0;
		}
		ptr = next;
	}
	_lvlObjectsList0 = 0;
}

void Game::clearLvlObjectsList1() {
	if (!_lvlObjectsList1) {
		return;
	}
	for (int i = 0; i < kMaxMonsterObjects1; ++i) {
		mstMonster1ResetData(&_monsterObjects1Table[i]);
	}
	for (int i = 0; i < kMaxMonsterObjects2; ++i) {
		mstMonster2ResetData(&_monsterObjects2Table[i]);
	}
	LvlObject *ptr = _lvlObjectsList1;
	while (ptr) {
		LvlObject *next = ptr->nextPtr;
		if (ptr->type == 8) {
			_res->decLvlSpriteDataRefCounter(ptr);
			ptr->nextPtr = _declaredLvlObjectsNextPtr;
			--_declaredLvlObjectsListCount;
			_declaredLvlObjectsNextPtr = ptr;
			switch (ptr->spriteNum) {
			case 0:
			case 2:
				ptr->dataPtr = 0;
				break;
			case 3:
			case 7:
				if (ptr->dataPtr) {
					clearShootLvlObjectData(ptr);
				}
				break;
			}
			if (ptr->sssObject) {
				removeSound(ptr);
			}
			ptr->sssObject = 0;
			ptr->bitmapBits = 0;
		}
		ptr = next;
	}
	_lvlObjectsList1 = 0;
}

void Game::clearLvlObjectsList2() {
	LvlObject *ptr = _lvlObjectsList2;
	while (ptr) {
		LvlObject *next = ptr->nextPtr;
		if (ptr->type == 8) {
			_res->decLvlSpriteDataRefCounter(ptr);
			ptr->nextPtr = _declaredLvlObjectsNextPtr;
			--_declaredLvlObjectsListCount;
			_declaredLvlObjectsNextPtr = ptr;
			switch (ptr->spriteNum) {
			case 0:
			case 2:
				ptr->dataPtr = 0;
				break;
			case 3:
			case 7:
				if (ptr->dataPtr) {
					clearShootLvlObjectData(ptr);
				}
				break;
			}
			if (ptr->sssObject) {
				removeSound(ptr);
			}
			ptr->sssObject = 0;
			ptr->bitmapBits = 0;
		}
		ptr = next;
	}
	_lvlObjectsList2 = 0;
}

void Game::clearLvlObjectsList3() {
	LvlObject *ptr = _lvlObjectsList3;
	while (ptr != 0) {
		LvlObject *next = ptr->nextPtr;
		if (ptr->type == 8) {
			_res->decLvlSpriteDataRefCounter(ptr);
			ptr->nextPtr = _declaredLvlObjectsNextPtr;
			--_declaredLvlObjectsListCount;
			_declaredLvlObjectsNextPtr = ptr;
			switch (ptr->spriteNum) {
			case 0:
			case 2:
				ptr->dataPtr = 0;
				break;
			case 3:
			case 7:
				if (ptr->dataPtr) {
					clearShootLvlObjectData(ptr);
				}
				break;
			}
			if (ptr->sssObject) {
				removeSound(ptr);
			}
			ptr->sssObject = 0;
			ptr->bitmapBits = 0;
		}
		ptr = next;
	}
	_lvlObjectsList3 = 0;
}

LvlObject *Game::addLvlObjectToList0(int num) {
	if (_res->_resLevelData0x2988PtrTable[num] != 0 && _declaredLvlObjectsListCount < kMaxLvlObjects) {
		assert(_declaredLvlObjectsNextPtr);
		LvlObject *ptr = _declaredLvlObjectsNextPtr;
		_declaredLvlObjectsNextPtr = _declaredLvlObjectsNextPtr->nextPtr;
		++_declaredLvlObjectsListCount;
		ptr->spriteNum = num;
		ptr->type = 8;
		_res->incLvlSpriteDataRefCounter(ptr);
		lvlObjectTypeCallback(ptr);
		ptr->currentSprite = 0;
		ptr->sssObject = 0;
		ptr->nextPtr = 0;
		ptr->bitmapBits = 0;
		ptr->nextPtr = _lvlObjectsList0;
		_lvlObjectsList0 = ptr;
		return ptr;
	}
	return 0;
}

LvlObject *Game::addLvlObjectToList1(int type, int num) {
	if ((type != 8 || _res->_resLevelData0x2988PtrTable[num] != 0) && _declaredLvlObjectsListCount < kMaxLvlObjects) {
		assert(_declaredLvlObjectsNextPtr);
		LvlObject *ptr = _declaredLvlObjectsNextPtr;
		_declaredLvlObjectsNextPtr = _declaredLvlObjectsNextPtr->nextPtr;
		++_declaredLvlObjectsListCount;
		ptr->spriteNum = num;
		ptr->type = type;
		if (type == 8) {
			_res->incLvlSpriteDataRefCounter(ptr);
			lvlObjectTypeCallback(ptr);
		}
		ptr->currentSprite = 0;
		ptr->sssObject = 0;
		ptr->nextPtr = 0;
		ptr->bitmapBits = 0;
		ptr->nextPtr = _lvlObjectsList1;
		_lvlObjectsList1 = ptr;
		return ptr;
	}
	return 0;
}

LvlObject *Game::addLvlObjectToList2(int num) {
	if (_res->_resLevelData0x2988PtrTable[num] != 0 && _declaredLvlObjectsListCount < kMaxLvlObjects) {
		assert(_declaredLvlObjectsNextPtr);
		LvlObject *ptr = _declaredLvlObjectsNextPtr;
		_declaredLvlObjectsNextPtr = _declaredLvlObjectsNextPtr->nextPtr;
		++_declaredLvlObjectsListCount;
		ptr->spriteNum = num;
		ptr->type = 8;
		_res->incLvlSpriteDataRefCounter(ptr);
		lvlObjectTypeCallback(ptr);
		ptr->currentSprite = 0;
		ptr->sssObject = 0;
		ptr->nextPtr = 0;
		ptr->bitmapBits = 0;
		ptr->nextPtr = _lvlObjectsList2;
		_lvlObjectsList2 = ptr;
		return ptr;
	}
	return 0;
}

LvlObject *Game::addLvlObjectToList3(int num) {
	if (_res->_resLevelData0x2988PtrTable[num] != 0 && _declaredLvlObjectsListCount < kMaxLvlObjects) {
		assert(_declaredLvlObjectsNextPtr);
		LvlObject *ptr = _declaredLvlObjectsNextPtr;
		_declaredLvlObjectsNextPtr = _declaredLvlObjectsNextPtr->nextPtr;
		++_declaredLvlObjectsListCount;
		ptr->spriteNum = num;
		ptr->type = 8;
		_res->incLvlSpriteDataRefCounter(ptr);
		lvlObjectTypeCallback(ptr);
		ptr->currentSprite = 0;
		ptr->sssObject = 0;
		ptr->nextPtr = 0;
		ptr->bitmapBits = 0;
		ptr->nextPtr = _lvlObjectsList3;
		_lvlObjectsList3 = ptr;
		ptr->callbackFuncPtr = &Game::lvlObjectList3Callback;
		return ptr;
	}
	return 0;
}

void Game::removeLvlObject(LvlObject *ptr) {
	AndyLvlObjectData *dataPtr = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
	LvlObject *o = dataPtr->shootLvlObject;
	if (o) {
		dataPtr->shootLvlObject = 0;
		removeLvlObjectFromList(&_lvlObjectsList0, o);
		destroyLvlObject(o);
	}
}

void Game::removeLvlObject2(LvlObject *o) {
	if (o->type != 2) {
		LvlObject *ptr = _lvlObjectsList1;
		if (ptr) {
			if (ptr == o) {
				_lvlObjectsList1 = o->nextPtr;
			} else {
				LvlObject *prev = 0;
				do {
					prev = ptr;
					ptr = ptr->nextPtr;
				} while (ptr && ptr != o);
				assert(ptr);
				prev->nextPtr = ptr->nextPtr;
			}
		}
	}
	o->dataPtr = 0;
	if (o->type == 8) {
		_res->decLvlSpriteDataRefCounter(o);
		o->nextPtr = _declaredLvlObjectsNextPtr;
		_declaredLvlObjectsNextPtr = o;
		--_declaredLvlObjectsListCount;
	} else {
		switch (o->spriteNum) {
		case 0:
		case 2:
			o->dataPtr = 0;
			break;
		case 3:
		case 7:
			if (o->dataPtr) {
				clearShootLvlObjectData(o);
			}
			break;
		}
	}
	if (o->sssObject) {
		removeSound(o);
		o->sssObject = 0;
	}
	o->bitmapBits = 0;
}

void Game::setAndySprite(int num) {
	switch (num) {
	case 0: //  Andy with plasma cannon and helmet
		removeLvlObject(_andyObject);
		setLvlObjectSprite(_andyObject, 8, 0);
		_andyObject->anim = 48;
		break;
	case 2: // Andy
		destroyLvlObjectPlasmaExplosion(_andyObject);
		_plasmaCannonDirection = 0;
		_plasmaCannonLastIndex1 = 0;
		_plasmaCannonExplodeFlag = false;
		_plasmaCannonPointsMask = 0;
		_plasmaCannonObject = 0;
		setLvlObjectSprite(_andyObject, 8, 2);
		_andyObject->anim = 232;
		break;
	}
	_andyObject->frame = 0;
}

void Game::setupAndyLvlObject() {
	LvlObject *ptr = _andyObject;
	_fallingAndyFlag = false;
	_andyActionKeysFlags = 0;
	_hideAndyObjectFlag = false;
	const CheckpointData *dat = _level->getCheckpointData(_level->_checkpoint);
	_plasmaCannonFlags = 0;
	_actionDirectionKeyMaskIndex = 0;
	_mstAndyCurrentScreenNum = ptr->screenNum;
	if (dat->spriteNum != ptr->spriteNum) {
		setAndySprite(dat->spriteNum);
	}
	ptr->childPtr = 0;
	ptr->xPos = dat->xPos;
	ptr->yPos = dat->yPos;
	ptr->flags2 = dat->flags2;
	ptr->anim = dat->anim;
	ptr->flags1 = ((ptr->flags2 >> 10) & 0x30) | (ptr->flags1 & ~0x30);
	ptr->screenNum = dat->screenNum;
	ptr->directionKeyMask = 0;
	ptr->actionKeyMask = 0;
	_currentScreen = dat->screenNum;
	_currentLeftScreen = _res->_screensGrid[_currentScreen][kPosLeftScreen];
	_currentRightScreen = _res->_screensGrid[_currentScreen][kPosRightScreen];
	ptr->frame = 0;
	setupLvlObjectBitmap(ptr);
	AndyLvlObjectData *dataPtr = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
	dataPtr->unk6 = 0;
	if (ptr->spriteNum == 2) {
		removeLvlObject(ptr);
	} else {
		destroyLvlObjectPlasmaExplosion(ptr);
	}
}

void Game::updateScreenHelper(int num) {
	_res->_screensState[num].s2 = 1;
	for (LvlObject *ptr = _screenLvlObjectsList[num]; ptr; ptr = ptr->nextPtr) {
		switch (ptr->type) {
		case 0: {
				AnimBackgroundData *p = (AnimBackgroundData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAnimBackgroundData);
				uint8_t *data = _res->_resLvlScreenBackgroundDataTable[num].backgroundAnimationTable[ptr->dataNum];
				if (!data) {
					warning("No backgroundAnimationData num %d screen %d", ptr->dataNum, num);
					break;
				}
				if (_res->_isPsx) {
					p->framesCount = READ_LE_UINT32(data); data += 4;
					ptr->currentSound = READ_LE_UINT32(data); data += 4;
					p->nextSpriteData = READ_LE_UINT16(data + 6) + data + 6;
				} else {
					p->framesCount = READ_LE_UINT16(data); data += 2;
					ptr->currentSound = READ_LE_UINT16(data); data += 2;
					p->nextSpriteData = READ_LE_UINT16(data + 4) + data + 4;
				}
				p->currentSpriteData = p->otherSpriteData = data;
				p->currentFrame = 0;
			}
			break;
		case 1: {
				uint8_t *data =  _res->_resLvlScreenBackgroundDataTable[num].backgroundSoundTable[ptr->dataNum];
				if (!data) {
					warning("No backgroundSoundData num %d screen %d", ptr->dataNum, num);
					break;
				}
				ptr->currentSound = READ_LE_UINT16(data); data += 2;
				ptr->dataPtr = data;
			}
			break;
		case 2:
			ptr->levelData0x2988 = _res->_resLvlScreenBackgroundDataTable[num].backgroundLvlObjectDataTable[ptr->dataNum];
			if (!ptr->levelData0x2988) {
				warning("No backgroundLvlObjectData num %d screen %d", ptr->dataNum, num);
				break;
			}
			if (_currentLevel == kLvl_rock) {
				switch (ptr->objectUpdateType) {
				case 0:
				case 5:
					ptr->callbackFuncPtr = &Game::objectUpdate_rock_case0;
					break;
				case 1: // shadow screen2
					ptr->callbackFuncPtr = &Game::objectUpdate_rock_case1;
					break;
				case 2: // shadow screen3
					ptr->callbackFuncPtr = &Game::objectUpdate_rock_case2;
					break;
				case 3:
					ptr->callbackFuncPtr = &Game::objectUpdate_rock_case3;
					break;
				case 4:
					ptr->callbackFuncPtr = &Game::objectUpdate_rock_case4;
					break;
				default:
					warning("updateScreenHelper unimplemented for level %d, state %d", _currentLevel, ptr->objectUpdateType);
					break;
				}
			} else {
				// other levels use two callbacks
				switch (ptr->objectUpdateType) {
				case 0:
					ptr->callbackFuncPtr = &Game::objectUpdate_rock_case0;
					break;
				case 1:
					ptr->callbackFuncPtr = &Game::objectUpdate_rock_case3;
					break;
				default:
					warning("updateScreenHelper unimplemented for level %d, state %d", _currentLevel, ptr->objectUpdateType);
					break;
				}
			}
			setupLvlObjectBitmap(ptr);
			break;
		}
	}
}

void Game::resetDisplay() {
//	_video_blitSrcPtr = _video_blitSrcPtr2;
	_video->_displayShadowLayer = false;
	_shakeScreenDuration = 0;
	_levelRestartCounter = 0;
	_fadePalette = false;
	memset(_video->_fadePaletteBuffer, 0, sizeof(_video->_fadePaletteBuffer));
	_snd_masterVolume = kDefaultSoundVolume; // _plyConfigTable[_plyConfigNumber].soundVolume;
}

void Game::updateScreen(uint8_t num) {
	uint8_t i, prev;

	if (num == kNoScreen) {
		return;
	}
	prev = _res->_currentScreenResourceNum;
	_res->_currentScreenResourceNum = num;
	updateScreenHelper(num);
	callLevel_preScreenUpdate(num);
	if (_res->_screensState[num].s0 >= _res->_screensState[num].s1) {
		--_res->_screensState[num].s1;
	}
	callLevel_postScreenUpdate(num);
	i = _res->_screensGrid[num][kPosTopScreen];
	if (i != kNoScreen && prev != i) {
		callLevel_preScreenUpdate(i);
		setupScreenMask(i);
		callLevel_postScreenUpdate(i);
	}
	i = _res->_screensGrid[num][kPosRightScreen];
	if (i != kNoScreen && _res->_resLevelData0x2B88SizeTable[i] != 0 && prev != i) {
		updateScreenHelper(i);
		callLevel_preScreenUpdate(i);
		setupScreenMask(i);
		callLevel_postScreenUpdate(i);
	}
	i = _res->_screensGrid[num][kPosBottomScreen];
	if (i != kNoScreen && prev != i) {
		callLevel_preScreenUpdate(i);
		setupScreenMask(i);
		callLevel_postScreenUpdate(i);
	}
	i = _res->_screensGrid[num][kPosLeftScreen];
	if (i != kNoScreen && _res->_resLevelData0x2B88SizeTable[i] != 0 && prev != i) {
		updateScreenHelper(i);
		callLevel_preScreenUpdate(i);
		setupScreenMask(i);
		callLevel_postScreenUpdate(i);
	}
	callLevel_postScreenUpdate(num);
	setupBackgroundBitmap();
	setupScreenMask(num);
	resetDisplay();
//	_time_counter1 = GetTickCount();
}

void Game::resetScreen() {
	for (int i = 0; i < _res->_lvlHdr.screensCount; ++i) {
		_res->_screensState[i].s0 = 0;
		_level->_screenCounterTable[i] = 0;
	}
	const uint8_t *dat2 = _level->getScreenRestartData();
	const int screenNum = _level->getCheckpointData(_level->_checkpoint)->screenNum;
	for (int i = 0; i < screenNum; ++i) {
		_res->_screensState[i].s0 = *dat2++;
		_level->_screenCounterTable[i] = *dat2++;
	}
	resetScreenMask();
	for (int i = screenNum; i < _res->_lvlHdr.screensCount; ++i) {
		_level->setupScreenCheckpoint(i);
	}
	resetWormHoleSprites();
}

void Game::restartLevel() {
	setupAndyLvlObject();
	clearLvlObjectsList2();
	clearLvlObjectsList3();
	if (!_mstDisabled) {
		resetMstCode();
		startMstCode();
	} else {
		_mstFlags = 0;
	}
	if (_res->_sssHdr.infosDataCount != 0) {
		resetSound();
	}
	const int screenNum = _level->getCheckpointData(_level->_checkpoint)->screenNum;
	preloadLevelScreenData(screenNum, kNoScreen);
	_andyObject->levelData0x2988 = _res->_resLevelData0x2988PtrTable[_andyObject->spriteNum];
	memset(_video->_backgroundLayer, 0, Video::W * Video::H);
	resetScreen();
	if (_andyObject->screenNum != screenNum) {
		preloadLevelScreenData(_andyObject->screenNum, kNoScreen);
	}
	updateScreen(_andyObject->screenNum);
}

void Game::playAndyFallingCutscene(int type) {
	bool play = false;
	if (type == 0) {
		play = true;
	} else if (_fallingAndyFlag) {
		++_fallingAndyCounter;
		if (_fallingAndyCounter >= 2) {
			play = true;
		}
	}
	if (!_paf->_skipCutscenes && play) {
		switch (_currentLevel) {
		case kLvl_rock:
			if (_andyObject->spriteNum == 0) {
				_paf->play(kPafAnimation_CanyonAndyFallingCannon); // Andy falls with cannon and helmet
			} else {
				_paf->play(kPafAnimation_CanyonAndyFalling); // Andy falls without cannon
			}
			break;
		case kLvl_fort:
			if (_res->_currentScreenResourceNum == 0) {
				_paf->play(kPafAnimation_CanyonAndyFalling);
			}
			break;
		case kLvl_isld:
			_paf->play(kPafAnimation_IslandAndyFalling);
			break;
		}
	}
	if (type != 0 && play) {
		restartLevel();
	}
}

int8_t Game::updateLvlObjectScreen(LvlObject *ptr) {
	int8_t ret = 0;

	if ((_plasmaCannonFlags & 1) == 0 && _plasmaCannonDirection == 0) {
		int xPosPrev = ptr->xPos;
		int xPos = ptr->xPos + ptr->posTable[3].x;
		int yPosPrev = ptr->yPos;
		int yPos = ptr->yPos + ptr->posTable[3].y;
		uint8_t num = ptr->screenNum;
		if (xPos < 0) {
			ptr->screenNum = _res->_screensGrid[num][kPosLeftScreen];
			ptr->xPos = xPosPrev + Video::W;
		} else if (xPos > Video::W) {
			ptr->screenNum = _res->_screensGrid[num][kPosRightScreen];
			ptr->xPos = xPosPrev - Video::W;
		}
		if (ptr->screenNum != kNoScreen) {
			if (yPos < 0) {
				ptr->screenNum = _res->_screensGrid[ptr->screenNum][kPosTopScreen];
				ptr->yPos = yPosPrev + Video::H;
			} else if (yPos > Video::H) {
				ptr->screenNum = _res->_screensGrid[ptr->screenNum][kPosBottomScreen];
				ptr->yPos = yPosPrev - Video::H;
			}
		}
		if (ptr->screenNum == kNoScreen) {
			debug(kDebug_GAME, "Changing screen from -1 to %d, pos=%d,%d (%d,%d)", num, xPos, yPos, xPosPrev, yPosPrev);
			ptr->screenNum = num;
			ptr->xPos = xPosPrev;
			ptr->yPos = yPosPrev;
			ret = -1;
		} else if (ptr->screenNum != num) {
			debug(kDebug_GAME, "Changing screen from %d to %d, pos=%d,%d", num, ptr->screenNum, xPos, yPos);
			ret = 1;
			AndyLvlObjectData *data = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
			data->boundingBox.x1 = ptr->xPos;
			data->boundingBox.x2 = ptr->xPos + ptr->width - 1;
			data->boundingBox.y1 = ptr->yPos;
			data->boundingBox.y2 = ptr->yPos + ptr->height - 1;
		}
	}
	_currentScreen = ptr->screenNum;
	_currentLeftScreen = _res->_screensGrid[_currentScreen][kPosLeftScreen];
	_currentRightScreen = _res->_screensGrid[_currentScreen][kPosRightScreen];
	return ret;
}

void Game::setAndyAnimationForArea(BoundingBox *box, int dx) {
	static uint8_t _prevAndyFlags0 = 0;
	BoundingBox objBox;
	objBox.x1 = _andyObject->xPos;
	objBox.x2 = _andyObject->xPos + _andyObject->posTable[3].x;
	objBox.y1 = _andyObject->yPos;
	objBox.y2 = _andyObject->yPos + _andyObject->posTable[3].y;
	const uint8_t _bl = _andyObject->flags0 & 0x1F;
	if (clipBoundingBox(box, &objBox)) {
		if ((_andyObject->actionKeyMask & 1) == 0) {
			_andyObject->actionKeyMask |= 8;
		}
		if (objBox.x2 >= box->x1 + dx && objBox.x2 <= box->x2 - dx) {
			uint8_t _al = 0;
			if (_currentLevel != kLvl_rock) {
				if (_bl == 1) {
					if (((_andyObject->flags0 >> 5) & 7) == 3) {
						_al = 0x80;
					}
				}
			} else {
				_andyObject->actionKeyMask &= ~1;
				_andyObject->directionKeyMask &= ~4;
				_andyObject->actionKeyMask |= 8;
			}
			if (_bl != 2) {
				if (_prevAndyFlags0 == 2) {
					_al = 0x80;
				}
				if (_al != 0 && _al > _actionDirectionKeyMaskIndex) {
					_actionDirectionKeyMaskIndex = _al;
					_actionDirectionKeyMaskCounter = 0;
				}
			}
		}
	}
	_prevAndyFlags0 = _bl;
}

void Game::setAndyLvlObjectPlasmaCannonKeyMask() {
	if (_actionDirectionKeyMaskCounter == 0) {
		switch (_actionDirectionKeyMaskIndex >> 4) {
		case 0:
			_actionDirectionKeyMaskCounter = 6;
			break;
		case 1:
		case 8:
		case 10:
			_actionDirectionKeyMaskCounter = 2;
			break;
		case 2:
		case 3:
		case 4:
		case 5:
			_actionDirectionKeyMaskCounter = 1;
			break;
		case 6:
		case 7:
		case 9:
			break;
		}
	}
	if (_actionDirectionKeyMaskIndex != 0) {
		if (_actionDirectionKeyMaskIndex == 0xA4 && !_fadePalette) { // game over
			_levelRestartCounter = 10;
			_plasmaCannonFlags |= 1;
		} else {
			if (_andyObject->spriteNum == 2 && _actionDirectionKeyMaskIndex >= 16) {
				removeLvlObject(_andyObject);
			}
			_andyActionKeysFlags = 0;
		}
		_andyObject->actionKeyMask = _actionDirectionKeyMaskTable[_actionDirectionKeyMaskIndex * 2];
		_andyObject->directionKeyMask = _actionDirectionKeyMaskTable[_actionDirectionKeyMaskIndex * 2 + 1];
	}
	--_actionDirectionKeyMaskCounter;
	if (_actionDirectionKeyMaskCounter == 0) {
		_actionDirectionKeyMaskIndex = 0;
	}
}

int Game::setAndySpecialAnimation(uint8_t mask) {
	if (mask > _actionDirectionKeyMaskIndex) {
		_actionDirectionKeyMaskIndex = mask;
		_actionDirectionKeyMaskCounter = 0;
		return 1;
	}
	return 0;
}

int Game::clipBoundingBox(BoundingBox *coords, BoundingBox *box) {
	if (coords->x1 > coords->x2) {
		SWAP(coords->x1, coords->x2);
	}
	if (coords->y1 > coords->y2) {
		SWAP(coords->y1, coords->y2);
	}
	if (box->x1 > box->x2) {
		SWAP(box->x1, box->x2);
	}
	if (box->y1 > box->y2) {
		SWAP(box->y1, box->y2);
	}
	if (coords->x1 > box->x2 || coords->x2 < box->x1 || coords->y1 > box->y2 || coords->y2 < box->y1) {
		return 0;
	}
	_clipBoxOffsetX = (box->x2 - coords->x1) / 2 + coords->x1;
	_clipBoxOffsetY = (box->y2 - coords->y1) / 2 + coords->y1;
	return 1;
}

int Game::updateBoundingBoxClippingOffset(BoundingBox *vc, BoundingBox *ve, const uint8_t *coords, int direction) {
	int ret = 0;
	int count = *coords++;
	if (count == 0) {
		return clipBoundingBox(vc, ve);
	}
	switch (direction) {
	case 1:
		for (; count-- != 0; coords += 4) {
			if (vc->x1 > ve->x2 - coords[0] || vc->x2 < ve->x2 - coords[2]) {
				continue;
			}
			if (vc->y1 > ve->y1 + coords[3] || vc->y2 < ve->y1 + coords[1]) {
				continue;
			}
			break;
		}
		break;
	case 2:
		for (; count-- != 0; coords += 4) {
			if (vc->x1 > coords[2] + ve->x1 || vc->x2 < coords[0] + ve->x1) {
				continue;
			}
			if (vc->y1 > ve->y2 - coords[1] || vc->y2 < ve->y2 - coords[3]) {
				continue;
			}
			break;
		}
		break;
	case 3:
		for (; count-- != 0; coords += 4) {
			if (vc->x1 > ve->x2 - coords[0] || vc->x2 < ve->x2 - coords[2]) {
				continue;
			}
			if (vc->y1 > ve->y2 - coords[1] || vc->y2 < ve->y2 - coords[3]) {
				continue;
			}
			break;
		}
		break;
	default:
		for (; count-- != 0; coords += 4) {
			if (vc->x1 > coords[2] + ve->x1 || vc->x2 < coords[0] + ve->x1) {
				continue;
			}
			if (vc->y1 > coords[3] + ve->y1 || vc->y2 < coords[1] + ve->y1) {
				continue;
			}
			break;
		}
		break;
	}
	if (count != 0) {
		_clipBoxOffsetX = (vc->x2 - vc->x1) / 2 + vc->x1;
		_clipBoxOffsetY = (vc->y2 - vc->y1) / 2 + vc->y1;
		ret = 1;
	}
	return ret;
}

int Game::clipLvlObjectsBoundingBoxHelper(LvlObject *o1, BoundingBox *box1, LvlObject *o2, BoundingBox *box2) {
	int ret = 0;
	const uint8_t *coords1 = _res->getLvlSpriteCoordPtr(o1->levelData0x2988, o1->currentSprite);
	const uint8_t *coords2 = _res->getLvlSpriteCoordPtr(o2->levelData0x2988, o2->currentSprite);
	if (clipBoundingBox(box1, box2)) {
		int count = *coords1++;
		if (count != 0) {
			const int direction = (o1->flags1 >> 4) & 3;
			switch (direction) {
			case 1:
				for (; count-- != 0 && !ret; coords1 += 4) {
					BoundingBox tmp;
					tmp.x1 = box1->x2 - coords1[2];
					tmp.x2 = box1->x2 - coords1[0];
					tmp.y1 = box1->y1 + coords1[1];
					tmp.y2 = box1->y2 + coords1[3];
					ret = updateBoundingBoxClippingOffset(&tmp, box2, coords2, (o2->flags1 >> 4) & 3);
				}
				break;
			case 2:
				for (; count-- != 0 && !ret; coords1 += 4) {
					BoundingBox tmp;
					tmp.x1 = box1->x1 + coords1[0];
					tmp.x2 = box1->x1 + coords1[2];
					tmp.y1 = box1->y2 - coords1[3];
					tmp.y2 = box1->y2 - coords1[1];
					ret = updateBoundingBoxClippingOffset(&tmp, box2, coords2, (o2->flags1 >> 4) & 3);
				}
				break;
			case 3:
				for (; count-- != 0 && !ret; coords1 += 4) {
					BoundingBox tmp;
					tmp.x1 = box1->x2 - coords1[2];
					tmp.x2 = box1->x2 - coords1[0];
					tmp.y1 = box1->y2 - coords1[3];
					tmp.y2 = box1->y2 - coords1[1];
					ret = updateBoundingBoxClippingOffset(&tmp, box2, coords2, (o2->flags1 >> 4) & 3);
				}
				break;
			default:
				for (; count-- != 0 && !ret; coords1 += 4) {
					BoundingBox tmp;
					tmp.x1 = box1->x1 + coords1[0];
					tmp.x2 = box1->x1 + coords1[2];
					tmp.y1 = box1->y1 + coords1[1];
					tmp.y2 = box1->y1 + coords1[3];
					ret = updateBoundingBoxClippingOffset(&tmp, box2, coords2, (o2->flags1 >> 4) & 3);
				}
				break;
			}
		}
	}
	return ret;
}

int Game::clipLvlObjectsBoundingBox(LvlObject *o, LvlObject *ptr, int type) {
	BoundingBox obj1, obj2;

	obj1.x1 = o->xPos + _res->_screensBasePos[o->screenNum].u;
	obj1.y1 = obj1.y2 = o->yPos + _res->_screensBasePos[o->screenNum].v;

	obj2.x1 = ptr->xPos + _res->_screensBasePos[ptr->screenNum].u;
	obj2.y1 = obj2.y2 = ptr->yPos + _res->_screensBasePos[ptr->screenNum].v;

	switch (type - 17) {
	case 1:
		obj1.x2 = obj1.x1 + o->posTable[1].x;
		obj1.x1 += o->posTable[0].x;
		obj1.y2 += o->posTable[1].y;
		obj1.y1 += o->posTable[0].y;
		obj2.x2 = obj2.x1 + ptr->width - 1;
		obj2.y2 += ptr->height - 1;
		return clipBoundingBox(&obj1, &obj2);
	case 17:
		obj1.x2 = obj1.x1 + o->posTable[1].x;
		obj1.x1 += o->posTable[0].x;
		obj1.y2 += o->posTable[1].y;
		obj1.y1 += o->posTable[0].y;
		obj2.x2 = obj2.x1 + ptr->posTable[1].x;
		obj2.x1 += ptr->posTable[0].x;
		obj2.y2 += ptr->posTable[1].y;
		obj2.y1 += ptr->posTable[0].y;
		return clipBoundingBox(&obj1, &obj2);
	case 49:
		obj1.x2 = obj1.x1 + o->posTable[1].x;
		obj1.x1 += o->posTable[0].x;
		obj1.y2 += o->posTable[1].y;
		obj1.y1 += o->posTable[0].y;
		obj2.x2 = obj2.x1 + ptr->width - 1;
		obj2.y2 += ptr->height - 1;
		if (clipBoundingBox(&obj1, &obj2)) {
			updateBoundingBoxClippingOffset(&obj1, &obj2, _res->getLvlSpriteCoordPtr(ptr->levelData0x2988, ptr->currentSprite), (ptr->flags1 >> 4) & 3);
		}
		break;
	case 16:
		obj1.x2 = obj1.x1 + o->width - 1;
		obj1.y2 += o->height - 1;
		obj2.y1 += ptr->posTable[0].y;
		obj2.x2 = obj2.x1 + ptr->posTable[1].x;
		obj2.x1 += ptr->posTable[0].x;
		obj2.y2 += ptr->posTable[1].y;
		return clipBoundingBox(&obj1, &obj2);
	case 0:
		obj1.x2 = obj1.x1 + o->width - 1;
		obj1.y2 += o->height - 1;
		obj2.x2 = obj2.x1 + ptr->width - 1;
		obj2.y2 += ptr->height - 1;
		return clipBoundingBox(&obj1, &obj2);
	case 48:
		obj1.x2 = obj1.x1 + o->width - 1;
		obj1.y2 += o->height - 1;
		obj2.x2 = obj2.x1 + ptr->width - 1;
		obj2.y2 += ptr->height - 1;
		if (clipBoundingBox(&obj1, &obj2)) {
			return updateBoundingBoxClippingOffset(&obj1, &obj2, _res->getLvlSpriteCoordPtr(ptr->levelData0x2988, ptr->currentSprite), (ptr->flags1 >> 4) & 3);
		}
		break;
	case 19:
		obj1.x2 = obj1.x1 + o->width - 1;
		obj1.y2 += o->height - 1;
		obj2.x2 = obj2.x1 + ptr->posTable[1].x;
		obj2.x1 += ptr->posTable[0].x;
		obj2.y1 += ptr->posTable[0].y;
		obj2.y2 += ptr->posTable[1].y;
		if (clipBoundingBox(&obj2, &obj1)) {
			return updateBoundingBoxClippingOffset(&obj2, &obj1, _res->getLvlSpriteCoordPtr(o->levelData0x2988, o->currentSprite), (o->flags1 >> 4) & 3);
		}
		break;
	case 3:
		obj1.x2 = obj1.x1 + o->width - 1;
		obj1.y2 += o->height - 1;
		obj2.x2 = obj2.x1 + ptr->width - 1;
		obj2.y2 += ptr->height - 1;
		if (clipBoundingBox(&obj2, &obj1)) {
			return updateBoundingBoxClippingOffset(&obj2, &obj1, _res->getLvlSpriteCoordPtr(o->levelData0x2988, o->currentSprite), (o->flags1 >> 4) & 3);
		}
		break;
	case 51:
		obj1.x2 = obj1.x1 + o->width - 1;
		obj1.y2 += o->height - 1;
		obj2.x2 = obj2.x1 + ptr->width - 1;
		obj2.y2 += ptr->height - 1;
		return clipLvlObjectsBoundingBoxHelper(o, &obj1, ptr, &obj2);
	case 115:
		if (o->width == 3) {
			obj1.y2 += 7;
			obj1.x2 = obj1.x1 + 7;
		} else {
			obj1.x2 = obj1.x1 + o->width - 1;
			obj1.y2 += o->height - 1;
		}
		obj2.x2 = obj2.x1 + ptr->width - 9;
		obj2.x1 += 4;
		obj2.y2 += ptr->height - 13;
		obj2.y1 += 6;
		if (clipBoundingBox(&obj2, &obj1)) {
			return updateBoundingBoxClippingOffset(&obj2, &obj1, _res->getLvlSpriteCoordPtr(o->levelData0x2988, o->currentSprite), (o->flags1 >> 4) & 3);
		}
		break;
	default:
		warning("Unhandled clipLvlObjectsBoundingBox type %d (%d)", type, type - 17);
		break;
	}
	return 0;
}

int Game::clipLvlObjectsSmall(LvlObject *o1, LvlObject *o2, int type) {
	if (o1->width > 3 || o1->height > 3) {
		return clipLvlObjectsBoundingBox(o1, o2, type);
	}
	LvlObject tmpObject;
	memcpy(&tmpObject, o1, sizeof(LvlObject));
	tmpObject.type = 2;
	if (o1->frame == 0) {
		updateAndyObject(&tmpObject);
	}
	updateAndyObject(&tmpObject);
	return clipLvlObjectsBoundingBox(&tmpObject, o2, type);
}

int Game::restoreAndyCollidesLava() {
	int ret = 0;
	if (_lvlObjectsList1 && !_hideAndyObjectFlag && (_mstFlags & 0x80000000) == 0) {
		AndyLvlObjectData *data = (AndyLvlObjectData *)getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		for (LvlObject *o = _lvlObjectsList1; o; o = o->nextPtr) {
			if (o->spriteNum != 21 || o->screenNum != _res->_currentScreenResourceNum) {
				continue;
			}
			BoundingBox b;
			b.x1 = o->xPos + o->posTable[0].x;
			b.x2 = o->xPos + o->posTable[1].x;
			b.y1 = o->yPos + o->posTable[0].y;
			b.y2 = o->yPos + o->posTable[1].y;
			if (!clipBoundingBox(&data->boundingBox, &b)) {
				ret = clipLvlObjectsBoundingBox(_andyObject, o, 68);
				if (ret) {
					setAndySpecialAnimation(0xA3);
				}
			}
		}
	}
	return ret;
}

int Game::updateAndyLvlObject() {
	if (!_andyObject) {
		return 0;
	}
	if (_actionDirectionKeyMaskIndex != 0) {
		setAndyLvlObjectPlasmaCannonKeyMask();
	}
	assert(_andyObject->callbackFuncPtr);
	(this->*_andyObject->callbackFuncPtr)(_andyObject);
	if (_currentLevel != kLvl_isld) {
		AndyLvlObjectData *data = (AndyLvlObjectData *)getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		_andyObject->xPos += data->dxPos;
		_andyObject->yPos += data->dyPos;
	}
	const uint8_t flags = _andyObject->flags0 & 255;
	if ((flags & 0x1F) == 0xB) {
		if (_andyObject->spriteNum == 2) {
			removeLvlObject(_andyObject);
		}
		if ((flags & 0xE0) == 0x40) {
			setAndySpecialAnimation(0xA4);
		}
	}
	const int ret = updateLvlObjectScreen(_andyObject);
	if (ret > 0) {
		// changed screen
		return 1;
	} else if (ret == 0) {
		if (_currentLevel != kLvl_rock && _currentLevel != kLvl_lar2 && _currentLevel != kLvl_test) {
			return 0;
		}
		if (_plasmaExplosionObject) {
			_plasmaExplosionObject->screenNum = _andyObject->screenNum;
			lvlObjectType1Callback(_plasmaExplosionObject);
			if (_andyObject->actionKeyMask & 4) {
				addToSpriteList(_plasmaExplosionObject);
			}
		} else if (_andyObject->spriteNum == 0) {
			lvlObjectType1Init(_andyObject);
		}
		return 0;
	}
	// moved to invalid screen (-1), restart
	if ((_andyObject->flags0 & 0x1F) != 0xB) {
		playAndyFallingCutscene(0);
	}
	restartLevel();
	return 1;
}

void Game::drawPlasmaCannon() {
	int index = _plasmaCannonFirstIndex;
	int lastIndex = _plasmaCannonLastIndex1;
	if (lastIndex == 0) {
		lastIndex = _plasmaCannonLastIndex2;
	}
	int x1 = _plasmaCannonPosX[index];
	int y1 = _plasmaCannonPosY[index];
	index += 4;
	do {
		_video->_drawLine.color = 0xA9;
		int x2 = _plasmaCannonXPointsTable1[index];
		int y2 = _plasmaCannonYPointsTable1[index];
		if (_plasmaCannonDirection == 1) {
			_video->drawLine(x1 - 1, y1, x2 - 1, y2);
			_video->drawLine(x1 + 1, y1, x2 + 1, y2);
		} else {
			_video->drawLine(x1, y1 - 1, x2, y2 - 1);
			_video->drawLine(x1, y1 + 1, x2, y2 + 1);
		}
		_video->_drawLine.color = 0xA6;
		_video->drawLine(x1, y1, x2, y2);
		x1 = x2;
		y1 = y2;
		index += 4;
	} while (index <= lastIndex);
	_plasmaCannonLastIndex1 = 0;
	_plasmaCannonPointsMask = 0;
	_plasmaCannonExplodeFlag = false;
	_plasmaCannonObject = 0;
}

void Game::drawScreen() {
	memcpy(_video->_frontLayer, _video->_backgroundLayer, Video::W * Video::H);

	// redraw background animation sprites
	LvlBackgroundData *dat = &_res->_resLvlScreenBackgroundDataTable[_res->_currentScreenResourceNum];
	for (Sprite *spr = _typeSpritesList[0]; spr; spr = spr->nextPtr) {
		if ((spr->num & 0x1F) == 0) {
			_video->decodeSPR(spr->bitmapBits, _video->_backgroundLayer, spr->xPos, spr->yPos, 0, spr->w, spr->h);
		}
	}
	memset(_video->_shadowLayer, 0, Video::W * Video::H + 1);
	for (int i = 1; i < 8; ++i) {
		for (Sprite *spr = _typeSpritesList[i]; spr; spr = spr->nextPtr) {
			if ((spr->num & 0x2000) != 0) {
				_video->decodeSPR(spr->bitmapBits, _video->_shadowLayer, spr->xPos, spr->yPos, (spr->num >> 0xE) & 3, spr->w, spr->h);
			}
		}
	}
	for (int i = 1; i < 4; ++i) {
		for (Sprite *spr = _typeSpritesList[i]; spr; spr = spr->nextPtr) {
			if ((spr->num & 0x1000) != 0) {
				_video->decodeSPR(spr->bitmapBits, _video->_frontLayer, spr->xPos, spr->yPos, (spr->num >> 0xE) & 3, spr->w, spr->h);
			}
		}
	}
	if (_andyObject->spriteNum == 0 && (_andyObject->flags2 & 0x1F) == 4) {
		if (_plasmaCannonFirstIndex < _plasmaCannonLastIndex2) {
			drawPlasmaCannon();
		}
	}
	for (int i = 4; i < 8; ++i) {
		for (Sprite *spr = _typeSpritesList[i]; spr; spr = spr->nextPtr) {
			if ((spr->num & 0x1000) != 0) {
				_video->decodeSPR(spr->bitmapBits, _video->_frontLayer, spr->xPos, spr->yPos, (spr->num >> 0xE) & 3, spr->w, spr->h);
			}
		}
	}
	for (int i = 0; i < 24; ++i) {
		for (Sprite *spr = _typeSpritesList[i]; spr; spr = spr->nextPtr) {
			if ((spr->num & 0x2000) != 0) {
				_video->decodeSPR(spr->bitmapBits, _video->_shadowLayer, spr->xPos, spr->yPos, (spr->num >> 0xE) & 3, spr->w, spr->h);
			}
		}
	}
	for (int i = 0; i < dat->shadowCount; ++i) {
		_video->applyShadowColors(_shadowScreenMasksTable[i].x,
			_shadowScreenMasksTable[i].y,
			_shadowScreenMasksTable[i].w,
			_shadowScreenMasksTable[i].h,
			256,
			_shadowScreenMasksTable[i].w,
			_video->_shadowLayer,
			_video->_frontLayer,
			_shadowScreenMasksTable[i].projectionDataPtr,
			_shadowScreenMasksTable[i].shadowPalettePtr);
	}
	for (int i = 1; i < 12; ++i) {
		for (Sprite *spr = _typeSpritesList[i]; spr; spr = spr->nextPtr) {
			if ((spr->num & 0x1000) != 0) {
				_video->decodeSPR(spr->bitmapBits, _video->_frontLayer, spr->xPos, spr->yPos, (spr->num >> 0xE) & 3, spr->w, spr->h);
			}
		}
	}
	if (_andyObject->spriteNum == 0 && (_andyObject->flags2 & 0x1F) == 0xC) {
		if (_plasmaCannonFirstIndex < _plasmaCannonLastIndex2) {
			drawPlasmaCannon();
		}
	}
	for (int i = 12; i <= 24; ++i) {
		for (Sprite *spr = _typeSpritesList[i]; spr; spr = spr->nextPtr) {
			if ((spr->num & 0x1000) != 0) {
				_video->decodeSPR(spr->bitmapBits, _video->_frontLayer, spr->xPos, spr->yPos, (spr->num >> 0xE) & 3, spr->w, spr->h);
			}
		}
	}
}

void Game::mainLoop(int level, int checkpoint, bool levelChanged) {
	if (_loadingScreenEnabled) {
		displayLoadingScreen();
	}
	if (_resumeGame) {
		const int num = _setupConfig.currentPlayer;
		level = _setupConfig.players[num].levelNum;
		if (level > kLvl_dark) {
			level = kLvl_dark;
		}
		checkpoint = _setupConfig.players[num].checkpointNum;
		if (checkpoint != 0 && checkpoint >= _res->_datHdr.levelCheckpointsCount[level]) {
			checkpoint = _setupConfig.players[num].progress[level];
		}
		_paf->_playedMask = _setupConfig.players[num].cutscenesMask;
		debug(kDebug_GAME, "Restart at level %d checkpoint %d cutscenes 0x%x", level, checkpoint, _paf->_playedMask);
		// resume once, on the starting level
		_resumeGame = false;
	}
	_video->_font = _res->_fontBuffer;
	assert(level < kLvl_test);
	_currentLevel = level;
	createLevel();
	assert(checkpoint < _res->_datHdr.levelCheckpointsCount[level]);
	_level->_checkpoint = checkpoint;
	_mix._lock(1);
	_res->loadLevelData(_currentLevel);
	clearSoundObjects();
	_mix._lock(0);
	_mstAndyCurrentScreenNum = -1;
	_rnd.initTable();
	const int screenNum = _level->getCheckpointData(checkpoint)->screenNum;
	if (_mstDisabled) {
		_specialAnimMask = 0;
		_mstCurrentAnim = 0;
		_mstOriginPosX = Video::W / 2;
		_mstOriginPosY = Video::H / 2;
	} else {
		_currentScreen = screenNum; // bugfix: clear previous level screen number
		initMstCode();
	}
	memset(_level->_screenCounterTable, 0, sizeof(_level->_screenCounterTable));
	clearDeclaredLvlObjectsList();
	initLvlObjects();
	resetPlasmaCannonState();
	for (int i = 0; i < _res->_lvlHdr.screensCount; ++i) {
		_res->_screensState[i].s2 = 0;
	}
	_res->_currentScreenResourceNum = _andyObject->screenNum;
	_currentRightScreen = _res->_screensGrid[_res->_currentScreenResourceNum][kPosRightScreen];
	_currentLeftScreen = _res->_screensGrid[_res->_currentScreenResourceNum][kPosLeftScreen];
	if (!_mstDisabled) {
		startMstCode();
	}
	if (!_paf->_skipCutscenes && _level->_checkpoint == 0 && !levelChanged) {
		const uint8_t num = _cutscenes[_currentLevel];
		_paf->preload(num);
		_paf->play(num);
		_paf->unload(num);
		if (g_system->inp.quit) {
			return;
		}
	}
	_endLevel = false;
	resetShootLvlObjectDataTable();
	callLevel_initialize();
	restartLevel();
	do {
		const int frameTimeStamp = g_system->getTimeStamp() + _frameMs;
		levelMainLoop();
		if (g_system->inp.quit) {
			break;
		}
		const int delay = MAX<int>(10, frameTimeStamp - g_system->getTimeStamp());
		g_system->sleep(delay);
	} while (!_endLevel);
	_animBackgroundDataCount = 0;
	callLevel_terminate();
}

void Game::mixAudio(int16_t *buf, int len) {

	if (_snd_muted) {
		return;
	}

	const int kStereoSamples = _res->_isPsx ? 1792 * 2 : 1764 * 2; // stereo

	static int count = 0;

	static int16_t buffer[4096];
	static int bufferOffset = 0;
	static int bufferSize = 0;

	// flush samples from previous run
	if (bufferSize > 0) {
		const int count = len < bufferSize ? len : bufferSize;
		memcpy(buf, buffer + bufferOffset, count * sizeof(int16_t));
		buf += count;
		len -= count;
		bufferOffset += count;
		bufferSize -= count;
	}

	while (len > 0) {
		// this enqueues 1764*2 bytes for mono samples and 3528*2 bytes for stereo
		_mix._mixingQueueSize = 0;
		// 17640 + 17640 * 25 / 100 == 22050 (1.25x)
		if (count == 4) {
			mixSoundObjects17640(true);
			count = 0;
		} else {
			mixSoundObjects17640(false);
			++count;
		}

		if (len >= kStereoSamples) {
			_mix.mix(buf, kStereoSamples);
			buf += kStereoSamples;
			len -= kStereoSamples;
		} else {
			memset(buffer, 0, sizeof(buffer));
			_mix.mix(buffer, kStereoSamples);
			memcpy(buf, buffer, len * sizeof(int16_t));
			bufferOffset = len;
			bufferSize = kStereoSamples - len;
			break;
		}
	}
}

void Game::updateLvlObjectList(LvlObject **list) {
	LvlObject *ptr = *list;
	while (ptr) {
		LvlObject *next = ptr->nextPtr; // get 'next' as callback can modify linked list (eg. remove)
		if (ptr->callbackFuncPtr == &Game::lvlObjectList3Callback && list != &_lvlObjectsList3) {
			warning("lvlObject %p has callbackType3 and not in _lvlObjectsList3", ptr);
			ptr = next;
			continue;
		}
		if (ptr->callbackFuncPtr) {
			(this->*(ptr->callbackFuncPtr))(ptr);
		}
		if (ptr->bitmapBits && list != &_lvlObjectsList3) {
			addToSpriteList(ptr);
		}
		ptr = next;
	}
}

void Game::updateLvlObjectLists() {
	updateLvlObjectList(&_lvlObjectsList0);
	updateLvlObjectList(&_lvlObjectsList1);
	updateLvlObjectList(&_lvlObjectsList2);
	updateLvlObjectList(&_lvlObjectsList3);
}

LvlObject *Game::updateAnimatedLvlObjectType0(LvlObject *ptr) {
	const bool isPsx = _res->_isPsx;
	const int soundDataLen = isPsx ? sizeof(uint32_t) : sizeof(uint16_t);
	AnimBackgroundData *vg = (AnimBackgroundData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAnimBackgroundData);
	const uint8_t *data = vg->currentSpriteData + soundDataLen;
	if (_res->_currentScreenResourceNum == ptr->screenNum) {
		if (ptr->currentSound != 0xFFFF) {
			playSound(ptr->currentSound, ptr, 0, 3);
			ptr->currentSound = 0xFFFF;
		}
		if (isPsx) {
			_video->decodeBackgroundOverlayPsx(data);
		} else {
			Sprite *spr = _spritesNextPtr;
			if (spr && READ_LE_UINT16(data + 2) > 8) {
				spr->xPos = data[0];
				spr->yPos = data[1];
				spr->w = READ_LE_UINT16(data + 4);
				spr->h = READ_LE_UINT16(data + 6);
				spr->bitmapBits = data + 8;
				spr->num = ptr->flags2;
				const int index = spr->num & 0x1F;
				_spritesNextPtr = spr->nextPtr;
				spr->nextPtr = _typeSpritesList[index];
				_typeSpritesList[index] = spr;
			}
		}
	}
	int16_t soundNum = -1;
	const int len = READ_LE_UINT16(data + 2);
	const uint8_t *nextSpriteData = len + data + 2;
	switch (ptr->objectUpdateType - 1) {
	case 6:
		vg->currentSpriteData = vg->nextSpriteData;
		if (vg->currentFrame == 0) {
			vg->currentFrame = 1;
			soundNum = isPsx ? READ_LE_UINT32(vg->nextSpriteData) : READ_LE_UINT16(vg->nextSpriteData);
		}
		ptr->objectUpdateType = 4;
		break;
	case 5:
		vg->currentFrame = 0;
		vg->currentSpriteData = vg->otherSpriteData;
		ptr->objectUpdateType = 1;
		break;
	case 3:
		++vg->currentFrame;
		if (vg->currentFrame < vg->framesCount) {
			vg->currentSpriteData = nextSpriteData;
		} else {
			vg->currentFrame = 0;
			vg->currentSpriteData = vg->otherSpriteData;
			ptr->objectUpdateType = 1;
		}
		soundNum = isPsx ? READ_LE_UINT32(vg->currentSpriteData) : READ_LE_UINT16(vg->currentSpriteData);
		break;
	case 4:
		++vg->currentFrame;
		if (vg->currentFrame < vg->framesCount) { // bugfix: original uses '<=' (out of bounds)
			vg->currentSpriteData = nextSpriteData;
		} else {
			vg->currentFrame = 0;
			vg->currentSpriteData = vg->otherSpriteData;
			ptr->objectUpdateType = 1;
		}
		soundNum = isPsx ? READ_LE_UINT32(vg->currentSpriteData) : READ_LE_UINT16(vg->currentSpriteData);
		break;
	case 2:
		while (vg->currentFrame < vg->framesCount - 2) {
			++vg->currentFrame;
			vg->currentSpriteData = nextSpriteData;
			nextSpriteData += soundDataLen;
			const int len = READ_LE_UINT16(nextSpriteData + 2);
			nextSpriteData += len + 2;
		}
		data = vg->currentSpriteData + soundDataLen;
		if (_res->_currentScreenResourceNum == ptr->screenNum) {
			if (isPsx) {
				_video->decodeBackgroundOverlayPsx(data);
			} else {
				Sprite *spr = _spritesNextPtr;
				if (spr && READ_LE_UINT16(data + 2) > 8) {
					spr->w = READ_LE_UINT16(data + 4);
					spr->h = READ_LE_UINT16(data + 6);
					spr->bitmapBits = data + 8;
					spr->xPos = data[0];
					spr->yPos = data[1];
					_spritesNextPtr = spr->nextPtr;
					spr->num = ptr->flags2;
					const int index = spr->num & 0x1F;
					spr->nextPtr = _typeSpritesList[index];
					_typeSpritesList[index] = spr;
				}
			}
		}
		ptr->objectUpdateType = 1;
		return ptr->nextPtr;
	case 1:
		++vg->currentFrame;
		if (vg->currentFrame < vg->framesCount - 1) {
			vg->currentSpriteData = nextSpriteData;
			soundNum = isPsx ? READ_LE_UINT32(vg->currentSpriteData) : READ_LE_UINT16(vg->currentSpriteData);
		} else {
			if (vg->currentFrame > vg->framesCount) {
				vg->currentFrame = vg->framesCount;
			}
			ptr->objectUpdateType = 1;
			return ptr->nextPtr;
		}
		break;
	case 0:
		return ptr->nextPtr;
	default:
		soundNum = isPsx ? READ_LE_UINT32(vg->currentSpriteData) : READ_LE_UINT16(vg->currentSpriteData);
		if (ptr->hitCount == 0) {
			++vg->currentFrame;
			if (vg->currentFrame >= vg->framesCount) {
				vg->currentSpriteData = vg->nextSpriteData;
				vg->currentFrame = 1;
			} else {
				vg->currentSpriteData = nextSpriteData;
			}
		} else {
			--ptr->hitCount;
		}
		break;
	}
	if (soundNum != -1) {
		playSound(soundNum, ptr, 0, 3);
	}
	return ptr->nextPtr;
}

LvlObject *Game::updateAnimatedLvlObjectType1(LvlObject *ptr) {
	if (ptr->screenNum == _res->_currentScreenResourceNum) {
		if (_res->_screensState[_res->_currentScreenResourceNum].s0 == ptr->screenState || ptr->screenState == 0xFF) {
			if (ptr->currentSound != 0xFFFF) {
				playSound(ptr->currentSound, 0, 0, 3);
				ptr->currentSound = 0xFFFF;
			}
			uint8_t *data = (uint8_t *)getLvlObjectDataPtr(ptr, kObjectDataTypeLvlBackgroundSound);
			Sprite *spr = _spritesNextPtr;
			if (spr && READ_LE_UINT16(data + 2) > 8) {
				spr->w = READ_LE_UINT16(data + 4);
				spr->h = READ_LE_UINT16(data + 6);
				spr->bitmapBits = data + 8;
				spr->xPos = data[0];
				spr->yPos = data[1];
				_spritesNextPtr = spr->nextPtr;
				spr->num = ptr->flags2;
				const int index = spr->num & 0x1F;
				spr->nextPtr = _typeSpritesList[index];
				_typeSpritesList[index] = spr;
			}
		}
	}
	return ptr->nextPtr;
}

LvlObject *Game::updateAnimatedLvlObjectType2(LvlObject *ptr) {
	LvlObject *next, *o;

	o = next = ptr->nextPtr;
	if ((ptr->spriteNum > 15 && ptr->dataPtr == 0) || ptr->levelData0x2988 == 0) {
		if (ptr->childPtr) {
			o = ptr->childPtr->nextPtr;
		}
		return o;
	}
	const int num = ptr->screenNum;
	if (_currentScreen != num && _currentRightScreen != num && _currentLeftScreen != num) {
		return o;
	}
	if (!ptr->callbackFuncPtr) {
		warning("updateAnimatedLvlObjectType2: no callback ptr");
	} else {
		if ((this->*(ptr->callbackFuncPtr))(ptr) == 0) {
			return o;
		}
	}
	if ((ptr->flags1 & 6) == 2) {
		const int index = (15 < ptr->spriteNum) ? 5 : 7;
		ptr->yPos += calcScreenMaskDy(ptr->xPos + ptr->posTable[index].x, ptr->yPos + ptr->posTable[index].y, ptr->screenNum);
	}
	if (!ptr->bitmapBits) {
		return o;
	}
	if (_currentScreen == ptr->screenNum) {
		const uint8_t *vf = ptr->bitmapBits;

		LvlObjectData *dat = ptr->levelData0x2988;
		LvlAnimHeader *ah = (LvlAnimHeader *)(dat->animsInfoData + kLvlAnimHdrOffset) + ptr->anim;
		LvlAnimSeqHeader *ash = (LvlAnimSeqHeader *)(dat->animsInfoData + ah->seqOffset) + ptr->frame;

		int vd = (ptr->flags1 >> 4) & 0xFF;
		int vc = (ash->flags1 >> 4) & 0xFF;
		vc = (((vc ^ vd) & 3) << 14) | ptr->flags2;
		Sprite *spr = _spritesNextPtr;
		if (spr && vf) {
			spr->yPos = ptr->yPos;
			spr->xPos = ptr->xPos;
			spr->w = ptr->width;
			spr->h = ptr->height;
			spr->bitmapBits = vf;
			spr->num = vc;
			const int index = spr->num & 0x1F;
			_spritesNextPtr = spr->nextPtr;
			spr->nextPtr = _typeSpritesList[index];
			_typeSpritesList[index] = spr;
		}
	}
	if (ptr->spriteNum <= 15 || ptr->dataPtr == 0) {
		if (ptr->currentSound != 0xFFFF) {
			playSound(ptr->currentSound, ptr, 0, 3);
		}
		return o;
	}
	int a, c;
	if (ptr->dataPtr >= &_monsterObjects1Table[0] && ptr->dataPtr < &_monsterObjects1Table[kMaxMonsterObjects1]) {
		MonsterObject1 *m = (MonsterObject1 *)ptr->dataPtr;
		if (m->flagsA6 & 2) {
			assert(ptr == m->o16);
			ptr->actionKeyMask = _mstCurrentActionKeyMask;
			ptr->directionKeyMask = _andyObject->directionKeyMask;
		}
		a = m->monster1Index;
		c = 1;
	} else {
		assert(ptr->dataPtr >= &_monsterObjects2Table[0] && ptr->dataPtr < &_monsterObjects2Table[kMaxMonsterObjects2]);
		MonsterObject1 *m = ((MonsterObject2 *)ptr->dataPtr)->monster1;
		if (m) {
			a = m->monster1Index;
			c = 2;
		} else {
			a = 4;
			c = 0;
		}
	}
	if (ptr->currentSound != 0xFFFF) {
		playSound(ptr->currentSound, ptr, c, a);
	}
	return o;
}

LvlObject *Game::updateAnimatedLvlObjectTypeDefault(LvlObject *ptr) {
	return ptr->nextPtr;
}

LvlObject *Game::updateAnimatedLvlObject(LvlObject *o) {
	switch (o->type) {
	case 0:
		o = updateAnimatedLvlObjectType0(o);
		break;
	case 1:
		o = updateAnimatedLvlObjectType1(o);
		break;
	case 2:
		o = updateAnimatedLvlObjectType2(o);
		break;
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		o = updateAnimatedLvlObjectTypeDefault(o);
		break;
	default:
		error("updateAnimatedLvlObject unhandled type %d", o->type);
		break;
	}
	return o;
}

void Game::updateAnimatedLvlObjectsLeftRightCurrentScreens() {
	LvlObject *ptr = _screenLvlObjectsList[_res->_currentScreenResourceNum];
	while (ptr) {
		if (ptr->screenState == 0xFF || ptr->screenState == _res->_screensState[_res->_currentScreenResourceNum].s0) {
			ptr = updateAnimatedLvlObject(ptr);
		} else {
			ptr = ptr->nextPtr;
		}
	}
	int index = _res->_screensGrid[_res->_currentScreenResourceNum][kPosRightScreen];
	if (index != kNoScreen && _res->_screensState[index].s2 != 0) {
		ptr = _screenLvlObjectsList[index];
		while (ptr) {
			if (ptr->screenState == 0xFF || ptr->screenState == _res->_screensState[index].s0) {
				ptr = updateAnimatedLvlObject(ptr);
			} else {
				ptr = ptr->nextPtr;
			}
		}
	}
	index = _res->_screensGrid[_res->_currentScreenResourceNum][kPosLeftScreen];
	if (index != kNoScreen && _res->_screensState[index].s2 != 0) {
		ptr = _screenLvlObjectsList[index];
		while (ptr) {
			if (ptr->screenState == 0xFF || ptr->screenState == _res->_screensState[index].s0) {
				ptr = updateAnimatedLvlObject(ptr);
			} else {
				ptr = ptr->nextPtr;
			}
		}
	}
}

void Game::updatePlasmaCannonExplosionLvlObject(LvlObject *ptr) {
	ptr->actionKeyMask = 0;
	ptr->directionKeyMask = 0;
	if (_plasmaCannonDirection != 0 && _plasmaCannonLastIndex1 != 0) {
		if (_plasmaCannonObject) {
			const int _al = (_plasmaCannonObject->xPos <= _andyObject->xPos) ? 0 : 0xFF;
			ptr->directionKeyMask = (_al & ~5) | 8;
			if (_plasmaCannonObject->yPos > _andyObject->yPos) {
				ptr->directionKeyMask |= 4;
			} else {
				ptr->directionKeyMask |= 1;
			}
		} else {
			ptr->directionKeyMask = 2;
			if (_plasmaCannonPointsMask != 0) {
				if ((_plasmaCannonPointsMask & 1) != 0 || (_plasmaCannonDirection & 4) != 0) {
					ptr->directionKeyMask = 4;
				} else if ((_plasmaCannonDirection & 8) != 0) {
					ptr->directionKeyMask = 8;
				}
			}
		}
		if ((_andyObject->flags0 & 0x1F) == 4 && (_andyObject->flags0 & 0xE0) == 0xC0) {
			ptr->directionKeyMask = 1;
		}
		if (_plasmaCannonExplodeFlag) {
			ptr->actionKeyMask = 4;
			if ((_rnd._rndSeed & 1) != 0 && addLvlObjectToList3(1)) {
				_lvlObjectsList3->flags0 = _andyObject->flags0;
				_lvlObjectsList3->flags1 = _andyObject->flags1;
				_lvlObjectsList3->screenNum = _andyObject->screenNum;
				_lvlObjectsList3->flags2 = _andyObject->flags2 & ~0x2000;
				if ((ptr->directionKeyMask & 1) == 0) {
					_lvlObjectsList3->anim = 12;
					_lvlObjectsList3->flags1 ^= 0x20;
				} else {
					_lvlObjectsList3->anim = 11;
				}
				_lvlObjectsList3->frame = 0;
				setLvlObjectPosRelativeToPoint(_lvlObjectsList3, 0, _plasmaCannonXPointsTable1[_plasmaCannonLastIndex1], _plasmaCannonYPointsTable1[_plasmaCannonLastIndex1]);
			}
		}
		setLvlObjectPosRelativeToPoint(ptr, 0, _plasmaCannonXPointsTable1[_plasmaCannonLastIndex1], _plasmaCannonYPointsTable1[_plasmaCannonLastIndex1]);
	}
	updateAndyObject(ptr);
	ptr->screenNum = _andyObject->screenNum;
	ptr->flags2 = merge_bits(ptr->flags2, _andyObject->flags2, 0x18);
	ptr->flags2 = merge_bits(ptr->flags2, _andyObject->flags2 + 1, 7);
	addToSpriteList(ptr);
}

void Game::resetPlasmaCannonState() {
	_plasmaCannonDirection = 0;
	_plasmaCannonPrevDirection = 0;
	_plasmaCannonPointsSetupCounter = 0;
	_plasmaCannonLastIndex1 = 0;
	_plasmaCannonExplodeFlag = false;
	_plasmaCannonPointsMask = 0;
	_plasmaCannonFirstIndex = 16;
	_plasmaCannonLastIndex2 = 16;
}

void Game::updateAndyMonsterObjects() {
	uint8_t _dl = 3;
	LvlObject *ptr = _andyObject;
	switch (_actionDirectionKeyMaskIndex >> 4) {
	case 6:
		_hideAndyObjectFlag = false;
		if (_actionDirectionKeyMaskIndex == 0x61) {
			assert(_specialAnimLvlObject);
			_mstOriginPosX += _specialAnimLvlObject->posTable[6].x + _specialAnimLvlObject->xPos;
			_mstOriginPosY += _specialAnimLvlObject->posTable[6].y + _specialAnimLvlObject->yPos;
		}
		ptr->childPtr = 0;
		break;
	case 7: // replace Andy sprite with a custom animation
		_hideAndyObjectFlag = true;
		if (_actionDirectionKeyMaskIndex == 0x71) {
			assert(_specialAnimLvlObject);
			_mstOriginPosX += _specialAnimLvlObject->posTable[6].x + _specialAnimLvlObject->xPos;
			_mstOriginPosY += _specialAnimLvlObject->posTable[6].y + _specialAnimLvlObject->yPos;
			ptr->childPtr = _specialAnimLvlObject;
			ptr->screenNum = _specialAnimLvlObject->screenNum;
		} else {
			ptr->childPtr = 0;
		}
		break;
	case 10:
		if (_actionDirectionKeyMaskIndex != 0xA3) {
			return;
		}
		ptr->actionKeyMask = _actionDirectionKeyMaskTable[0x146];
		ptr->directionKeyMask = _actionDirectionKeyMaskTable[_actionDirectionKeyMaskIndex * 2 + 1];
		updateAndyObject(ptr);
		_actionDirectionKeyMaskIndex = 0;
		_hideAndyObjectFlag = false;
		_mstFlags |= 0x80000000;
		_dl = 1;
		break;
	default:
		return;
	}
	if (_dl & 2) {
		_actionDirectionKeyMaskIndex = 0;
		ptr->anim = _mstCurrentAnim;
		ptr->frame = 0;
		ptr->flags1 = merge_bits(ptr->flags1, _specialAnimMask, 0x30);
		setupLvlObjectBitmap(ptr);
		setLvlObjectPosRelativeToPoint(ptr, 3, _mstOriginPosX, _mstOriginPosY);
	}
	_andyActionKeysFlags = 0;
	if (ptr->spriteNum == 2) {
		removeLvlObject(ptr);
	}
}

void Game::updateInput() {
	const uint8_t inputMask = g_system->inp.mask;
	if (inputMask & SYS_INP_RUN) {
		_actionKeyMask |= kActionKeyMaskRun;
	}
	if (inputMask & SYS_INP_JUMP) {
		_actionKeyMask |= kActionKeyMaskJump;
	}
	if (inputMask & SYS_INP_SHOOT) {
		_actionKeyMask |= kActionKeyMaskShoot;
	}
	if (inputMask & SYS_INP_UP) {
		_directionKeyMask |= kDirectionKeyMaskUp;
	} else if (inputMask & SYS_INP_DOWN) {
		_directionKeyMask |= kDirectionKeyMaskDown;
	}
	if (inputMask & SYS_INP_RIGHT) {
		_directionKeyMask |= kDirectionKeyMaskRight;
	} else if (inputMask & SYS_INP_LEFT) {
		_directionKeyMask |= kDirectionKeyMaskLeft;
	}
}

void Game::levelMainLoop() {
	memset(_typeSpritesList, 0, sizeof(_typeSpritesList));
	_spritesNextPtr = &_spritesTable[0];
	for (int i = 0; i < kMaxSprites - 1; ++i) {
		_spritesTable[i].nextPtr = &_spritesTable[i + 1];
	}
	_spritesTable[kMaxSprites - 1].nextPtr = 0;
	_directionKeyMask = 0;
	_actionKeyMask = 0;
	updateInput();
	_andyObject->directionKeyMask = _directionKeyMask;
	_andyObject->actionKeyMask = _actionKeyMask;
	_video->fillBackBuffer();
	if (_andyObject->screenNum != _res->_currentScreenResourceNum) {
		preloadLevelScreenData(_andyObject->screenNum, _res->_currentScreenResourceNum);
		updateScreen(_andyObject->screenNum);
	} else if (_fadePalette && _levelRestartCounter == 0) {
		restartLevel();
	} else {
		callLevel_postScreenUpdate(_res->_currentScreenResourceNum);
		if (_currentLeftScreen != kNoScreen) {
			callLevel_postScreenUpdate(_currentLeftScreen);
		}
		if (_currentRightScreen != kNoScreen) {
			callLevel_postScreenUpdate(_currentRightScreen);
		}
	}
	_currentLevelCheckpoint = _level->_checkpoint;
	if (updateAndyLvlObject() != 0) {
		callLevel_tick();
//		_time_counter1 -= _time_counter2;
		return;
	}
	executeMstCode();
	updateLvlObjectLists();
	callLevel_tick();
	updateAndyMonsterObjects();
	if (!_hideAndyObjectFlag) {
		addToSpriteList(_andyObject);
	}
	((AndyLvlObjectData *)_andyObject->dataPtr)->dxPos = 0;
	((AndyLvlObjectData *)_andyObject->dataPtr)->dyPos = 0;
	updateAnimatedLvlObjectsLeftRightCurrentScreens();
	if (_currentLevel == kLvl_rock || _currentLevel == kLvl_lar2 || _currentLevel == kLvl_test) {
		if (_andyObject->spriteNum == 0 && _plasmaExplosionObject && _plasmaExplosionObject->nextPtr != 0) {
			updatePlasmaCannonExplosionLvlObject(_plasmaExplosionObject->nextPtr);
		}
	}
	if (_res->_sssHdr.infosDataCount != 0) {
		// sound thread signaling
	}
	if (_video->_paletteNeedRefresh) {
		_video->_paletteNeedRefresh = false;
		_video->refreshGamePalette(_video->_displayPaletteBuffer);
		g_system->copyRectWidescreen(Video::W, Video::H, _video->_backgroundLayer, _video->_palette);
	}
	drawScreen();
	if (g_system->inp.screenshot) {
		g_system->inp.screenshot = false;
		captureScreenshot();
	}
	if (_cheats != 0) {
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "P%d S%02d %d R%d", _currentLevel, _andyObject->screenNum, _res->_screensState[_andyObject->screenNum].s0, _level->_checkpoint);
		_video->drawString(buffer, (Video::W - strlen(buffer) * 8) / 2, 8, _video->findWhiteColor(), _video->_frontLayer);
	}
	if (_shakeScreenDuration != 0 || _levelRestartCounter != 0 || _video->_displayShadowLayer) {
		shakeScreen();
		uint8_t *p = _video->_shadowLayer;
		if (!_video->_displayShadowLayer) {
			p = _video->_frontLayer;
		}
		_video->updateGameDisplay(p);
	} else {
		_video->updateGameDisplay(_video->_frontLayer);
	}
	_rnd.update();
	g_system->processEvents();
	if (g_system->inp.keyPressed(SYS_INP_ESC)) { // display exit confirmation screen
		if (displayHintScreen(-1, 0)) {
			g_system->inp.quit = true;
			return;
		}
	} else {
		// displayHintScreen(1, 0);
		_video->updateScreen();
	}
}

void Game::callLevel_postScreenUpdate(int num) {
	_level->postScreenUpdate(num);
}

void Game::callLevel_preScreenUpdate(int num) {
	_level->preScreenUpdate(num);
}

Level *Game::createLevel() {
	switch (_currentLevel) {
	case 0:
		_level = Level_rock_create();
		break;
	case 1:
		_level = Level_fort_create();
		break;
	case 2:
		_level = Level_pwr1_create();
		break;
	case 3:
		_level = Level_isld_create();
		break;
	case 4:
		_level = Level_lava_create();
		break;
	case 5:
		_level = Level_pwr2_create();
		break;
	case 6:
		_level = Level_lar1_create();
		break;
	case 7:
		_level = Level_lar2_create();
		break;
	case 8:
		_level = Level_dark_create();
		break;
	}
	return _level;
}

void Game::callLevel_initialize() {
	_level->setPointers(this, _andyObject, _paf, _res, _video);
	_level->initialize();
}

void Game::callLevel_tick() {
	_level->tick();
}

void Game::callLevel_terminate() {
	_level->terminate();
	delete _level;
	_level = 0;
}

void Game::displayLoadingScreen() {
	if (_res->loadDatLoadingImage(_video->_frontLayer, _video->_palette)) {
		g_system->setPalette(_video->_palette, 256, 6);
		g_system->copyRect(0, 0, Video::W, Video::H, _video->_frontLayer, 256);
		g_system->updateScreen(false);
	}
}

int Game::displayHintScreen(int num, int pause) {
	static const int kQuitYes = 0;
	static const int kQuitNo = 1;
	int quit = kQuitYes;
	bool confirmQuit = false;
	uint8_t *quitBuffers[] = {
		_video->_frontLayer,
		_video->_shadowLayer,
	};
	muteSound();
	if (num == -1) {
		num = _res->_datHdr.yesNoQuitImage; // 'Yes'
		_res->loadDatHintImage(num + 1, _video->_shadowLayer, _video->_palette); // 'No'
		confirmQuit = true;
	}
	if (_res->loadDatHintImage(num, _video->_frontLayer, _video->_palette)) {
		g_system->setPalette(_video->_palette, 256, 6);
		g_system->copyRect(0, 0, Video::W, Video::H, _video->_frontLayer, 256);
		g_system->updateScreen(false);
		do {
			g_system->processEvents();
			if (confirmQuit) {
				const int currentQuit = quit;
				if (g_system->inp.keyReleased(SYS_INP_LEFT)) {
					quit = kQuitNo;
				}
				if (g_system->inp.keyReleased(SYS_INP_RIGHT)) {
					quit = kQuitYes;
				}
				if (currentQuit != quit) {
					g_system->copyRect(0, 0, Video::W, Video::H, quitBuffers[quit], 256);
					g_system->updateScreen(false);
				}
			}
			g_system->sleep(30);
		} while (!g_system->inp.quit && !g_system->inp.keyReleased(SYS_INP_JUMP));
		_video->_paletteNeedRefresh = true;
	}
	unmuteSound();
	return confirmQuit && quit == kQuitYes;
}

void Game::prependLvlObjectToList(LvlObject **list, LvlObject *ptr) {
	ptr->nextPtr = *list;
	*list = ptr;
}

void Game::removeLvlObjectFromList(LvlObject **list, LvlObject *ptr) {
	LvlObject *current = *list;
	if (current && ptr) {
		if (current == ptr) {
			*list = ptr->nextPtr;
		} else {
			LvlObject *prev = 0;
			do {
				prev = current;
				current = current->nextPtr;
			} while (current && current != ptr);
			assert(prev);
			prev->nextPtr = current->nextPtr;
		}
	}
}

void *Game::getLvlObjectDataPtr(LvlObject *o, int type) const {
	switch (type) {
	case kObjectDataTypeAndy:
		assert(o == _andyObject);
		assert(o->dataPtr == &_andyObjectScreenData);
		break;
	case kObjectDataTypeAnimBackgroundData:
		assert(o->dataPtr >= &_animBackgroundDataTable[0] && o->dataPtr < &_animBackgroundDataTable[kMaxBackgroundAnims]);
		break;
	case kObjectDataTypeShoot:
		assert(o->dataPtr >= &_shootLvlObjectDataTable[0] && o->dataPtr < &_shootLvlObjectDataTable[kMaxShootLvlObjectData]);
		break;
	case kObjectDataTypeLvlBackgroundSound:
		assert(o->type == 1);
		// dataPtr is _res->_resLvlScreenBackgroundDataTable[num].backgroundSoundTable + 2
		assert(o->dataPtr);
		break;
	case kObjectDataTypeMonster1:
		assert(o->dataPtr >= &_monsterObjects1Table[0] && o->dataPtr < &_monsterObjects1Table[kMaxMonsterObjects1]);
		break;
	case kObjectDataTypeMonster2:
		assert(o->dataPtr >= &_monsterObjects2Table[0] && o->dataPtr < &_monsterObjects2Table[kMaxMonsterObjects2]);
		break;
	}
	return o->dataPtr;
}

// Andy
void Game::lvlObjectType0Init(LvlObject *ptr) {
	uint8_t num = ptr->spriteNum;
	if (_currentLevel == kLvl_rock && _level->_checkpoint >= 5) {
		num = 2; // sprite without 'plasma cannon'
	}
	_andyObject = declareLvlObject(ptr->type, num);
	assert(_andyObject);
	_andyObject->xPos = ptr->xPos;
	_andyObject->yPos = ptr->yPos;
	_andyObject->screenNum = ptr->screenNum;
	_andyObject->anim = ptr->anim;
	_andyObject->frame = ptr->frame;
	_andyObject->flags2 = ptr->flags2;
	_andyObject->dataPtr = &_andyObjectScreenData;
	memset(&_andyObjectScreenData, 0, sizeof(_andyObjectScreenData));
}

// Plasma cannon explosion
void Game::lvlObjectType1Init(LvlObject *ptr) {
	AndyLvlObjectData *dataPtr = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
	if (dataPtr->shootLvlObject) {
		return;
	}
	LvlObject *o = declareLvlObject(8, 1);
	assert(o);
	o->xPos = ptr->xPos;
	o->yPos = ptr->yPos;
	o->anim = 13;
	o->frame = 0;
	o->screenNum = ptr->screenNum;
	o->flags1 = merge_bits(o->flags1, ptr->flags1, 0x30); // vg->flags1 ^= (vg->flags1 ^ ptr->flags1) & 0x30;
	o->flags2 = ptr->flags2 & ~0x2000;
	setupLvlObjectBitmap(o);
	prependLvlObjectToList(&_plasmaExplosionObject, o);

	o = declareLvlObject(8, 1);
	assert(o);
	dataPtr->shootLvlObject = o;
	o->xPos = ptr->xPos;
	o->yPos = ptr->yPos;
	o->anim = 5;
	o->frame = 0;
	o->screenNum = ptr->screenNum;
	o->flags1 = merge_bits(o->flags1, ptr->flags1, 0x30); // vg->flags1 ^= (vg->flags1 ^ ptr->flags1) & 0x30;
	o->flags2 = ptr->flags2 & ~0x2000;
	setupLvlObjectBitmap(o);
	prependLvlObjectToList(&_plasmaExplosionObject, o);
}

void Game::lvlObjectTypeInit(LvlObject *o) {
	switch (o->spriteNum) {
	case 0: // Andy with plasma cannon and helmet
	case 2: // Andy
		lvlObjectType0Init(o);
		break;
	case 1: // plasma cannon explosion
		lvlObjectType1Init(o);
		break;
	default:
		error("lvlObjectTypeInit unhandled case %d", o->spriteNum);
		break;
	}
}

void Game::lvlObjectType0CallbackHelper1() {
	uint8_t _bl, _cl, _dl;

	_cl = _dl = _andyObject->flags0;
	_bl = _andyObject->actionKeyMask;

	_dl &= 0x1F;
	_cl >>= 5;
	_cl &= 7;

	if (_currentLevel == kLvl_dark && (_bl & 4) != 0) {
		_bl &= ~4;
		_bl |= 8;
	}
	if (_dl == 3) {
		if (_cl == _dl) {
			_andyActionKeysFlags |= 2;
		}
	} else if (_dl == 7) {
		if (_cl == 5) {
			_andyActionKeysFlags |= _bl & 4;
		} else {
			_andyActionKeysFlags &= ~4;
		}
	}
	if ((_andyActionKeysFlags & 2) != 0) {
		if (_bl & 2) {
			_bl &= ~2;
		} else {
			_andyActionKeysFlags &= ~2;
		}
	}
	if (_andyActionKeysFlags & 4) {
		_bl |= 4;
	}
	if (_andyObject->spriteNum == 2 && (_bl & 5) == 5) {
		AndyLvlObjectData *data = (AndyLvlObjectData *)getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
		LvlObject *o = data->shootLvlObject;
		if (o) {
			ShootLvlObjectData *dataUnk1 = (ShootLvlObjectData *)getLvlObjectDataPtr(o, kObjectDataTypeShoot);
			if (dataUnk1->type < 4) {
				_bl |= 0xC0;
			}
		}
	}
	if (_plasmaCannonFlags & 2) {
		_bl &= ~4;
	}
	_andyObject->actionKeyMask = (_bl & _andyActionKeyMaskAnd) | _andyActionKeyMaskOr;
	_bl = _andyObject->directionKeyMask;
	_andyObject->directionKeyMask = (_bl & _andyDirectionKeyMaskAnd) | _andyDirectionKeyMaskOr;
}

int Game::calcScreenMaskDx(int x, int y, int num) {
	const uint32_t offset = screenMaskOffset(x, y);
	int ret = -(x & 7);
	if (num & 1) {
		ret += 8;
		if (_screenMaskBuffer[offset] & 2) {
			return ret;
		} else if (_screenMaskBuffer[offset - 1] & 2) {
			return ret - 8;
		}
	} else {
		--ret;
		if (_screenMaskBuffer[offset] & 2) {
			return ret;
		} else if (_screenMaskBuffer[offset + 1] & 2) {
			return ret + 8;
		} else if (_screenMaskBuffer[offset + 2] & 2) {
			return ret + 16;
		}
	}
	return 0;
}

void Game::lvlObjectType0CallbackBreathBubbles(LvlObject *ptr) {

	AndyLvlObjectData *vf = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);

	BoundingBox b;
	b.x1 = b.x2 = ptr->xPos + ptr->posTable[7].x;
	b.y1 = b.y2 = ptr->yPos + ptr->posTable[7].y;

	const int num = _pwr1_screenTransformLut[_res->_currentScreenResourceNum * 2 + 1];
	if (!clipBoundingBox(&_screenTransformRects[num], &b)) {
		++vf->unk6; // apnea counter/time
	} else {
		vf->unk6 = 0;
	}
	b.y1 -= 24;
	if (vf->unk2 == 0 && !clipBoundingBox(&_screenTransformRects[num], &b)) {

		if (addLvlObjectToList3(4)) {
			_lvlObjectsList3->xPos = b.x1;
			_lvlObjectsList3->yPos = b.y1 + 24;
			_lvlObjectsList3->screenNum = ptr->screenNum;
			_lvlObjectsList3->anim = 0;
			_lvlObjectsList3->frame = 0;
			_lvlObjectsList3->flags2 = ptr->flags2 + 1;
			_lvlObjectsList3->flags0 = (_lvlObjectsList3->flags0 & ~0x19) | 6;
			_lvlObjectsList3->flags1 &= ~0x20;
		}

		int currentApneaLevel = vf->unk3;
		static const int16_t _pwr1_apneaDuration[] = { 625, 937, 1094, 1250 };
		int newApneaLevel = 0;
		while (newApneaLevel < 4 && vf->unk6 >= _pwr1_apneaDuration[newApneaLevel]) {
			++newApneaLevel;
		}
		vf->unk3 = newApneaLevel;
		static const uint8_t _pwr1_apneaBubble[] = { 22, 20, 18, 16, 14 };
		vf->unk2 = _pwr1_apneaBubble[newApneaLevel];
		// play Andy animation when apnea level changes
		switch (currentApneaLevel) {
		case 2:
			if (newApneaLevel == 3) {
				if (_actionDirectionKeyMaskIndex < 1) {
					_actionDirectionKeyMaskIndex = 1;
					_actionDirectionKeyMaskCounter = 0;
				}
			}
			break;
		case 3:
			if (newApneaLevel == 4) {
				if (_actionDirectionKeyMaskIndex < 2) {
					_actionDirectionKeyMaskIndex = 2;
					_actionDirectionKeyMaskCounter = 0;
				}
			}
			break;
		case 4:
			if (vf->unk6 >= 1250) {
				if (_actionDirectionKeyMaskIndex < 160) {
					_actionDirectionKeyMaskIndex = 160;
					_actionDirectionKeyMaskCounter = 0;
				}
			}
			break;
		}
		assert(_lvlObjectsList3);
		switch (newApneaLevel) {
		case 0:
			_lvlObjectsList3->actionKeyMask = 1;
			break;
		case 1:
			_lvlObjectsList3->actionKeyMask = 2;
			break;
		case 2:
			_lvlObjectsList3->actionKeyMask = 4;
			break;
		case 3:
			_lvlObjectsList3->actionKeyMask = 8;
			break;
		default:
			_lvlObjectsList3->actionKeyMask = 16;
			break;
		}
	}
	if (vf->unk2 != 0) {
		--vf->unk2;
	}
}

void Game::setupSpecialPowers(LvlObject *ptr) {
	assert(ptr == _andyObject);
	AndyLvlObjectData *vf = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
	LvlObject *vg = vf->shootLvlObject;
	const uint8_t pos = ptr->flags0 & 0x1F;
	uint8_t var1 = (ptr->flags0 >> 5) & 7;
	if (pos == 4) { // release
		assert(vg->dataPtr);
		ShootLvlObjectData *va = (ShootLvlObjectData *)getLvlObjectDataPtr(vg, kObjectDataTypeShoot);
		vg->callbackFuncPtr = &Game::lvlObjectSpecialPowersCallback;
		uint8_t _cl = (ptr->flags1 >> 4) & 3;
		if (va->type == 4) {
			va->counter = 33;
			switch (var1) {
			case 0:
				va->state = ((_cl & 1) != 0) ? 5 : 0;
				break;
			case 1:
				va->state = ((_cl & 1) != 0) ? 3 : 1;
				break;
			case 2:
				va->state = ((_cl & 1) != 0) ? 4 : 2;
				break;
			case 3:
				va->state = ((_cl & 1) != 0) ? 1 : 3;
				break;
			case 4:
				va->state = ((_cl & 1) != 0) ? 2 : 4;
				break;
			case 5:
				va->state = ((_cl & 1) != 0) ? 0 : 5;
				break;
			case 6:
				va->state = 6;
				break;
			case 7:
				va->state = 7;
				break;
			}
			// dx, dy
			static const uint8_t _byte_43E670[16] = {
				0x0F, 0x00, 0xF1, 0xF6, 0xF1, 0x0A, 0x0F, 0xF6, 0x0F, 0x0A, 0xF1, 0x00, 0x00, 0xF1, 0x00, 0x0F
			};
			va->dxPos = (int8_t)_byte_43E670[va->state * 2];
			va->dyPos = (int8_t)_byte_43E670[va->state * 2 + 1];
			vg->anim = 10;
		} else {
			va->counter = 17;
			switch (var1) {
			case 0:
				vg->anim = 13;
				va->state = ((_cl & 1) != 0) ? 5 : 0;
				break;
			case 1:
				vg->anim = 12;
				va->state = ((_cl & 1) != 0) ? 3 : 1;
				_cl ^= 1;
				break;
			case 2:
				vg->anim = 12;
				va->state = ((_cl & 1) != 0) ? 4 : 2;
				_cl ^= 3;
				break;
			case 3:
				vg->anim = 12;
				va->state = ((_cl & 1) != 0) ? 1 : 3;
				break;
			case 4:
				vg->anim = 12;
				va->state = ((_cl & 1) != 0) ? 2 : 4;
				_cl ^= 2;
				break;
			case 5:
				vg->anim = 13;
				va->state = ((_cl & 1) != 0) ? 0 : 5;
				_cl ^= 1;
				break;
			case 6:
				vg->anim = 11;
				va->state = 6;
				break;
			case 7:
				vg->anim = 11;
				va->state = 7;
				_cl ^= 2;
				break;
			}
			va->dxPos = (int8_t)_specialPowersDxDyTable[va->state * 2];
			va->dyPos = (int8_t)_specialPowersDxDyTable[va->state * 2 + 1];
		}
		vg->frame = 0;
		vg->flags1 = (vg->flags1 & ~0x30) | ((_cl & 3) << 4);
		setupLvlObjectBitmap(vg);
		vg->screenNum = ptr->screenNum;
		setLvlObjectPosRelativeToObject(vg, 7, ptr, 6);
		if (_currentLevel == kLvl_isld) {
			AndyLvlObjectData *vc = (AndyLvlObjectData *)getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			vg->xPos += vc->dxPos;
		}
		vf->shootLvlObject = 0;
	} else if (pos == 7) { // hold
		switch (var1) {
		case 0:
			if (!vg) {
				if (!vf->shootLvlObject) {
					LvlObject *vd = declareLvlObject(8, 3);
					vf->shootLvlObject = vd;
					vd->dataPtr = _shootLvlObjectDataNextPtr;
					if (_shootLvlObjectDataNextPtr) {
						_shootLvlObjectDataNextPtr = _shootLvlObjectDataNextPtr->nextPtr;
						memset(vd->dataPtr, 0, sizeof(ShootLvlObjectData));
					} else {
						warning("Nothing free in _shootLvlObjectDataNextPtr");
					}
					vd->xPos = ptr->xPos;
					vd->yPos = ptr->yPos;
					vd->flags1 &= ~0x30;
					vd->screenNum = ptr->screenNum;
					vd->anim = 7;
					vd->frame = 0;
					vd->bitmapBits = 0;
					vd->flags2 = (ptr->flags2 & ~0x2000) - 1;
					prependLvlObjectToList(&_lvlObjectsList0, vd);
				}
				AndyLvlObjectData *vc = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
				LvlObject *va = vc->shootLvlObject;
				if (va) {
					if (!va->dataPtr) {
						warning("lvlObject %p with NULL dataPtr", va);
						break;
					}
					ShootLvlObjectData *vd = (ShootLvlObjectData *)getLvlObjectDataPtr(va, kObjectDataTypeShoot);
					vd->type = 0;
				}
			} else {
				if (!vg->dataPtr) {
					warning("lvlObject %p with NULL dataPtr", vg);
					break;
				}
				ShootLvlObjectData *va = (ShootLvlObjectData *)getLvlObjectDataPtr(vg, kObjectDataTypeShoot);
				vg->anim = (va->type == 4) ? 14 : 15;
				updateAndyObject(vg);
				setLvlObjectPosRelativeToObject(vg, 0, ptr, 6);
				if (_currentLevel == kLvl_isld) {
					AndyLvlObjectData *vd = (AndyLvlObjectData *)getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
					assert(vd == vf);
					vg->xPos += vd->dxPos;
				}
			}
			break;
		case 2: {
				if (!vf->shootLvlObject) {
					LvlObject *vd = declareLvlObject(8, 3);
					vf->shootLvlObject = vd;
					vd->dataPtr = _shootLvlObjectDataNextPtr;
					if (_shootLvlObjectDataNextPtr) {
						_shootLvlObjectDataNextPtr = _shootLvlObjectDataNextPtr->nextPtr;
						memset(vd->dataPtr, 0, sizeof(ShootLvlObjectData));
					} else {
						warning("Nothing free in _shootLvlObjectDataNextPtr");
					}
					vd->xPos = ptr->xPos;
					vd->yPos = ptr->yPos;
					vd->flags1 &= ~0x30;
					vd->screenNum = ptr->screenNum;
					vd->anim = 7;
					vd->frame = 0;
					vd->bitmapBits = 0;
					vd->flags2 = (ptr->flags2 & ~0x2000) - 1;
					prependLvlObjectToList(&_lvlObjectsList0, vd);
				}
				AndyLvlObjectData *vc = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
				assert(vc == vf);
				LvlObject *va = vc->shootLvlObject;
				if (va) {
					if (!va->dataPtr) {
						warning("lvlObject %p with NULL dataPtr", va);
						break;
					}
					ShootLvlObjectData *vd = (ShootLvlObjectData *)getLvlObjectDataPtr(va, kObjectDataTypeShoot);
					vd->type = 0;
				}
			}
			break;
		case 1:
			if (vg) {
				updateAndyObject(vg);
				vg->bitmapBits = 0;
			}
			break;
		case 3: {
				if (!vf->shootLvlObject) {
					LvlObject *vd = declareLvlObject(8, 3);
					vf->shootLvlObject = vd;
					vd->dataPtr = _shootLvlObjectDataNextPtr;
					if (_shootLvlObjectDataNextPtr) {
						_shootLvlObjectDataNextPtr = _shootLvlObjectDataNextPtr->nextPtr;
						memset(vd->dataPtr, 0, sizeof(ShootLvlObjectData));
					} else {
						warning("Nothing free in _shootLvlObjectDataNextPtr");
					}
					vd->xPos = ptr->xPos;
					vd->yPos = ptr->yPos;
					vd->flags1 &= ~0x30;
					vd->screenNum = ptr->screenNum;
					vd->anim = 7;
					vd->frame = 0;
					vd->bitmapBits = 0;
					vd->flags2 = (ptr->flags2 & ~0x2000) - 1;
					prependLvlObjectToList(&_lvlObjectsList0, vd);
				}
				AndyLvlObjectData *vc = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
				LvlObject *va = vc->shootLvlObject;
				if (va) {
					if (!va->dataPtr) {
						warning("lvlObject %p with NULL dataPtr", va);
						break;
					}
					ShootLvlObjectData *vd = (ShootLvlObjectData *)getLvlObjectDataPtr(va, kObjectDataTypeShoot);
					vd->type = 4; // large power
				}
			}
			break;
		case 4:
			if (vg) {
				vf->shootLvlObject = 0;
				removeLvlObjectFromList(&_lvlObjectsList0, vg);
				destroyLvlObject(vg);
			}
			break;
		}
	}
}

int Game::lvlObjectType0Callback(LvlObject *ptr) {
	AndyLvlObjectData *vf = 0;
	if (!_hideAndyObjectFlag) {
		vf = (AndyLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeAndy);
		vf->unk4 = ptr->flags0 & 0x1F;
		vf->unk5 = (ptr->flags0 >> 5) & 7;
		lvlObjectType0CallbackHelper1();
		updateAndyObject(ptr);
		if (vf->unk4 == (ptr->flags0 & 0x1F) && vf->unk4 == 5) {
			_fallingAndyFlag = true;
		} else {
			_fallingAndyFlag = false;
			_fallingAndyCounter = 0;
		}
		vf->boundingBox.x1 = ptr->xPos;
		vf->boundingBox.x2 = ptr->xPos + ptr->width - 1;
		vf->boundingBox.y1 = ptr->yPos;
		vf->boundingBox.y2 = ptr->yPos + ptr->height - 1;
		if ((ptr->flags0 & 0x300) == 0x100) {
			const int y = _res->_screensBasePos[_res->_currentScreenResourceNum].v + ptr->posTable[3].y + ptr->yPos;
			const int x = _res->_screensBasePos[_res->_currentScreenResourceNum].u + ptr->posTable[3].x + ptr->xPos;
			ptr->xPos += calcScreenMaskDx(x, y, (ptr->flags1 >> 4) & 3);
		} else if ((ptr->flags1 & 6) == 2) {
			ptr->yPos += calcScreenMaskDy(ptr->posTable[7].x + ptr->xPos, ptr->posTable[7].y + ptr->yPos, ptr->screenNum);
		}
	} else if (ptr->childPtr) {
		assert(_specialAnimLvlObject);
		if (_specialAnimLvlObject->screenNum != ptr->screenNum) {
			setLvlObjectPosRelativeToObject(ptr, 3, _specialAnimLvlObject, 6);
		}
	}
	switch (_currentLevel) {
	case 0:
	case 7:
		if (ptr->spriteNum == 0) {
			setupPlasmaCannonPoints(ptr);
		}
		break;
	case 9: // test_hod
		if (ptr->spriteNum == 0) {
			setupPlasmaCannonPoints(ptr);
		} else {
			setupSpecialPowers(ptr);
		}
		break;
	case 2: // pwr1_hod
		if (!_hideAndyObjectFlag && vf->unk4 == 6) {
			lvlObjectType0CallbackBreathBubbles(ptr);
		}
		// fall through
	case 3:
	case 4:
	case 5:
	case 6:
		setupSpecialPowers(ptr);
		break;
	case 1:
	case 8:
		break;
	}
	if (!_hideAndyObjectFlag) {
		assert(vf);
		vf->unk0 = ptr->actionKeyMask;
	}
	return _hideAndyObjectFlag;
}

int Game::lvlObjectType1Callback(LvlObject *ptr) {
	if (ptr) {
		ptr->actionKeyMask = 0;
		switch (_plasmaCannonDirection - 1) {
		case 0:
			ptr->directionKeyMask = 1;
			break;
		case 2:
			ptr->directionKeyMask = 3;
			break;
		case 1:
			ptr->directionKeyMask = 2;
			break;
		case 5:
			ptr->directionKeyMask = 6;
			break;
		case 3:
			ptr->directionKeyMask = 4;
			break;
		case 11:
			ptr->directionKeyMask = 12;
			break;
		case 7:
			ptr->directionKeyMask = 8;
			break;
		case 8:
			ptr->directionKeyMask = 9;
			break;
		default:
			ptr->directionKeyMask = 0;
			break;
		}
		setLvlObjectPosRelativeToPoint(ptr, 0, _plasmaCannonPosX[_plasmaCannonFirstIndex], _plasmaCannonPosY[_plasmaCannonFirstIndex]);
		updateAndyObject(ptr);
		ptr->flags2 = merge_bits(ptr->flags2, _andyObject->flags2, 0x18);
		ptr->flags2 = merge_bits(ptr->flags2, _andyObject->flags2 + 1, 7);
	}
	return 0;
}

int Game::lvlObjectType7Callback(LvlObject *ptr) {
	ShootLvlObjectData *dat = (ShootLvlObjectData *)getLvlObjectDataPtr(ptr, kObjectDataTypeShoot);
	if (!dat) {
		return 0;
	}
	if ((ptr->flags0 & 0x1F) == 1) {
		dat->xPosObject = ptr->posTable[7].x + ptr->xPos;
		dat->yPosObject = ptr->posTable[7].y + ptr->yPos;
		ptr->xPos += dat->dxPos;
		ptr->yPos += dat->dyPos;
		if (!_hideAndyObjectFlag && ptr->screenNum == _andyObject->screenNum && (_andyObject->flags0 & 0x1F) != 0xB && clipLvlObjectsBoundingBox(_andyObject, ptr, 68) && (_mstFlags & 0x80000000) == 0 && (_cheats & kCheatSpectreFireballNoHit) == 0) {
			dat->unk3 = 0x80;
			dat->xPosShoot = _clipBoxOffsetX;
			dat->yPosShoot = _clipBoxOffsetY;
			setAndySpecialAnimation(0xA3);
		}
		if (dat->unk3 != 0x80 && dat->counter != 0) {
			if (dat->type == 7) {
				ptr->yPos += calcScreenMaskDy(ptr->xPos + ptr->posTable[7].x, ptr->yPos + ptr->posTable[7].y + 4, ptr->screenNum);

			}
			const uint8_t ret = lvlObjectCallbackCollideScreen(ptr);
			if (ret != 0 && (dat->type != 7 || (ret & 1) == 0)) {
				dat->unk3 = 0x80;
			}
		}
		assert(dat->state < 8);
		static const uint8_t animData1[16] = {
			0x06, 0x0B, 0x05, 0x09, 0x05, 0x09, 0x05, 0x09, 0x05, 0x09, 0x06, 0x0B, 0x04, 0x07, 0x04, 0x07
		};
		static const uint8_t animData2[16] = {
			0x06, 0x0E, 0x05, 0x0E, 0x05, 0x0E, 0x05, 0x0E, 0x05, 0x0E, 0x06, 0x0E, 0x04, 0x0E, 0x04, 0x0E
		};
		const uint8_t *anim = (dat->type >= 7) ? &animData2[dat->state * 2] : &animData1[dat->state * 2];
		if (dat->counter != 0 && dat->unk3 != 0x80) {
			if (addLvlObjectToList3(ptr->spriteNum)) {
				LvlObject *o = _lvlObjectsList3;
				o->flags0 = ptr->flags0;
				o->flags1 = ptr->flags1;
				o->screenNum = ptr->screenNum;
				o->flags2 = ptr->flags2;
				o->anim = anim[1];
				if (_rnd._rndSeed & 1) {
					++o->anim;
				}
				o->frame = 0;
				if (dat->xPosShoot >= Video::W) {
					dat->xPosShoot -= _res->_screensBasePos[ptr->screenNum].u;
				}
				if (dat->yPosShoot >= Video::H) {
					dat->yPosShoot -= _res->_screensBasePos[ptr->screenNum].v;
				}
				setupLvlObjectBitmap(o);
				setLvlObjectPosRelativeToPoint(o, 0, dat->xPosObject, dat->yPosObject);
			}
		} else {
			dat->type = (dat->type < 7) ? 3 : 9;
			if (dat->counter != 0 && _actionDirectionKeyMaskIndex != 0xA3) {
				ptr->anim = anim[0];
			} else {
				ptr->anim = 3;
			}
			ptr->frame = 0;
			if (dat->xPosShoot >= Video::W) {
				dat->xPosShoot -= _res->_screensBasePos[ptr->screenNum].u;
			}
			if (dat->yPosShoot >= Video::H) {
				dat->yPosShoot -= _res->_screensBasePos[ptr->screenNum].v;
			}
			setupLvlObjectBitmap(ptr);
			setLvlObjectPosRelativeToPoint(ptr, 0, dat->xPosShoot, dat->yPosShoot);
			return 0;
		}
	} else if ((ptr->flags0 & 0x1F) == 11) {
		dat->counter = ((ptr->flags0 & 0xE0) == 0x40) ? 0 : 1;
	}
	if (dat->counter == 0) {
		if (ptr->spriteNum == 3) {
			removeLvlObjectFromList(&_lvlObjectsList0, ptr);
		} else {
			removeLvlObjectFromList(&_lvlObjectsList2, ptr);
		}
		destroyLvlObject(ptr);
	} else {
		--dat->counter;
		updateAndyObject(ptr);
	}
	if (setLvlObjectPosInScreenGrid(ptr, 3) < 0) {
		dat->counter = 0;
	}
	return 0;
}

int Game::lvlObjectType8Callback(LvlObject *ptr) {
	if (_mstDisabled) {
		ptr->actionKeyMask = _andyObject->actionKeyMask;
		ptr->directionKeyMask = _andyObject->directionKeyMask;
		if (_andyObject->spriteNum == 2 && _lvlObjectsList0) {
			warning("lvlObjectType8CallbackHelper unimplemented");
			// lvlObjectType8CallbackHelper(ptr);
		}
		updateAndyObject(ptr);
		setLvlObjectPosInScreenGrid(ptr, 7);
	} else {
		const void *dataPtr = ptr->dataPtr;
		if (!dataPtr) {
			ptr->bitmapBits = 0;
			return 0;
		}
		int vb, var4;
		MonsterObject1 *m = 0; // ve
		if (dataPtr >= &_monsterObjects1Table[0] && dataPtr < &_monsterObjects1Table[kMaxMonsterObjects1]) {
			m = (MonsterObject1 *)ptr->dataPtr;
			vb = 1;
			var4 = m->monster1Index;
			if (m->flagsA6 & 2) {
				assert(ptr == m->o16);
				m->o16->actionKeyMask = _mstCurrentActionKeyMask;
				m->o16->directionKeyMask = _andyObject->directionKeyMask;
			}
			if (m->flagsA6 & 8) {
				ptr->bitmapBits = 0;
				return 0;
			}
		} else {
			assert(dataPtr >= &_monsterObjects2Table[0] && dataPtr < &_monsterObjects2Table[kMaxMonsterObjects2]);
			MonsterObject2 *mo = (MonsterObject2 *)dataPtr;
			m = mo->monster1;
			if (m) {
				vb = 2;
				var4 = m->monster1Index;
			} else {
				vb = 0;
				var4 = 4;
			}
			m = 0; // ve = 0
			if (mo->flags24 & 8) {
				ptr->bitmapBits = 0;
				return 0;
			}
		}
		LvlObject *o = 0; // vf
		updateAndyObject(ptr);
		if (m && m->o20) {
			o = m->o20;
			o->actionKeyMask = ptr->actionKeyMask;
			o->directionKeyMask = ptr->directionKeyMask;
			updateAndyObject(o);
			setLvlObjectPosRelativeToObject(ptr, 6, o, 6);
			addToSpriteList(o);
			setLvlObjectPosInScreenGrid(o, 7);
		}
		setLvlObjectPosInScreenGrid(ptr, 7);
		if (ptr->screenNum == _currentScreen || ptr->screenNum == _currentLeftScreen || ptr->screenNum == _currentRightScreen || o || (_currentLevel == kLvl_lar2 && ptr->spriteNum == 27) || (_currentLevel == kLvl_isld && ptr->spriteNum == 26)) {
			if (ptr->currentSound != 0xFFFF) {
				playSound(ptr->currentSound, ptr, vb, var4);
			}
			if (o && o->currentSound != 0xFFFF) {
				playSound(o->currentSound, o, vb, var4);
			}
		}
	}
	if ((ptr->flags1 & 6) == 2) {
		ptr->yPos += calcScreenMaskDy(ptr->xPos + ptr->posTable[5].x, ptr->yPos + ptr->posTable[5].y, ptr->screenNum);
	}
	return 0;
}

int Game::lvlObjectList3Callback(LvlObject *o) {
	const uint8_t flags = o->flags0 & 0xFF;
	if ((o->spriteNum <= 7 && (flags & 0x1F) == 0xB) || (o->spriteNum > 7 && flags == 0x1F)) {
		if (_lvlObjectsList3 && o) {
			if (o != _lvlObjectsList3) {
				LvlObject *prev = 0;
				LvlObject *ptr = _lvlObjectsList3;
				do {
					prev = ptr;
					ptr = ptr->nextPtr;
				} while (ptr && ptr != o);
				assert(ptr);
				prev->nextPtr = ptr->nextPtr;
			} else {
				_lvlObjectsList3 = o->nextPtr;
			}
		}
		if (o->type == 8) {
			_res->decLvlSpriteDataRefCounter(o);
			o->nextPtr = _declaredLvlObjectsNextPtr;
			--_declaredLvlObjectsListCount;
			_declaredLvlObjectsNextPtr = o;
			switch (o->spriteNum) {
			case 0:
			case 2:
				o->dataPtr = 0;
				break;
			case 3:
			case 7:
				if (o->dataPtr) {
					clearShootLvlObjectData(o);
				}
				break;
			}
		}
		if (o->sssObject) {
			removeSound(o);
		}
		o->sssObject = 0;
		o->bitmapBits = 0;
	} else {
		updateAndyObject(o);
		o->actionKeyMask = 0;
		o->directionKeyMask = 0;
		if (o->currentSound != 0xFFFF) {
			playSound(o->currentSound, o, 0, 3);
		}
		if (o->bitmapBits) {
			addToSpriteList(o);
		}
	}
	return 0;
}

void Game::lvlObjectSpecialPowersCallbackHelper1(LvlObject *o) {
	int xPos = o->xPos + o->posTable[3].x;
	int yPos = o->yPos + o->posTable[3].y;
	ShootLvlObjectData *dat = (ShootLvlObjectData *)getLvlObjectDataPtr(o, kObjectDataTypeShoot);
	const uint8_t val = dat->unk3;
	if (val == 0x80) {
		dat->xPosShoot = xPos;
		dat->yPosShoot = yPos;
	}
	uint8_t screenNum = o->screenNum;
	if (xPos < 0) {
		xPos += Video::W;
		screenNum = _res->_screensGrid[screenNum][kPosLeftScreen];
	} else if (xPos >= Video::W) {
		xPos -= Video::W;
		screenNum = _res->_screensGrid[screenNum][kPosRightScreen];
	}
	if (screenNum != kNoScreen) {
		if (yPos < 0) {
			yPos += Video::H;
			screenNum = _res->_screensGrid[screenNum][kPosTopScreen];
		} else if (yPos >= Video::H) {
			yPos -= Video::H;
			screenNum = _res->_screensGrid[screenNum][kPosBottomScreen];
		}
	}
	int8_t dy = 255 - (yPos & 7);
	if (screenNum != kNoScreen) {
		const int xLevelPos = _res->_screensBasePos[screenNum].u + xPos;
		const int yLevelPos = _res->_screensBasePos[screenNum].v + yPos + 8;
		int offset = screenMaskOffset(xLevelPos, yLevelPos);
		if (_screenMaskBuffer[offset] & 1) {
			dy = -8;
			goto set_dat03;
		} else if (_screenMaskBuffer[offset + 512] & 1) {
			const int vg = screenGridOffset(xPos, yPos);
			int i = _res->findScreenGridIndex(screenNum);
			if (i < 0) {
				goto set_dat03;
			}
			const uint8_t *p = _res->_resLevelData0x470CTablePtrData + (xPos & 7);
			dy += (int8_t)p[_screenPosTable[i][vg] * 8];
			goto set_dat03;
		} else if (_screenMaskBuffer[offset - 1024] & 1) {
			dy -= 16;
			goto set_dat03;
		} else {
			dy = val;
			if (val < 0x18) {
				dat->unk3 = val + 4;
			}
			goto set_dxpos;
		}
	}
set_dat03:
	if (val == 0x18) {
		dat->type = 6;
	} else {
		dat->unk3 = 8;
	}
set_dxpos:
	if (dat->type != 6 && dat->unk3 == 0x80) {
		dat->yPosShoot += dy;
		setLvlObjectPosRelativeToPoint(o, 3, dat->xPosShoot, dat->yPosShoot);
	} else {
		dat->unk3 = 0x80;
		dat->dxPos = 0;
		dat->dyPos = 0;
	}
}

uint8_t Game::lvlObjectCallbackCollideScreen(LvlObject *o) {
	uint8_t ret = 0;
	uint8_t screenNum = o->screenNum;
	uint8_t var30 = 0;

	int yPos = o->yPos; // va
	if ((o->flags0 & 0xE0) != 0x20) {
		yPos += o->posTable[6].y;
	} else {
		yPos += o->posTable[3].y;
	}
	int xPos = o->xPos + o->posTable[3].x; // vc

	int var1C = 0;
	int var20 = 0;
	if (xPos < 0) {
		xPos += Video::W;
		var20 = -Video::W;
		screenNum = _res->_screensGrid[screenNum][kPosLeftScreen];
	} else if (xPos >= Video::W) {
		xPos -= Video::W;
		var20 = Video::W;
		screenNum = _res->_screensGrid[screenNum][kPosRightScreen];
	}
	if (screenNum != kNoScreen) {
		if (yPos < 0) {
			yPos += Video::H;
			var1C = -Video::H;
			screenNum = _res->_screensGrid[screenNum][kPosTopScreen];
		} else if (yPos >= Video::H) {
			yPos -= Video::H;
			var1C = Video::H;
			screenNum = _res->_screensGrid[screenNum][kPosBottomScreen];
		}
	}
	if (screenNum == kNoScreen) {
		return 0;
	}
	ShootLvlObjectData *dat = (ShootLvlObjectData *)getLvlObjectDataPtr(o, kObjectDataTypeShoot);
	static const uint8_t data1[] = {
		0xFF, 0x00, 0x07, 0x00, 0x0F, 0x00, 0x17, 0x00, 0x08, 0x08, 0x00, 0x00, 0xF8, 0xF8, 0xF0, 0xF0,
		0x08, 0xF8, 0x00, 0xFF, 0xF8, 0x07, 0xF0, 0x0F, 0xFF, 0x08, 0x07, 0x00, 0x0F, 0xF8, 0x17, 0xF0,
		0xFF, 0xF8, 0x07, 0xFF, 0x0F, 0x07, 0x17, 0x0F, 0x08, 0x00, 0x00, 0x00, 0xF8, 0x00, 0xF0, 0x00,
		0x00, 0x08, 0x00, 0x00, 0x00, 0xF8, 0x00, 0xF0, 0x00, 0xF8, 0x00, 0xFF, 0x00, 0x07, 0x00, 0x0F
	};
	static const uint8_t data2[] = {
		0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x08, 0x08, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x08, 0xF8, 0x08,
		0x00, 0x00, 0x08, 0x00, 0x00, 0xF8, 0x08, 0xF8, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xF8, 0xF8, 0xF8
	};
	static const int offsets1[] = {
		0, 1, 1, 1, 0, -513, -513, -513, 0, 511, 511, 511, 0, -511, -511, -511, 0, 513, 513, 513, 0, -1, -1, -1, 0, -512, -512, -512, 0, 512, 512, 512
	};
	static const int offsets2[] = {
		0, 1, 512, -1, 0, -1, 512, 1, 0, 1, -512, -1, 0, -1, -512, 1
	};
	uint8_t _bl, _cl = dat->type;
	const uint8_t *var10;
	const int *vg;
	if (_cl == 4) {
		_bl = _cl;
		const uint8_t num = (o->flags1 >> 4) & 3;
		var10 = data2 + num * 8;
		vg = offsets2 + num * 4;
	} else {
		const uint8_t num = dat->state;
		var10 = data1 + num * 8;
		vg = offsets1 + num * 4;
		_bl = (_cl != 2) ? 4 : 2;
	}
	int num;
	int var2E = _bl;
	int vd = _res->_screensBasePos[screenNum].v + yPos;
	int vf = _res->_screensBasePos[screenNum].u + xPos; // vf
	vd = screenMaskOffset(vf, vd);
	int var4 = screenGridOffset(xPos, yPos);
	if (_cl >= 4) {
		num = 0;
		vd += vg[num];
		int ve = vd;
		uint8_t _cl, _al = 0;
		while (1) {
			_cl = _screenMaskBuffer[vd];
			if ((_cl & 6) != 0) {
				ret = _al = 1;
				break;
			}
			++num;
			if (num >= var2E) {
				_al = ret;
				break;
			}
			vd += vg[num];
		}

		_bl = dat->state; // var18
		if (_bl != 6 && _bl != 1 && _bl != 3) {
			vd = ve;
			++vg;
			num = 0;
			while (1) {
				var30 = _screenMaskBuffer[vd];
				if ((var30 & 1) != 0) {
					_al = 1;
					break;
				}
				++num;
				if (num >= var2E) {
					_al = ret;
					break;
				}
				vd += *vg++;
			}
		}
		if (_cl & 6) {
			var30 = _cl;
		}
		if (_al == 0) {
			return 0;
		}

	} else {
		_bl = dat->state;
		const uint8_t mask = (_bl == 6 || _bl == 1 || _bl == 3) ? 6 : 7;
		num = 0;
		vd += *vg++;
		while (1) {
			var30 = _screenMaskBuffer[vd];
			if ((var30 & mask) != 0) {
				break;
			}
			++num;
			if (num >= var2E) {
				return ret;
			}
			vd += *vg++;
		}
	}
	dat->xPosShoot = (int8_t)var10[num * 2    ] + var20 + (xPos & ~7);
	dat->yPosShoot = (int8_t)var10[num * 2 + 1] + var1C + (yPos & ~7);
	_bl = dat->state;
	if (_bl != 2 && _bl != 4 && _bl != 7) {
		return var30;
	}
	num = _res->findScreenGridIndex(screenNum);
	if (num < 0) {
		dat->yPosShoot += 4;
		return var30;
	}
	const int vc = (o->posTable[3].x + o->xPos) & 7;
	const uint8_t *p = _res->_resLevelData0x470CTablePtrData + vc;
	dat->yPosShoot += (int8_t)p[_screenPosTable[num][var4] * 8];
	return var30;
}

int Game::lvlObjectSpecialPowersCallback(LvlObject *o) {
	if (!o->dataPtr) {
		return 0;
	}
	ShootLvlObjectData *dat = (ShootLvlObjectData *)getLvlObjectDataPtr(o, kObjectDataTypeShoot);
	const uint16_t fl = o->flags0 & 0x1F;
	if (fl == 1) {
		if (dat->unk3 != 0x80 && dat->counter != 0) {
			uint8_t _al = lvlObjectCallbackCollideScreen(o);
			if (_al != 0) {
				if (dat->type == 4 && (_al & 1) != 0 && (dat->state == 4 || dat->state == 2)) {
					dat->type = 5;
					_al -= 4;
					dat->state = (_al != 0) ? 5 : 0;
				} else {
					dat->unk3 = 0x80;
				}
			}
		}
		if (dat->type == 5) {
			dat->dyPos = 0;
			if (dat->unk3 != 0x80) {
				lvlObjectSpecialPowersCallbackHelper1(o);
			}
		}
		static const uint8_t animData1[] = {
			0x04, 0x14, 0x03, 0x16, 0x03, 0x16, 0x03, 0x16, 0x03, 0x16, 0x04, 0x14, 0x02, 0x18, 0x02, 0x18
		};
		static const uint8_t animData2[] = {
			0x04, 0x08, 0x03, 0x08, 0x03, 0x08, 0x03, 0x08, 0x03, 0x08, 0x04, 0x08, 0x02, 0x08, 0x02, 0x08
		};
		const uint8_t *p = (dat->type >= 4) ? &animData2[dat->state * 2] : &animData1[dat->state * 2];
		if (dat->unk3 != 0x80 && dat->counter != 0) {
			if (addLvlObjectToList3(o->spriteNum)) {
				LvlObject *ptr = _lvlObjectsList3;
				ptr->flags0 = o->flags0;
				ptr->flags1 = o->flags1;
				ptr->flags2 = o->flags2;
				ptr->screenNum = o->screenNum;
				ptr->anim = p[1];
				if (_rnd._rndSeed & 1) {
					++ptr->anim;
				}
				ptr->frame = 0;
				setupLvlObjectBitmap(ptr);
				setLvlObjectPosRelativeToObject(ptr, 0, o, 7);
				o->xPos += dat->dxPos;
				o->yPos += dat->dyPos;
			}
		} else {
			o->anim = p[0];
			if (dat->xPosShoot >= Video::W) {
				dat->xPosShoot -= _res->_screensBasePos[o->screenNum].u;
			}
			if (dat->yPosShoot >= Video::H) {
				dat->yPosShoot -= _res->_screensBasePos[o->screenNum].v;
			}
			if (dat->o && (dat->o->actionKeyMask & 7) == 7) {
				const uint8_t num = dat->o->spriteNum;
				if ((num >= 8 && num <= 16) || num == 28) {
					o->anim = 16;
				}
			} else {
				if (dat->counter == 0) {
					o->anim = 16;
				}
			}
			if (dat->type >= 4) {
				dat->type = 6;
				if (dat->dxPos <= 0) {
					dat->xPosShoot += 8;
				}
				if (dat->dyPos <= 0) {
					dat->yPosShoot += 8;
				}
			} else {
				dat->type = 1;
			}
			dat->dxPos = 0;
			dat->dyPos = 0;
			o->frame = 0;
			setupLvlObjectBitmap(o);
			setLvlObjectPosRelativeToPoint(o, 0, dat->xPosShoot, dat->yPosShoot);
			return 0;
		}
	} else if (fl == 11) {
		if ((o->flags0 & 0xE0) == 0x40) {
			dat->counter = 0;
		} else {
			dat->counter = 1;
		}
	}
	if (dat->counter == 0) {
		if (o->spriteNum == 3) {
			removeLvlObjectFromList(&_lvlObjectsList0, o);
		} else {
			removeLvlObjectFromList(&_lvlObjectsList2, o);
		}
		destroyLvlObject(o);
	} else {
		--dat->counter;
		updateAndyObject(o);
	}
	if (setLvlObjectPosInScreenGrid(o, 3) < 0) {
		dat->counter = 0;
	}
	return 0;
}

void Game::lvlObjectTypeCallback(LvlObject *o) {
	switch (o->spriteNum) {
	case 0:
	case 2:
		o->callbackFuncPtr = &Game::lvlObjectType0Callback;
		break;
	case 1: // plasma cannon explosion
		o->callbackFuncPtr = &Game::lvlObjectType1Callback;
		break;
	case 3:
	case 4:
	case 5:
	case 6:
	case 30:
	case 31:
	case 32:
	case 33:
	case 34:
		o->callbackFuncPtr = 0;
		break;
	case 7: // flying spectre fireball
		o->callbackFuncPtr = &Game::lvlObjectType7Callback;
		break;
	case 8: // lizard
	case 9: // spider
	case 10: // flying spectre
	case 11: // spectre
	case 12: // spectre
	case 13: // spectre
	case 14:
	case 15:
	case 16: // double spectre
	case 17:
	case 18:
	case 19:
	case 20:
	case 21: // lava
	case 22:
	case 23:
	case 24: // rope
	case 25: // swamp snake
	case 26:
	case 27: // green fire-fly
	case 28:
	case 29:
		o->callbackFuncPtr = &Game::lvlObjectType8Callback;
		break;
	default:
		warning("lvlObjectTypeCallback unhandled case %d", o->spriteNum);
		break;
	}
}

LvlObject *Game::addLvlObject(int type, int x, int y, int screen, int num, int o_anim, int o_flags1, int o_flags2, int actionKeyMask, int directionKeyMask) {
	LvlObject *ptr = 0;
	switch (type) {
	case 0:
		addLvlObjectToList1(8, num);
		ptr = _lvlObjectsList1;
		break;
	case 1:
		if (screen != _currentScreen && screen != _currentLeftScreen && screen != _currentRightScreen) {
			return 0;
		}
		ptr = findLvlObjectType2(num, screen);
		break;
	case 2:
		addLvlObjectToList3(num);
		ptr = _lvlObjectsList3;
		break;
	}
	if (!ptr) {
		return 0;
	}
	ptr->anim = o_anim;
	ptr->flags2 = o_flags2;
	ptr->frame = 0;
	ptr->flags1 = ((o_flags1 & 3) << 4) | (ptr->flags1 & ~0x30);
	ptr->actionKeyMask = actionKeyMask;
	ptr->directionKeyMask = directionKeyMask;
	setupLvlObjectBitmap(ptr);
	ptr->xPos = x - ptr->posTable[7].x;
	ptr->yPos = y - ptr->posTable[7].y;
	ptr->screenNum = screen;
	return ptr;
}

int Game::setLvlObjectPosInScreenGrid(LvlObject *o, int pos) {
	int ret = 0;
	if (o->screenNum < _res->_lvlHdr.screensCount) {
		int xPrev = o->xPos;
		int x = o->xPos + o->posTable[pos].x;
		int yPrev = o->yPos;
		int y = o->yPos + o->posTable[pos].y;
		int numPrev = o->screenNum;
		int screenNum = o->screenNum;
		if (x < 0) {
			o->screenNum = _res->_screensGrid[screenNum][kPosLeftScreen];
			o->xPos = xPrev + Video::W;
		} else if (x >= Video::W) {
			o->screenNum = _res->_screensGrid[screenNum][kPosRightScreen];
			o->xPos = xPrev - Video::W;
		}
		screenNum = o->screenNum;
		if (screenNum != kNoScreen) {
			if (y < 0) {
				o->screenNum = _res->_screensGrid[screenNum][kPosTopScreen];
				o->yPos = yPrev + Video::H;
			} else if (y >= Video::H) {
				o->screenNum = _res->_screensGrid[screenNum][kPosBottomScreen];
				o->yPos = yPrev - Video::H;
			}
		}
		screenNum = o->screenNum;
		if (screenNum == kNoScreen) {
			o->xPos = xPrev;
			o->yPos = yPrev;
			o->screenNum = numPrev;
			ret = -1;
		} else if (screenNum != numPrev) {
			ret = 1;
		}
	}
	return ret;
}

LvlObject *Game::declareLvlObject(uint8_t type, uint8_t num) {
	if (type != 8 || _res->_resLevelData0x2988PtrTable[num] != 0) {
		if (_declaredLvlObjectsListCount < kMaxLvlObjects) {
			assert(_declaredLvlObjectsNextPtr);
			LvlObject *ptr = _declaredLvlObjectsNextPtr;
			_declaredLvlObjectsNextPtr = _declaredLvlObjectsNextPtr->nextPtr;
			assert(ptr);
			++_declaredLvlObjectsListCount;
			ptr->spriteNum = num;
			ptr->type = type;
			if (type == 8) {
				_res->incLvlSpriteDataRefCounter(ptr);
				lvlObjectTypeCallback(ptr);
			}
			ptr->currentSprite = 0;
			ptr->sssObject = 0;
			ptr->nextPtr = 0;
			ptr->bitmapBits = 0;
			return ptr;
		}
	}
	return 0;
}

void Game::clearDeclaredLvlObjectsList() {
	memset(_declaredLvlObjectsList, 0, sizeof(_declaredLvlObjectsList));
	for (int i = 0; i < kMaxLvlObjects - 1; ++i) {
		_declaredLvlObjectsList[i].nextPtr = &_declaredLvlObjectsList[i + 1];
	}
	_declaredLvlObjectsList[kMaxLvlObjects - 1].nextPtr = 0;
	_declaredLvlObjectsNextPtr = &_declaredLvlObjectsList[0];
	_declaredLvlObjectsListCount = 0;
}

void Game::initLvlObjects() {
	for (int i = 0; i < _res->_lvlHdr.screensCount; ++i) {
		_screenLvlObjectsList[i] = 0;
	}
	LvlObject *prevLvlObj = 0;
	for (int i = 0; i < _res->_lvlHdr.staticLvlObjectsCount; ++i) {
		LvlObject *ptr = &_res->_resLvlScreenObjectDataTable[i];
		int index = ptr->screenNum;
		ptr->nextPtr = _screenLvlObjectsList[index];
		_screenLvlObjectsList[index] = ptr;
		switch (ptr->type) {
		case 0:
			assert(_animBackgroundDataCount < kMaxBackgroundAnims);
			ptr->dataPtr = &_animBackgroundDataTable[_animBackgroundDataCount++];
			memset(ptr->dataPtr, 0, sizeof(AnimBackgroundData));
			break;
		case 1:
			if (ptr->dataPtr) {
				warning("Trying to free _resLvlScreenBackgroundDataTable.backgroundSoundTable (i=%d index=%d)", i, index);
			}
			ptr->xPos = 0;
			ptr->yPos = 0;
			break;
		case 2:
			if (prevLvlObj == &_res->_dummyObject) {
				prevLvlObj = 0;
				ptr->childPtr = ptr->nextPtr;
			} else {
				prevLvlObj = ptr->childPtr;
			}
			break;
		}
	}
	for (int i = _res->_lvlHdr.staticLvlObjectsCount; i < _res->_lvlHdr.staticLvlObjectsCount + _res->_lvlHdr.otherLvlObjectsCount; ++i) {
		LvlObject *ptr = &_res->_resLvlScreenObjectDataTable[i];
		lvlObjectTypeInit(ptr);
	}
}

void Game::setLvlObjectSprite(LvlObject *ptr, uint8_t type, uint8_t num) {
	if (ptr->type == 8) {
		_res->decLvlSpriteDataRefCounter(ptr);
		ptr->spriteNum = num;
		ptr->type = type;
		_res->incLvlSpriteDataRefCounter(ptr);
	}
}

LvlObject *Game::findLvlObject(uint8_t type, uint8_t spriteNum, int screenNum) {
	LvlObject *ptr = _screenLvlObjectsList[screenNum];
	while (ptr) {
		if (ptr->type == type && ptr->spriteNum == spriteNum) {
			break;
		}
		ptr = ptr->nextPtr;
	}
	return ptr;
}

LvlObject *Game::findLvlObject2(uint8_t type, uint8_t dataNum, int screenNum) {
	LvlObject *ptr = _screenLvlObjectsList[screenNum];
	while (ptr) {
		if (ptr->type == type && ptr->dataNum == dataNum) {
			break;
		}
		ptr = ptr->nextPtr;
	}
	return ptr;
}

LvlObject *Game::findLvlObjectType2(int spriteNum, int screenNum) {
	LvlObject *ptr = _screenLvlObjectsList[screenNum];
	while (ptr) {
		if (ptr->type == 2 && ptr->spriteNum == spriteNum && !ptr->dataPtr) {
			break;
		}
		ptr = ptr->nextPtr;
	}
	return ptr;
}

LvlObject *Game::findLvlObjectBoundingBox(BoundingBox *box) {
	LvlObject *ptr = _lvlObjectsList0;
	while (ptr) {
		if ((ptr->flags0 & 0x1F) != 0xB || (ptr->flags0 & 0xE0) != 0x40) {
			BoundingBox b;
			b.x1 = ptr->xPos;
			b.x2 = ptr->xPos + ptr->width - 1;
			b.y1 = ptr->yPos;
			b.y2 = ptr->yPos + ptr->height - 1;
			const uint8_t *coords = _res->getLvlSpriteCoordPtr(ptr->levelData0x2988, ptr->currentSprite);
			if (updateBoundingBoxClippingOffset(box, &b, coords, (ptr->flags1 >> 4) & 3)) {
				return ptr;
			}
		}
		ptr = ptr->nextPtr;
	}
	return 0;
}

void Game::setLavaAndyAnimation(int yPos) {
	const uint8_t flags = (_andyObject->flags0) & 0x1F;
	if ((_cheats & kCheatWalkOnLava) == 0 && !_hideAndyObjectFlag) {
		if ((_mstFlags & 0x80000000) == 0) {
			uint8_t mask = 0;
			const int y = _andyObject->yPos;
			if (_andyObject->posTable[5].y + y >= yPos || _andyObject->posTable[4].y + y >= yPos) {
				mask = 0xA3;
			}
			if (flags != 2 && _andyObject->posTable[7].y + y >= yPos) {
				mask = 0xA3;
			}
			if (mask != 0 && mask > _actionDirectionKeyMaskIndex) {
				_actionDirectionKeyMaskIndex = mask;
				_actionDirectionKeyMaskCounter = 0;
			}
		} else if (flags == 0xB) {
			_mstFlags &= 0x7FFFFFFF;
		}
	}
}

void Game::updateGatesLar(LvlObject *o, uint8_t *p, int num) {
	uint32_t mask = 1 << num; // ve
	uint8_t _cl = p[0] & 15;
	if (_cl >= 3) {
		if ((o->flags0 & 0x1F) == 0) {
			if (p[3] == 0) {
				if (_cl == 3) {
					p[0] = (p[0] & ~0xB) | 4;
					p[3] = p[1];
					o->directionKeyMask = 1;
					o->actionKeyMask = 0;
				} else {
					p[0] = (p[0] & ~0xC) | 3;
					p[3] = p[2];
					o->directionKeyMask = 4;
					o->actionKeyMask = 0;
				}
			} else {
				--p[3];
				o->directionKeyMask = 0;
				o->actionKeyMask = 0;
			}
		}
	} else {
		num = p[1];
		if ((p[1] | p[2]) != 0) {
			uint8_t _dl = p[0] >> 4;
			if (_cl != _dl) {
				uint8_t _al = (p[0] & 0xF0) | _dl;
				p[0] = _al;
				if (_al & 0xF0) {
					p[3] = p[1];
				} else {
					p[3] = p[2];
				}
			}
			if (p[3] == 0) {
				if (p[0] & 0xF) {
					o->directionKeyMask = 1;
					_mstAndyVarMask &= ~mask;
				} else {
					o->directionKeyMask = 4;
					_mstAndyVarMask |= mask;
				}
				_mstLevelGatesMask |= mask;
			} else {
				--p[3];
				o->actionKeyMask = 0;
				o->directionKeyMask = 0;
			}
		} else {
			uint8_t _dl = p[0] >> 4;
			if (_cl != _dl) {
				if (p[3] != 0) {
					--p[3];
				} else {
					uint8_t _al = (p[0] & 0xF0) | _dl;
					p[0] = _al;
					if (_al & 0xF0) {
						o->directionKeyMask = 1;
						_mstAndyVarMask &= ~mask;
					} else {
						o->directionKeyMask = 4;
						_mstAndyVarMask |= mask;
					}
					_mstLevelGatesMask |= mask;
					if (o->screenNum != _currentScreen && o->screenNum != _currentLeftScreen && o->screenNum != _currentRightScreen) {
						o->actionKeyMask = 1;
					} else {
						o->actionKeyMask = 0;
					}
				}
			}
		}
	}
	int y1 = o->yPos + o->posTable[1].y; // ve
	int h1 = o->posTable[1].y - o->posTable[2].y - 7; // vc
	int x1 = o->xPos + o->posTable[1].x; // vd
	if (x1 < 0) {
		x1 = 0;
	}
	if (y1 < 0) {
		y1 = 0;
	}
	uint32_t offset = screenMaskOffset(_res->_screensBasePos[o->screenNum].u + x1, _res->_screensBasePos[o->screenNum].v + y1);
	if (h1 < 0) {
		h1 = -h1;
		for (int i = 0; i < h1 / 8; ++i) {
			memset(_screenMaskBuffer + offset, 0, 4);
			offset += 512;
		}
	} else {
		for (int i = 0; i < h1 / 8; ++i) {
			memset(_screenMaskBuffer + offset, 2, 4);
			offset += 512;
		}
	}
	if (o->screenNum == _currentScreen || (o->screenNum == _currentRightScreen && _res->_resLevelData0x2B88SizeTable[_currentRightScreen] != 0) || (o->screenNum == _currentLeftScreen && _res->_resLevelData0x2B88SizeTable[_currentLeftScreen] != 0)) {
		if (o->levelData0x2988) {
			updateAndyObject(o);
		}
	}
	int y2 = o->yPos + o->posTable[1].y; // vb
	int h2 = o->posTable[2].y - o->posTable[1].y + 7; // vc
	int x2 = o->xPos + o->posTable[1].x; // vd
	if (x2 < 0) {
		x2 = 0;
	}
	if (y2 < 0) {
		y2 = 0;
	}
	offset = screenMaskOffset(_res->_screensBasePos[o->screenNum].u + x2, _res->_screensBasePos[o->screenNum].v + y2);
	if (h2 < 0) {
		h2 = -h2;
		for (int i = 0; i < h2 / 8; ++i) {
			memset(_screenMaskBuffer + offset, 0, 4);
			offset += 512;
		}
	} else {
		for (int i = 0; i < h2 / 8; ++i) {
			memset(_screenMaskBuffer + offset, 2, 4);
			offset += 512;
		}
	}
	// gate closing on Andy
	if ((_cheats & kCheatGateNoCrush) == 0 && o->screenNum == _res->_currentScreenResourceNum && o->directionKeyMask == 4) {
		if ((o->flags0 & 0x1F) == 1 && (o->flags0 & 0xE0) == 0x40) {
			if (!_hideAndyObjectFlag && (_mstFlags & 0x80000000) == 0) {
				if (clipLvlObjectsBoundingBox(_andyObject, o, 132)) {
					setAndySpecialAnimation(0xA1);
				}
			}
		}
	}
}

void Game::updateSwitchesLar(int count, uint8_t *switchesData, BoundingBox *switchesBoundingBox, uint8_t *gatesData) {
	for (int i = 0; i < count; ++i) {
		switchesData[i * 4 + 1] &= ~0x40;
	}
	for (int i = 0; i < count; ++i) {
		if (_andyObject->screenNum == switchesData[i * 4]) {
			if ((switchesData[i * 4 + 1] & 0x10) == 0x10) { // can be actioned by a spectre
				updateSwitchesLar_checkSpectre(i, &switchesData[i * 4], &switchesBoundingBox[i], gatesData);
			}
			AndyLvlObjectData *data = (AndyLvlObjectData *)getLvlObjectDataPtr(_andyObject, kObjectDataTypeAndy);
			updateSwitchesLar_checkAndy(i, &switchesData[i * 4], &data->boundingBox, &switchesBoundingBox[i], gatesData);
		}
	}
	for (int i = 0; i < count; ++i) {
		const uint8_t _al = switchesData[i * 4 + 1];
		const uint8_t _dl = _al & 0x40;
		if (_dl == 0 && (_al & 0x80) == 0) {
			switchesData[i * 4 + 1] |= 0x80;
		} else if (_dl != 0 && (_al & 4) != 0) {
			switchesData[i * 4 + 1] &= ~1;
		}
	}
}

void Game::updateSwitchesLar_checkSpectre(int num, uint8_t *p, BoundingBox *r, uint8_t *gatesData) {
	bool found = false;
	for (LvlObject *o = _lvlObjectsList1; o && !found; o = o->nextPtr) {
		if (o->screenNum != p[0]) {
			continue;
		}
		if (!((o->spriteNum >= 11 && o->spriteNum <= 13) || o->spriteNum == 16)) {
			// not a spectre
			continue;
		}
		BoundingBox b;
		b.x1 = o->xPos;
		b.y1 = o->yPos;
		b.x2 = b.x1 + o->width - 1;
		b.y2 = b.y1 + o->height - 1;
		uint8_t *vf;
		if ((p[1] & 0x40) == 0 && clipBoundingBox(r, &b)) {
			found = true;
			if ((p[2] & 0x80) == 0 && !updateSwitchesLar_toggle(true, p[2], p[0], num, (p[1] >> 5) & 1, r)) {
				continue;
			}
			p[1] |= 0x40;
			if ((p[1] & 0x8) != 0) {
				continue;
			}
			vf = &gatesData[p[3] * 4];
			uint8_t _al = (p[1] >> 1) & 1;
			uint8_t _bl = (vf[0] >> 4);
			if (_bl != _al) {
				_bl = (_al << 4) | (vf[0] & 15);
				vf[0] = _bl;
				const uint8_t _cl = (p[1] >> 5) & 1;
				if (_cl == 1 && _al == _cl) {
					vf[3] = 4;
				}
			}
		} else {
			if ((p[1] & 0xC) == 0 && (p[1] & 0x80) != 0) {
				vf = &gatesData[p[3] * 4];
				uint8_t _al = ((~p[1]) >> 1) & 1;
				uint8_t _bl = (vf[0] >> 4);
				if (_bl != _al) {
					_bl = (_al << 4) | (vf[0] & 15);
					vf[0] = _bl;
					const uint8_t _cl = (p[1] >> 5) & 1;
					if (_cl == 1 && _al == _cl) {
						vf[3] = 4;
					}
				}
			}
		}
	}
}

int Game::updateSwitchesLar_checkAndy(int num, uint8_t *p, BoundingBox *b1, BoundingBox *b2, uint8_t *gatesData) {
	int ret = 0;
	//const uint8_t flags = _andyObject->flags0 & 0x1F;
	if ((p[1] & 0x40) == 0 && (ret = clipBoundingBox(b1, b2)) != 0) {
		if ((p[1] & 1) == 0) { // switch already actioned
			return ret;
		}
		const int flag = (p[1] >> 5) & 1;
		uint8_t _al = clipAndyLvlObjectLar(b1, b2, flag);
		_al = updateSwitchesLar_toggle(_al, p[2], p[0], num, flag, b2);
		_al = ((_al & 1) << 6) | (p[1] & ~0x40);
		p[1] = _al;
		if ((_al & 0x40) == 0) {
			return ret;
		}
		ret = 1;
		if ((_al & 8) != 0) {
			const int mask = (_al & 2) != 0 ? p[3] : -p[3];
			updateGateMaskLar(mask);
			return ret;
		}
		const uint8_t _cl = (_al >> 5) & 1;
		_al = (_al >> 1) & 1;
		p = &gatesData[p[3] * 4];
		uint8_t _bl = p[0] >> 4;
		if (_bl != _al) {
			_bl = (_al << 4) | (p[0] & 0xF);
			p[0] = _bl;
			if (_cl == 1 && _al == _cl) {
				p[3] = 4;
			}
		}
		return ret;
	}
	if ((p[1] & 0xC) == 0 && (p[1] & 0x80) != 0) {
		p = &gatesData[p[3] * 4];
		const uint8_t _cl = (p[1] >> 5) & 1;
		uint8_t _al = ((~p[1]) >> 1) & 1;

		uint8_t _bl = p[0] >> 4;
		if (_bl != _al) {
			_bl = (_al << 4) | (p[0] & 0xF);
			p[0] = _bl;
			if (_cl == 1 && _al == _cl) {
				p[3] = 4;
			}
		}
	}
	return ret;
}

int Game::updateSwitchesLar_toggle(bool flag, uint8_t dataNum, int screenNum, int switchNum, int anim, const BoundingBox *box) {
	uint8_t _al = (_andyObject->flags0 >> 5) & 7;
	uint8_t _cl = (_andyObject->flags0 & 0x1F);
	int ret = 0; // _bl
	if ((dataNum & 0x80) == 0) {
		const int dy = box->y2 - box->y1;
		const int dx = box->x2 - box->x1;
		if (dx >= dy) {
			ret = 1;
		} else {
			const uint8_t _dl = ((_andyObject->flags1) >> 4) & 3;
			if (anim != _dl) {
				return 0;
			}
			if (_cl == 3 || _cl == 5 || _cl == 2 || _al == 2 || _cl == 0 || _al == 7) {
				ret = 1;
			} else {
				setAndySpecialAnimation(3);
			}
		}
	} else {
		ret = 1;
	}
	if (ret) {
		if (!flag) {
			ret = (_andyObject->anim == 224) ? 1 : 0;
		}
		if (ret) {
			LvlObject *o = findLvlObject2(0, dataNum, screenNum);
			if (o) {
				o->objectUpdateType = 7;
			}
		}
	}
	return ret;
}

void Game::dumpSwitchesLar(int switchesCount, const uint8_t *switchesData, const BoundingBox *switchesBoundingBox, int gatesCount, const uint8_t *gatesData) {
	fprintf(stdout, "_mstAndyVarMask 0x%x _mstLevelGatesMask 0x%x\n", _mstAndyVarMask, _mstLevelGatesMask);
	for (int i = 0; i < gatesCount; ++i) {
		const uint8_t *p = gatesData + i * 4;
		fprintf(stdout, "gate %2d: state 0x%02x\n", i, p[0]);
	}
	for (int i = 0; i < switchesCount; ++i) {
		const uint8_t *p = switchesData + i * 4;
		const BoundingBox *b = &switchesBoundingBox[i];
		fprintf(stdout, "switch %2d: screen %2d (%3d,%3d,%3d,%3d) flags 0x%02x sprite %2d gate %2d\n", i, p[0], b->x1, b->y1, b->x2, b->y2, p[1], (int8_t)p[2], p[3]);
	}
}

void Game::updateScreenMaskLar(uint8_t *p, uint8_t flag) {
	if (p[1] != flag) {
		p[1] = flag;
		const uint8_t screenNum = p[4];
		uint32_t maskOffset = screenMaskOffset(_res->_screensBasePos[screenNum].u + p[2], _res->_screensBasePos[screenNum].v + p[3]);
		uint8_t *dst = _screenMaskBuffer + maskOffset;
		assert(p[0] < 6);
		const int16_t *src = _lar_screenMaskOffsets[p[0]];
		const int count = *src++;
		if (!flag) {
			for (int i = 0; i < count; ++i) {
				const int16_t offset = *src++;
				dst += offset;
				*dst &= ~8;
			}
		} else {
			for (int i = 0; i < count; ++i) {
				const int16_t offset = *src++;
				dst += offset;
				*dst |= 8;
			}
		}
	}
}

void Game::updateGateMaskLar(int num) {
	int offset, type;
	if (num < 0) {
		offset = -num * 6;
		updateScreenMaskLar(_lar1_maskData + offset, 0);
		type = 5;
	} else {
		offset = num * 6;
		updateScreenMaskLar(_lar1_maskData + offset, 1);
		type = 2;
	}
	LvlObject *o = findLvlObject2(0, _lar1_maskData[offset + 5], _lar1_maskData[offset + 4]);
	if (o) {
		o->objectUpdateType = type;
	}
}

int Game::clipAndyLvlObjectLar(BoundingBox *a /* unused */, BoundingBox *b, bool flag) {
	int ret = 0;
	const uint8_t flags = _andyObject->flags0 & 0x1F;
	if (flags != 3 && flags != 5 && !flag) {
		BoundingBox b1;
		b1.x1 = _andyObject->xPos + _andyObject->posTable[5].x - 3;
		b1.x2 = b1.x1 + 6;
		b1.y1 = _andyObject->yPos + _andyObject->posTable[5].y - 3;
		b1.y2 = b1.y1 + 6;
		ret = clipBoundingBox(&b1, b);
		if (!ret) {
			BoundingBox b2;
			b2.x1 = _andyObject->xPos + _andyObject->posTable[4].x - 3;
			b2.x2 = b2.x1 + 6;
			b2.y1 = _andyObject->yPos + _andyObject->posTable[4].y - 3;
			b2.y2 = b2.y1 + 6;
			ret = clipBoundingBox(&b2, b);
		}
	}
	return ret;
}

void Game::resetWormHoleSprites() {
	memset(_wormHoleSpritesTable, 0, sizeof(_wormHoleSpritesTable));
	_wormHoleSpritesCount = 0;
}

void Game::updateWormHoleSprites() {
	const uint8_t screenNum = _res->_currentScreenResourceNum;
	WormHoleSprite *spr = 0;
	for (int i = 0; i < _wormHoleSpritesCount; ++i) {
		if (_wormHoleSpritesTable[i].screenNum == screenNum) {
			spr = &_wormHoleSpritesTable[i];
			break;
		}
	}
	if (!spr) {
		return;
	}
	LvlObject tmp;
	memset(&tmp, 0, sizeof(tmp));
	tmp.screenNum = screenNum;
	tmp.flags2 = 0x1001;
	tmp.type = 8;
	tmp.spriteNum = 20;
	_res->incLvlSpriteDataRefCounter(&tmp);
	uint32_t yOffset = 0;
	for (int i = 0; i < 6; ++i) {
		const uint32_t *flags = &spr->flags[i];
		if (*flags != 0) {
			int xOffset = 0;
			for (int j = 0; j < 11; ++j) {
				uint8_t _al = (*flags >> (j * 2)) & 3;
				if (_al != 0 && _spritesNextPtr != 0) {
					const int xPos = spr->xPos + xOffset + 12;
					const int yPos = spr->yPos + yOffset + 16;
					if (rect_contains(spr->rect1_x1, spr->rect1_y1, spr->rect1_x2, spr->rect1_y2, xPos, yPos)) {
						_al += 3;
					} else if (rect_contains(spr->rect2_x1, spr->rect2_y1, spr->rect2_x2, spr->rect2_y2, xPos, yPos)) {
						_al += 6;
					}
					tmp.anim = _al;
					setupLvlObjectBitmap(&tmp);
					setLvlObjectPosRelativeToPoint(&tmp, 7, xPos, yPos);
					if (j & 1) {
						tmp.yPos -= 16;
					}
					if (tmp.bitmapBits) {
						Sprite *spr = _spritesNextPtr;
						spr->xPos = tmp.xPos;
						spr->yPos = tmp.yPos;
						spr->w = tmp.width;
						spr->h = tmp.height;
						spr->bitmapBits = tmp.bitmapBits;
						spr->num = tmp.flags2 & 0x3FFF;
						const int index = spr->num & 0x1F;
						_spritesNextPtr = spr->nextPtr;
						spr->nextPtr = _typeSpritesList[index];
						_typeSpritesList[index] = spr;
					}
				}
				xOffset += 24;
			}
		}
		yOffset += 32;
	}
	_res->decLvlSpriteDataRefCounter(&tmp);
}

bool Game::loadSetupCfg(bool resume) {
	_resumeGame = resume;
	if (_res->readSetupCfg(&_setupConfig)) {
		return true;
	}
	memset(&_setupConfig, 0, sizeof(_setupConfig));
	return false;
}

void Game::saveSetupCfg() {
	const int num = _setupConfig.currentPlayer;
	if (_currentLevelCheckpoint > _setupConfig.players[num].progress[_currentLevel]) {
		_setupConfig.players[num].progress[_currentLevel] = _currentLevelCheckpoint;
	}
	_setupConfig.players[num].levelNum = _currentLevel;
	_setupConfig.players[num].checkpointNum = _currentLevelCheckpoint;
	_setupConfig.players[num].cutscenesMask = _paf->_playedMask;
	_setupConfig.players[num].difficulty = _difficulty;
	_setupConfig.players[num].volume = _snd_masterVolume;
	if (_currentLevel > _setupConfig.players[num].currentLevel) {
		_setupConfig.players[num].currentLevel = _currentLevel;
	}
	_res->writeSetupCfg(&_setupConfig);
}

void Game::captureScreenshot() {
	static int screenshot = 1;

	char name[64];
	snprintf(name, sizeof(name), "screenshot-%03d.bmp", screenshot);
	FILE *fp = _fs.openSaveFile(name, true);
	if (fp) {
		saveBMP(fp, _video->_frontLayer, _video->_palette, Video::W, Video::H);
		fclose(fp);
	}

	++screenshot;
}
