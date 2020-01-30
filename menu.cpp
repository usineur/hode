
#include "game.h"
#include "menu.h"
#include "lzw.h"
#include "paf.h"
#include "resource.h"
#include "system.h"
#include "util.h"
#include "video.h"

enum {
	kTitleScreen_AssignPlayer = 0,
	kTitleScreen_Play = 1,
	kTitleScreen_Options = 2,
	kTitleScreen_Quit = 3,
	kTitleScreenPSX_Load = 3, // PSX does not have 'Quit'
	kTitleScreenPSX_Save = 4
};

enum {
	kMenu_NewGame = 0,
	kMenu_CurrentGame = 2,
	kMenu_Load = 4,
	kMenu_ResumeGame = 7,
	kMenu_Settings = 8,
	kMenu_Quit = 15,
	kMenu_Cutscenes = 17
};

static void setDefaultsSetupCfg(SetupConfig *config, int num) {
	assert(num >= 0 && num < 4);
	memset(config->players[num].progress, 0, 10);
	config->players[num].levelNum = 0;
	config->players[num].checkpointNum = 0;
	config->players[num].cutscenesMask = 0;
	memset(config->players[num].controls, 0, 32);
	config->players[num].controls[0x0] = 0x11;
	config->players[num].controls[0x4] = 0x22;
	config->players[num].controls[0x8] = 0x84;
	config->players[num].controls[0xC] = 0x48;
	config->players[num].difficulty = 1;
	config->players[num].stereo = 1;
	config->players[num].volume = 128;
	config->players[num].currentLevel = 0;
}

static bool isEmptySetupCfg(SetupConfig *config, int num) {
	return config->players[num].levelNum == 0 && config->players[num].checkpointNum == 0 && config->players[num].cutscenesMask == 0;
}

Menu::Menu(Game *g, PafPlayer *paf, Resource *res, Video *video)
	: _g(g), _paf(paf), _res(res), _video(video) {

	_config = &_g->_setupConfig;
}

void Menu::setVolume() {
	const int volume = _config->players[_config->currentPlayer].volume;
	if (volume != _g->_snd_masterVolume) {
		_g->_snd_masterVolume = volume;
		if (volume == 0) {
			_g->muteSound();
		} else {
			_g->unmuteSound();
		}
	}
}

static uint32_t readBitmapsGroup(int count, DatBitmapsGroup *bitmapsGroup, uint32_t ptrOffset, int paletteSize) {
	const uint32_t baseOffset = ptrOffset;
	for (int i = 0; i < count; ++i) {
		int size;
		if (paletteSize == 0) { // PSX
			size = le32toh(bitmapsGroup[i].offset);
			bitmapsGroup[i].offset = ptrOffset - baseOffset;
		} else {
			size = bitmapsGroup[i].w * bitmapsGroup[i].h;
			bitmapsGroup[i].offset = ptrOffset - baseOffset;
		}
		bitmapsGroup[i].palette = bitmapsGroup[i].offset + size;
		ptrOffset += size + paletteSize;
	}
	return ptrOffset - baseOffset;
}

void Menu::loadData() {
	_g->_mix._lock(1);
	_res->loadDatMenuBuffers();
	_g->clearSoundObjects();
	_g->_mix._lock(0);

	const int version = _res->_datHdr.version;

	const int paletteSize = _res->_isPsx ? 0 : 256 * 3;

	const uint8_t *ptr = _res->_menuBuffer1;
	uint32_t hdrOffset, ptrOffset = 0;

	if (version == 10) {

		_titleSprites = (DatSpritesGroup *)(ptr + ptrOffset);
		_titleSprites->size = le32toh(_titleSprites->size);
		_titleSprites->count = le16toh(_titleSprites->count);
		ptrOffset += sizeof(DatSpritesGroup) + _titleSprites->size;
		_titleSprites->firstFrameOffset = 0;

		_playerSprites = (DatSpritesGroup *)(ptr + ptrOffset);
		_playerSprites->size = le32toh(_playerSprites->size);
		_playerSprites->count = le16toh(_playerSprites->count);
		ptrOffset += sizeof(DatSpritesGroup) + _playerSprites->size;
		_playerSprites->firstFrameOffset = 0;

		_titleBitmapSize = READ_LE_UINT32(ptr + ptrOffset);
		_titleBitmapData = ptr + ptrOffset + sizeof(DatBitmap);
		ptrOffset += sizeof(DatBitmap) + _titleBitmapSize + paletteSize;

		_playerBitmapSize = READ_LE_UINT32(ptr + ptrOffset);
		_playerBitmapData = ptr + ptrOffset + sizeof(DatBitmap);
		ptrOffset += sizeof(DatBitmap) + _playerBitmapSize + paletteSize;

		const int size = READ_LE_UINT32(ptr + ptrOffset); ptrOffset += 4;
		assert((size % (16 * 10)) == 0);
		_digitsData = ptr + ptrOffset;
		ptrOffset += size;

		const int cutscenesCount = _res->_datHdr.cutscenesCount;
		_cutscenesBitmaps = (DatBitmapsGroup *)(ptr + ptrOffset);
		ptrOffset += cutscenesCount * sizeof(DatBitmapsGroup);
		_cutscenesBitmapsData = ptr + ptrOffset;
		ptrOffset += readBitmapsGroup(cutscenesCount, _cutscenesBitmaps, ptrOffset, paletteSize);

		for (int i = 0; i < kCheckpointLevelsCount; ++i) {
			DatBitmapsGroup *bitmapsGroup = (DatBitmapsGroup *)(ptr + ptrOffset);
			_checkpointsBitmaps[i] = bitmapsGroup;
			const int count = _res->_datHdr.levelCheckpointsCount[i];
			ptrOffset += count * sizeof(DatBitmapsGroup);
			_checkpointsBitmapsData[i] = ptr + ptrOffset;
			ptrOffset += readBitmapsGroup(count, bitmapsGroup, ptrOffset, paletteSize);
		}

		_soundData = ptr + ptrOffset;
		ptrOffset += _res->_datHdr.soundDataSize;

	} else if (version == 11) {

		ptr = _res->_menuBuffer1;
		hdrOffset = 4;

		ptrOffset = 4 + (2 + kOptionsCount) * sizeof(DatBitmap);
		ptrOffset += _res->_datHdr.cutscenesCount * sizeof(DatBitmapsGroup);
		for (int i = 0; i < kCheckpointLevelsCount; ++i) {
			ptrOffset += _res->_datHdr.levelCheckpointsCount[i] * sizeof(DatBitmapsGroup);
		}
		ptrOffset += _res->_datHdr.levelsCount * sizeof(DatBitmapsGroup);

		_titleBitmapSize = READ_LE_UINT32(ptr + hdrOffset);
		hdrOffset += sizeof(DatBitmap);
		_titleBitmapData = ptr + ptrOffset;
		ptrOffset += _titleBitmapSize + paletteSize;

		_playerBitmapSize = READ_LE_UINT32(ptr + hdrOffset);
		hdrOffset += sizeof(DatBitmap);
		_playerBitmapData = ptr + ptrOffset;
		ptrOffset += _playerBitmapSize + paletteSize;

		for (int i = 0; i < kOptionsCount; ++i) {
			_optionsBitmapSize[i] = READ_LE_UINT32(ptr + hdrOffset);
			hdrOffset += sizeof(DatBitmap);
			if (_optionsBitmapSize[i] != 0) {
				_optionsBitmapData[i] = ptr + ptrOffset;
				ptrOffset += _optionsBitmapSize[i] + paletteSize;
			} else {
				_optionsBitmapData[i] = 0;
			}
		}

		const int cutscenesCount = _res->_datHdr.cutscenesCount;
		_cutscenesBitmaps = (DatBitmapsGroup *)(ptr + hdrOffset);
		_cutscenesBitmapsData = ptr + ptrOffset;
		hdrOffset += cutscenesCount * sizeof(DatBitmapsGroup);
		ptrOffset += readBitmapsGroup(cutscenesCount, _cutscenesBitmaps, ptrOffset, paletteSize);

		for (int i = 0; i < kCheckpointLevelsCount; ++i) {
			DatBitmapsGroup *bitmapsGroup = (DatBitmapsGroup *)(ptr + hdrOffset);
			_checkpointsBitmaps[i] = bitmapsGroup;
			_checkpointsBitmapsData[i] = ptr + ptrOffset;
			const int count = _res->_datHdr.levelCheckpointsCount[i];
			hdrOffset += count * sizeof(DatBitmapsGroup);
			ptrOffset += readBitmapsGroup(count, bitmapsGroup, ptrOffset, paletteSize);
		}

		const int levelsCount = _res->_datHdr.levelsCount;
		_levelsBitmaps = (DatBitmapsGroup *)(ptr + hdrOffset);
		_levelsBitmapsData = ptr + ptrOffset;
		ptrOffset += readBitmapsGroup(levelsCount, _levelsBitmaps, ptrOffset, paletteSize);
	}

	ptr = _res->_menuBuffer0;
	ptrOffset = 0;

	if (version == 11) {

		_titleSprites = (DatSpritesGroup *)(ptr + ptrOffset);
		_titleSprites->size = le32toh(_titleSprites->size);
		ptrOffset += sizeof(DatSpritesGroup) + _titleSprites->size;
		_titleSprites->count = le16toh(_titleSprites->count);
		_titleSprites->firstFrameOffset = 0;

		_playerSprites = (DatSpritesGroup *)(ptr + ptrOffset);
		_playerSprites->size = le32toh(_playerSprites->size);
		ptrOffset += sizeof(DatSpritesGroup) + _playerSprites->size;
		_playerSprites->count = le16toh(_playerSprites->count);
		_playerSprites->firstFrameOffset = 0;

		_optionData = ptr + ptrOffset;
		ptrOffset += _res->_datHdr.menusCount * 8;

		const int size = READ_LE_UINT32(ptr + ptrOffset); ptrOffset += 4;
		assert((size % (16 * 10)) == 0);
		_digitsData = ptr + ptrOffset;
		ptrOffset += size;

		_soundData = ptr + ptrOffset;
		ptrOffset += _res->_datHdr.soundDataSize;
	}

	if (_res->_isPsx) {
		return;
	}

	hdrOffset = ptrOffset;
	_iconsSprites = (DatSpritesGroup *)(ptr + ptrOffset);
	const int iconsCount = _res->_datHdr.iconsCount;
	ptrOffset += iconsCount * sizeof(DatSpritesGroup);
	_iconsSpritesData = ptr + ptrOffset;
	const uint32_t baseOffset = ptrOffset;
	for (int i = 0; i < iconsCount; ++i) {
		_iconsSprites[i].size = le32toh(_iconsSprites[i].size);
		_iconsSprites[i].count = le16toh(_iconsSprites[i].count);
		_iconsSprites[i].firstFrameOffset = ptrOffset - baseOffset;
		ptrOffset += _iconsSprites[i].size;
	}

	_optionsButtonSpritesCount = READ_LE_UINT32(ptr + ptrOffset); ptrOffset += 4;
	if (_optionsButtonSpritesCount != 0) {
		_optionsButtonSpritesData = ptr + ptrOffset;
		hdrOffset = ptrOffset;
		ptrOffset += _optionsButtonSpritesCount * 20;
		const uint32_t baseOffset = ptrOffset;
		for (int i = 0; i < _optionsButtonSpritesCount; ++i) {
			WRITE_LE_UINT32(_res->_menuBuffer0 + hdrOffset, ptrOffset - baseOffset);
			DatSpritesGroup *spritesGroup = (DatSpritesGroup *)(ptr + hdrOffset + 4);
			spritesGroup->size = le32toh(spritesGroup->size);
			spritesGroup->count = le16toh(spritesGroup->count);
			hdrOffset += 20;
			ptrOffset += spritesGroup->size;
		}
	} else {
		_optionsButtonSpritesData = 0;
	}

	if (version == 10) {

		_optionData = ptr + ptrOffset;
		ptrOffset += _res->_datHdr.menusCount * 8;

		hdrOffset = ptrOffset;
		ptrOffset += kOptionsCount * sizeof(DatBitmap);
		for (int i = 0; i < kOptionsCount; ++i) {
			_optionsBitmapSize[i] = READ_LE_UINT32(ptr + hdrOffset);
			hdrOffset += sizeof(DatBitmap);
			if (_optionsBitmapSize[i] != 0) {
				_optionsBitmapData[i] = ptr + ptrOffset;
				ptrOffset += _optionsBitmapSize[i] + paletteSize;
			} else {
				_optionsBitmapData[i] = 0;
			}
		}

		const int levelsCount = _res->_datHdr.levelsCount;
		hdrOffset = ptrOffset;
		ptrOffset += levelsCount * sizeof(DatBitmapsGroup);
		_levelsBitmaps = (DatBitmapsGroup *)(ptr + hdrOffset);
		_levelsBitmapsData = ptr + ptrOffset;
		readBitmapsGroup(levelsCount, _levelsBitmaps, ptrOffset, paletteSize);
	}
}

int Menu::getSoundNum(int num) const {
	assert((num & 7) == 0);
	num /= 8;
	if (_soundData) {
		const int count = READ_LE_UINT32(_soundData + 4);
		const uint8_t *p = _soundData + 8 + count * 8;
		for (int i = 0; i < count; ++i) {
			const int count2 = READ_LE_UINT32(_soundData + 8 + i * 8 + 4);
			if (i == num) {
				assert(count2 != 0);
				return (int16_t)READ_LE_UINT16(p);
			}
			p += count2 * 2;
		}
		// sound not found
		assert((uint32_t)(p - _soundData) == _res->_datHdr.soundDataSize);
	}
	return -1;
}

void Menu::playSound(int num) {
	num = getSoundNum(num);
	if (num != -1) {
		_g->playSound(num, 0, 0, 5);
	}
}

void Menu::drawSprite(const DatSpritesGroup *spriteGroup, const uint8_t *ptr, uint32_t num) {
	ptr += spriteGroup->firstFrameOffset;
	for (uint32_t i = 0; i < spriteGroup->count; ++i) {
		const uint16_t size = READ_LE_UINT16(ptr + 2);
		if (num == i) {
			if (_res->_isPsx) {
				_video->decodeBackgroundOverlayPsx(ptr);
			} else {
				_video->decodeSPR(ptr + 8, _video->_frontLayer, ptr[0], ptr[1], 0, READ_LE_UINT16(ptr + 4), READ_LE_UINT16(ptr + 6));
			}
			break;
		}
		ptr += size + 2;
	}
}

void Menu::drawSpritePos(const DatSpritesGroup *spriteGroup, const uint8_t *ptr, int x, int y, uint32_t num) {
	assert(x != 0 || y != 0);
	ptr += spriteGroup->firstFrameOffset;
	for (uint32_t i = 0; i < spriteGroup->count; ++i) {
		const uint16_t size = READ_LE_UINT16(ptr + 2);
		if (num == i) {
			if (_res->_isPsx) {
				_video->decodeBackgroundOverlayPsx(ptr);
			} else {
				_video->decodeSPR(ptr + 8, _video->_frontLayer, x, y, 0, READ_LE_UINT16(ptr + 4), READ_LE_UINT16(ptr + 6));
			}
			break;
		}
		ptr += size + 2;
	}
}

void Menu::drawSpriteNextFrame(DatSpritesGroup *spriteGroup, int num, int x, int y) {
	const uint8_t *ptr = _iconsSpritesData;
	if (spriteGroup[num].num == 0) {
		spriteGroup[num].currentFrameOffset = spriteGroup[num].firstFrameOffset;
	}
	ptr += spriteGroup[num].currentFrameOffset;
	if (_res->_isPsx) {
		_video->decodeBackgroundOverlayPsx(ptr, x, y);
	} else {
		_video->decodeSPR(ptr + 8, _video->_frontLayer, ptr[0] + x, ptr[1] + y, 0, READ_LE_UINT16(ptr + 4), READ_LE_UINT16(ptr + 6));
	}
	++spriteGroup[num].num;
	if (spriteGroup[num].num < spriteGroup[num].count) {
		const uint16_t size = READ_LE_UINT16(ptr + 2);
		spriteGroup[num].currentFrameOffset += size + 2;
	} else {
		spriteGroup[num].num = 0;
		spriteGroup[num].currentFrameOffset = spriteGroup[num].firstFrameOffset;
	}
}

void Menu::refreshScreen(bool updatePalette) {
	if (updatePalette) {
		g_system->setPalette(_paletteBuffer, 256, 6);
	}
	_video->updateGameDisplay(_video->_frontLayer);
	g_system->updateScreen(false);
}

bool Menu::mainLoop() {
	bool ret = false;
	loadData();
	while (!g_system->inp.quit) {
		const int option = handleTitleScreen();
		if (option == kTitleScreen_AssignPlayer) {
			handleAssignPlayer();
			debug(kDebug_MENU, "currentPlayer %d", _config->currentPlayer);
		} else if (option == kTitleScreen_Play) {
			return true;
		} else if (option == kTitleScreen_Options) {
			handleOptions();
			debug(kDebug_MENU, "optionNum %d", _optionNum);
			if (_optionNum == kMenu_NewGame + 1 || _optionNum == kMenu_CurrentGame + 1 || _optionNum == kMenu_ResumeGame) {
				ret = true;
				break;
			} else if (_optionNum == kMenu_Quit + 1) {
				break;
			}
		} else if (option == kTitleScreen_Quit) {
			break;
		}
	}
	_res->unloadDatMenuBuffers();
	return ret;
}

void Menu::drawTitleScreen(int option) {
	if (_res->_isPsx) {
		_video->decodeBackgroundPsx(_titleBitmapData, _titleBitmapSize, Video::W, Video::H);
	} else {
		decodeLZW(_titleBitmapData, _video->_frontLayer);
		g_system->setPalette(_titleBitmapData + _titleBitmapSize, 256, 6);
	}
	drawSprite(_titleSprites, (const uint8_t *)&_titleSprites[1], option);
	refreshScreen(false);
}

int Menu::handleTitleScreen() {
	const int firstOption = kTitleScreen_AssignPlayer;
	const int lastOption = _res->_isPsx ? kTitleScreen_Play : kTitleScreen_Quit;
	int currentOption = kTitleScreen_Play;
	while (!g_system->inp.quit) {
		g_system->processEvents();
		if (g_system->inp.keyReleased(SYS_INP_UP)) {
			if (currentOption > firstOption) {
				playSound(0x70);
				--currentOption;
			}
		}
		if (g_system->inp.keyReleased(SYS_INP_DOWN)) {
			if (currentOption < lastOption) {
				playSound(0x70);
				++currentOption;
			}
		}
		if (g_system->inp.keyReleased(SYS_INP_SHOOT) || g_system->inp.keyReleased(SYS_INP_JUMP)) {
			playSound(0x78);
			break;
		}
		drawTitleScreen(currentOption);
		g_system->sleep(15);
	}
	return currentOption;
}

void Menu::drawDigit(int x, int y, int num) {
	assert(x >= 0 && x + 16 < Video::W && y >= 0 && y + 10 < Video::H);
	assert(num < 16);
	uint8_t *dst = _video->_frontLayer + y * Video::W + x;
	const uint8_t *src = _digitsData + num * 16;
	for (int j = 0; j < 10; ++j) {
		for (int i = 0; i < 16; ++i) {
			if (src[i] != 0) {
				dst[i] = src[i];
			}
		}
		dst += Video::W;
		src += Video::W;
	}
}

void Menu::drawBitmap(const DatBitmapsGroup *bitmapsGroup, const uint8_t *bitmapData, int x, int y, int w, int h, uint8_t baseColor) {
	if (_res->_isPsx) {
		const int size = bitmapsGroup->palette - bitmapsGroup->offset;
		_video->decodeBackgroundPsx(bitmapData, size, w, h, x, y);
		return;
	}
	const int srcPitch = w;
	if (x < 0) {
		bitmapData -= x;
		w += x;
		x = 0;
	}
	if (x + w > Video::W) {
		w = Video::W - x;
	}
	if (y < 0) {
		bitmapData -= y * srcPitch;
		h += y;
		y = 0;
	}
	if (y + h > Video::H) {
		h = Video::H - y;
	}
	uint8_t *dst = _video->_frontLayer + y * Video::W + x;
	for (int j = 0; j < h; ++j) {
		for (int i = 0; i < w; ++i) {
			dst[i] = baseColor - bitmapsGroup->colors + bitmapData[i];
		}
		dst += Video::W;
		bitmapData += srcPitch;
	}
}

void Menu::setCurrentPlayer(int num) {
	debug(kDebug_MENU, "setCurrentPlayer %d", num);
	setLevelCheckpoint(num);
	_levelNum = _config->players[num].levelNum;
	if (_levelNum > kLvl_dark) {
		_levelNum = kLvl_dark;
	}
	if (_levelNum == kLvl_dark) {
		_levelNum = 7;
		_checkpointNum = 11;
	} else {
		_checkpointNum = _config->players[num].checkpointNum;
		if (_checkpointNum >= _res->_datHdr.levelCheckpointsCount[_levelNum]) {
			_checkpointNum = _res->_datHdr.levelCheckpointsCount[_levelNum] - 1;
		}
	}
	const DatBitmapsGroup *bitmap = &_checkpointsBitmaps[_levelNum][_checkpointNum];
	memcpy(_paletteBuffer + 205 * 3, _checkpointsBitmapsData[_levelNum] + bitmap->palette, 50 * 3);
	g_system->setPalette(_paletteBuffer, 256, 6);
}

void Menu::setLevelCheckpoint(int num) {
	for (int i = 0; i < kCheckpointLevelsCount; ++i) {
		uint8_t checkpoint = _config->players[num].progress[i] + 1;
		if (checkpoint >= _res->_datHdr.levelCheckpointsCount[i]) {
			checkpoint = _res->_datHdr.levelCheckpointsCount[i];
		}
		_lastLevelCheckpointNum[i] = checkpoint;
	}
}

void Menu::drawPlayerProgress(int state, int cursor) {
	if (_res->_isPsx) {
		_video->decodeBackgroundPsx(_playerBitmapData, _playerBitmapSize, Video::W, Video::H);
	} else {
		decodeLZW(_playerBitmapData, _video->_frontLayer);
	}
	int player = 0;
	for (int y = 96; y < 164; y += 17) {
		if (isEmptySetupCfg(_config, player)) {
			drawSpritePos(_playerSprites, (const uint8_t *)&_playerSprites[1], 82, y - 3, 3); // empty
		} else {
			int levelNum = _config->players[player].levelNum;
			int checkpointNum;
			if (levelNum == kLvl_dark) {
				levelNum = 7;
				checkpointNum = 11;
			} else {
				checkpointNum = _config->players[player].checkpointNum;
			}
			drawDigit(145, y, levelNum + 1);
			drawDigit(234, y, checkpointNum + 1);
		}
		++player;
	}
	player = (cursor == 0 || cursor == 5) ? _config->currentPlayer : (cursor - 1);
	uint8_t *p = _video->_frontLayer;
	const int offset = (player * 17) + 92;
	for (int i = 0; i < 16; ++i) { // player
		memcpy(p + i * Video::W +  6935, p + i * Video::W + (offset + 1) * 256 +   8, 72);
	}
	for (int i = 0; i < 16; ++i) { // level
		memcpy(p + i * Video::W + 11287, p + i * Video::W + (offset + 1) * 256 +  83, 76);
	}
	for (int i = 0; i < 16; ++i) { // checkpoint
		memcpy(p + i * Video::W + 15639, p + i * Video::W + (offset + 1) * 256 + 172, 76);
	}
	if (!isEmptySetupCfg(_config, player)) {
		DatBitmapsGroup *bitmap = &_checkpointsBitmaps[_levelNum][_checkpointNum];
		const uint8_t *src = _checkpointsBitmapsData[_levelNum] + bitmap->offset;
		drawBitmap(bitmap, src, 132, 0, bitmap->w, bitmap->h, 205);
	}
	if (cursor > 0) {
		if (cursor <= 4) { // highlight one player
			drawSpritePos(_playerSprites, (const uint8_t *)&_playerSprites[1], 2, cursor * 17 + 74, 8);
			drawSprite(_playerSprites, (const uint8_t *)&_playerSprites[1], 6);
		} else if (cursor == 5) { // cancel
			drawSprite(_playerSprites, (const uint8_t *)&_playerSprites[1], 7);
		}
	}
	drawSprite(_playerSprites, (const uint8_t *)&_playerSprites[1], state); // Yes/No
	if (state > 2) {
		drawSprite(_playerSprites, (const uint8_t *)&_playerSprites[1], 2); // MessageBox
	}
	refreshScreen(false);
}

void Menu::handleAssignPlayer() {
	memcpy(_paletteBuffer, _playerBitmapData + _playerBitmapSize, 256 * 3);
	int state = 1;
	int cursor = 0;
	setCurrentPlayer(_config->currentPlayer);
	drawPlayerProgress(state, cursor);
	while (!g_system->inp.quit) {
		g_system->processEvents();
		if (g_system->inp.keyReleased(SYS_INP_SHOOT) || g_system->inp.keyReleased(SYS_INP_JUMP)) {
			if (state != 0 && cursor == 5) {
				playSound(0x80);
			} else {
				playSound(0x78);
			}
			if (state == 0) {
				// return to title screen
				return;
			}
			if (cursor == 0) {
				cursor = _config->currentPlayer + 1;
			} else {
				if (cursor == 5) {
					cursor = 0;
				} else if (state == 1) { // select
					--cursor;
					_config->currentPlayer = cursor;
					// setVolume();
					cursor = 0;
				} else if (state == 2) { // clear
					state = 5; // 'No'
				} else {
					if (state == 4) { // 'Yes', clear confirmation
						--cursor;
						setDefaultsSetupCfg(_config, cursor);
						// setVolume();
					}
					setCurrentPlayer(_config->currentPlayer);
					state = 2;
					cursor = 0;
				}
			}
		}
		if (cursor != 0 && state < 3) {
			if (g_system->inp.keyReleased(SYS_INP_UP)) {
				if (cursor > 1) {
					playSound(0x70);
					--cursor;
					setCurrentPlayer(cursor - 1);
				}
			}
			if (g_system->inp.keyReleased(SYS_INP_DOWN)) {
				if (cursor < 5) {
					playSound(0x70);
					++cursor;
					setCurrentPlayer((cursor == 5) ? _config->currentPlayer : (cursor - 1));
				}
			}
		} else {
			if (g_system->inp.keyReleased(SYS_INP_LEFT)) {
				if (state == 1 || state == 2 || state == 5) {
					playSound(0x70);
					--state;
				}
			}
			if (g_system->inp.keyReleased(SYS_INP_RIGHT)) {
				if (state == 0 || state == 1 || state == 4) {
					playSound(0x70);
					++state;
				}
			}
		}
		drawPlayerProgress(state, cursor);
		g_system->sleep(15);
	}
}

void Menu::updateBitmapsCircularList(const DatBitmapsGroup *bitmapsGroup, const uint8_t *bitmapData, int num, int count) {
	_bitmapCircularListIndex[1] = num;
	if (count > 1) {
		_bitmapCircularListIndex[2] = (num + 1) % count;
	} else {
		_bitmapCircularListIndex[2] = -1;
	}
	if (count > 2) {
		_bitmapCircularListIndex[0] = (num + count - 1) % count;
	} else {
		_bitmapCircularListIndex[0] = -1;
	}
	if (bitmapsGroup == _cutscenesBitmaps) {
		for (int i = 0; i < 3; ++i) {
			const int num = _bitmapCircularListIndex[i];
			if (num != -1) {
				_bitmapCircularListIndex[i] = _cutsceneIndexes[num];
			}
		}
	}
	for (int i = 0; i < 3; ++i) {
		if (_bitmapCircularListIndex[i] != -1) {
			const DatBitmapsGroup *bitmap = &bitmapsGroup[_bitmapCircularListIndex[i]];
			memcpy(_paletteBuffer + (105 + 50 * i) * 3, bitmapData + bitmap->palette, 50 * 3);
		}
	}
	g_system->setPalette(_paletteBuffer, 256, 6);
}

void Menu::drawBitmapsCircularList(const DatBitmapsGroup *bitmapsGroup, const uint8_t *bitmapData, int num, int count, bool updatePalette) {
	memcpy(_paletteBuffer, _optionsBitmapData[_optionNum] + _optionsBitmapSize[_optionNum], 768);
	if (updatePalette) {
		g_system->setPalette(_paletteBuffer, 256, 6);
	}
	updateBitmapsCircularList(bitmapsGroup, bitmapData, num, count);
	static const int xPos[] = { -60, 68, 196 };
	for (int i = 0; i < 3; ++i) {
		if (_bitmapCircularListIndex[i] != -1) {
			const DatBitmapsGroup *bitmap = &bitmapsGroup[_bitmapCircularListIndex[i]];
			drawBitmap(bitmap, bitmapData + bitmap->offset, xPos[i], 14, bitmap->w, bitmap->h, 105 + 50 * i);
		}
	}
}

void Menu::drawCheckpointScreen() {
	decodeLZW(_optionsBitmapData[_optionNum], _video->_frontLayer);
	drawBitmapsCircularList(_checkpointsBitmaps[_levelNum], _checkpointsBitmapsData[_levelNum], _checkpointNum, _lastLevelCheckpointNum[_levelNum], false);
	drawSpriteNextFrame(_iconsSprites, 5, 0, 0);
	drawSpritePos(&_iconsSprites[0], _iconsSpritesData, 119, 108, (_checkpointNum + 1) / 10);
	drawSpritePos(&_iconsSprites[0], _iconsSpritesData, 127, 108, (_checkpointNum + 1) % 10);
	const int num = _loadCheckpointButtonState;
	if (num > 1 && num < 7) {
		drawSprite(&_iconsSprites[9], _iconsSpritesData, num - 2);
	} else {
		drawSpriteNextFrame(_iconsSprites, (num != 0) ? 8 : 7, 0, 0);
	}
	drawSpriteNextFrame(_iconsSprites, 6, 0, 0);
	refreshScreen(false);
}

void Menu::drawLevelScreen() {
	decodeLZW(_optionsBitmapData[_optionNum], _video->_frontLayer);
	drawSprite(&_iconsSprites[1], _iconsSpritesData, _levelNum);
	DatBitmapsGroup *bitmap = &_levelsBitmaps[_levelNum];
	drawBitmap(bitmap, _levelsBitmapsData + bitmap->offset, 23, 10, bitmap->w, bitmap->h, 192);
	drawSpriteNextFrame(_iconsSprites, 4, 0, 0);
	drawSpriteNextFrame(_iconsSprites, (_loadLevelButtonState != 0) ? 3 : 2, 0, 0);
	refreshScreen(false);
}

void Menu::drawCutsceneScreen() {
	decodeLZW(_optionsBitmapData[_optionNum], _video->_frontLayer);
	drawBitmapsCircularList(_cutscenesBitmaps, _cutscenesBitmapsData, _cutsceneNum, _cutsceneIndexesCount, false);
	drawSpriteNextFrame(_iconsSprites, 10, 0, 0);
	drawSpritePos(&_iconsSprites[0], _iconsSpritesData, 119, 108, (_cutsceneNum + 1) / 10);
	drawSpritePos(&_iconsSprites[0], _iconsSpritesData, 127, 108, (_cutsceneNum + 1) % 10);
	const int num = _loadCutsceneButtonState;
	if (num > 1 && num < 7) {
		drawSprite(&_iconsSprites[14], _iconsSpritesData, num - 2);
	} else {
		drawSpriteNextFrame(_iconsSprites, (num != 0) ? 13 : 12, 0, 0);
	}
	drawSpriteNextFrame(_iconsSprites, 11, 0, 0);
	refreshScreen(false);
}

void Menu::handleSettingsScreen(int num) {
}

void Menu::changeToOption(int num) {
	const uint8_t *data = &_optionData[num * 8];
	const int button = data[6];
	if (button != 0xFF) {
		assert(button < _optionsButtonSpritesCount);
		_currentOptionButton = _optionsButtonSpritesData + button * 20;
	} else {
		_currentOptionButton = 0;
	}
	_paf->play(data[5]);
	if (_optionNum == kMenu_NewGame + 1) {
		_config->players[_config->currentPlayer].levelNum = 0;
		_config->players[_config->currentPlayer].checkpointNum = 0;
	} else if (_optionNum == kMenu_CurrentGame + 1) {
		// _config->players[_config->currentPlayer].levelNum = _g->_currentLevel;
		// _config->players[_config->currentPlayer].checkpointNum = _g->_currentLevelCheckpoint;
	} else if (_optionNum == kMenu_Load + 1) {
		_loadLevelButtonState = 0;
		_levelNum = 0;
		memcpy(_paletteBuffer, _optionsBitmapData[5] + _optionsBitmapSize[5], 768);
		memcpy(_paletteBuffer + 192 * 3, _levelsBitmapsData + _levelsBitmaps[_levelNum].palette, 64 * 3);
		g_system->setPalette(_paletteBuffer, 256, 6);
		drawLevelScreen();
	} else if (_optionNum == kMenu_Load + 2) {
		_loadCheckpointButtonState = 0;
		_checkpointNum = 0;
		setLevelCheckpoint(_config->currentPlayer);
		memcpy(_paletteBuffer, _optionsBitmapData[6] + _optionsBitmapSize[6], 768);
		g_system->setPalette(_paletteBuffer, 256, 6);
		drawCheckpointScreen();
	} else if (_optionNum == kMenu_Settings + 1) {
		memcpy(_paletteBuffer, _optionsBitmapData[0] + _optionsBitmapSize[0], 768);
		handleSettingsScreen(5);
	} else if (_optionNum == kMenu_Cutscenes + 1) {
		_loadCutsceneButtonState = 0;
		_cutsceneNum = 0;
		drawCutsceneScreen();
	} else if (_optionsBitmapSize[_optionNum] != 0) {
		decodeLZW(_optionsBitmapData[_optionNum], _video->_frontLayer);
		memcpy(_paletteBuffer, _optionsBitmapData[_optionNum] + _optionsBitmapSize[_optionNum], 256 * 3);
		refreshScreen(true);
	}
}

void Menu::handleLoadLevel(int num) {
	const uint8_t *data = &_optionData[num * 8];
	num = data[5];
	if (num == 16) {
		if (_loadLevelButtonState != 0) {
			playSound(0x70);
		}
		_loadLevelButtonState = 0;
	} else if (num == 17) {
		if (_loadLevelButtonState == 0) {
			playSound(0x70);
		}
		_loadLevelButtonState = 1;
	} else if (num == 18) {
		if (_loadLevelButtonState != 0) {
			playSound(0x80);
			_condMask = 0x80;
		} else {
			playSound(0x78);
			_condMask = 0x10;
		}
	} else if (num == 19) {
		if (_lastLevelNum > 1) {
			playSound(0x70);
			++_levelNum;
			_checkpointNum = 0;
			if (_levelNum >= _lastLevelNum) {
				_levelNum = 0;
			}
			memcpy(_paletteBuffer + 192 * 3, _levelsBitmapsData + _levelsBitmaps[_levelNum].palette, 64 * 3);
			g_system->setPalette(_paletteBuffer, 256, 6);
			_condMask = 0x20;
		}
	} else if (num == 20) {
		if (_lastLevelNum > 1) {
			playSound(0x70);
			--_levelNum;
			_checkpointNum = 0;
			if (_levelNum < 0) {
				_levelNum = _lastLevelNum - 1;
			}
			memcpy(_paletteBuffer + 192 * 3, _levelsBitmapsData + _levelsBitmaps[_levelNum].palette, 64 * 3);
			g_system->setPalette(_paletteBuffer, 256, 6);
			_condMask = 0x40;
		}
	}
	drawLevelScreen();
}

void Menu::handleLoadCheckpoint(int num) {
	const uint8_t *data = &_optionData[num * 8];
	num = data[5];
	if (num == 11) {
		if (_loadCheckpointButtonState != 0) {
			playSound(0x70);
			_loadCheckpointButtonState = 0;
		}
	} else if (num == 12) {
		if (_loadCheckpointButtonState == 0) {
			playSound(0x70);
			_loadCheckpointButtonState = 1;
		}
	} else if (num == 13) {
		if (_loadCheckpointButtonState != 0) {
			playSound(0x80);
			_condMask = 0x80;
		} else {
			playSound(0x78);
			_loadCheckpointButtonState = 2;
			_condMask = 0x08;
			if (_levelNum == 7 && _checkpointNum == 11) {
				_config->players[_config->currentPlayer].levelNum = kLvl_dark;
				_config->players[_config->currentPlayer].checkpointNum = 0;
			} else {
				_config->players[_config->currentPlayer].levelNum = _levelNum;
				_config->players[_config->currentPlayer].checkpointNum = _checkpointNum;
			}
			debug(kDebug_MENU, "Restart game at level %d checkpoint %d", _levelNum, _checkpointNum);
			return;
		}
	} else if (num == 14) {
		if (_lastLevelCheckpointNum[_levelNum] > 2 || _loadCheckpointButtonState == 0) {
			playSound(0x70);
			++_checkpointNum;
			if (_checkpointNum >= _lastLevelCheckpointNum[_levelNum]) {
				_checkpointNum = 0;
			}
		}
	} else if (num == 15) {
		if (_lastLevelCheckpointNum[_levelNum] > 2 || _loadCheckpointButtonState == 1) {
			playSound(0x70);
			--_checkpointNum;
			if (_checkpointNum < 0) {
				_checkpointNum = _lastLevelCheckpointNum[_levelNum] - 1;
			}
		}
	}
	drawCheckpointScreen();
}

void Menu::handleLoadCutscene(int num) {
	const uint8_t *data = &_optionData[num * 8];
	num = data[5];
	if (num == 6) {
		if (_loadCutsceneButtonState != 0) {
			playSound(0x70);
			_loadCutsceneButtonState = 0;
		}
	} else if (num == 7) {
		if (_loadCutsceneButtonState == 0) {
			playSound(0x70);
			_loadCutsceneButtonState = 1;
		}
	} else if (num == 8) {
		if (_loadCutsceneButtonState != 0) {
			playSound(0x80);
			_condMask = 0x80;
		} else {
			playSound(0x78);
			_loadCutsceneButtonState = 2;
			const int num = _cutscenesBitmaps[_cutsceneIndexes[_cutsceneNum]].data;
			_paf->play(num);
			if (num == kPafAnimation_end) {
				_paf->play(kPafAnimation_cinema);
			}
			playSound(0x98);
			playSound(0xA0);
			_loadCutsceneButtonState = 0;
		}
	} else if (num == 9) {
		if (_cutsceneIndexesCount > 2 || _loadCutsceneButtonState == 0) {
			playSound(0x70);
			++_cutsceneNum;
			if (_cutsceneNum >= _cutsceneIndexesCount) {
				_cutsceneNum = 0;
			}
		}
	} else if (num == 10) {
		if (_cutsceneIndexesCount > 2 || _loadCutsceneButtonState == 1) {
			playSound(0x70);
			--_cutsceneNum;
			if (_cutsceneNum < 0) {
				_cutsceneNum = _cutsceneIndexesCount - 1;
			}
		}
	}
	drawCutsceneScreen();
}

static bool matchInput(uint8_t menu, uint8_t type, uint8_t mask, const PlayerInput &inp, uint8_t optionMask) {
	if (type != 0) {
		if (menu == kMenu_Settings) {
			// not implemented yet
			return false;
		}
		if ((mask & 1) != 0 && inp.keyReleased(SYS_INP_RUN)) {
			return true;
		}
		if ((mask & 2) != 0 && inp.keyReleased(SYS_INP_JUMP)) {
			return true;
		}
		if ((mask & 4) != 0 && inp.keyReleased(SYS_INP_SHOOT)) {
			return true;
		}
		if ((mask & optionMask) != 0) {
			return true;
		}
	} else {
		if ((mask & 1) != 0 && inp.keyReleased(SYS_INP_UP)) {
			return true;
		}
		if ((mask & 2) != 0 && inp.keyReleased(SYS_INP_RIGHT)) {
			return true;
		}
		if ((mask & 4) != 0 && inp.keyReleased(SYS_INP_DOWN)) {
			return true;
		}
		if ((mask & 8) != 0 && inp.keyReleased(SYS_INP_LEFT)) {
			return true;
		}
	}
	return false;
}

void Menu::handleOptions() {
	_lastLevelNum = _config->players[_config->currentPlayer].currentLevel + 1;
	if (_lastLevelNum >= _res->_datHdr.levelsCount) {
		_lastLevelNum = _res->_datHdr.levelsCount;
	}
	_cutsceneIndexesCount = 0;
	const uint32_t playedCutscenesMask = _config->players[_config->currentPlayer].cutscenesMask;
	for (int i = 0; i < _res->_datHdr.cutscenesCount; ++i) {
		if ((playedCutscenesMask & (1 << i)) != 0) {
			_cutsceneIndexes[_cutsceneIndexesCount] = i;
			++_cutsceneIndexesCount;
		}
	}
	if (_cutsceneIndexesCount > _res->_datHdr.cutscenesCount) {
		_cutsceneIndexesCount = _res->_datHdr.cutscenesCount;
	}
	_optionNum = kMenu_Settings;
	changeToOption(0);
	_condMask = 0;
	while (!g_system->inp.quit) {
		g_system->processEvents();
		if (g_system->inp.keyPressed(SYS_INP_ESC)) {
			_optionNum = -1;
			break;
		}
		// get transition from inputs and menu return code (_condMask)
		int num = -1;
		for (int i = 0; i < _res->_datHdr.menusCount; ++i) {
			const uint8_t *data = _optionData + i * 8;
			if (data[0] == _optionNum && matchInput(data[0], data[1] & 1, data[2], g_system->inp, _condMask)) {
				num = i;
				break;
			}
		}
		_condMask = 0;
		if (num == -1) {
			g_system->sleep(15);
			continue;
		}
		const uint8_t *data = &_optionData[num * 8];
		_optionNum = data[3];
		debug(kDebug_MENU, "handleOptions option %d code %d", _optionNum, data[4]);
		switch (data[4]) {
		case 0:
			handleSettingsScreen(num);
			break;
		case 6:
			playSound(0x70);
			changeToOption(num);
			break;
		case 7:
			playSound(0x78);
			changeToOption(num);
			break;
		case 8:
			changeToOption(num);
			break;
		case 9:
			handleLoadCutscene(num);
			break;
		case 10:
			handleLoadLevel(num);
			break;
		case 11:
			break;
		case 12:
			setLevelCheckpoint(_config->currentPlayer);
			handleLoadCheckpoint(num);
			break;
		default:
			warning("Unhandled option %d %d", _optionNum, data[4]);
			break;
		}
		if (_optionNum == kMenu_Quit + 1 || _optionNum == kMenu_NewGame + 1 || _optionNum == kMenu_CurrentGame + 1 || _optionNum == kMenu_ResumeGame) {
			// 'setup.cfg' is saved when exiting the main loop
			break;
		}
	}
}
