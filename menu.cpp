
#include "menu.h"
#include "lzw.h"
#include "paf.h"
#include "resource.h"
#include "system.h"
#include "util.h"
#include "video.h"

Menu::Menu(PafPlayer *paf, Resource *res, System *system, Video *video)
	: _paf(paf), _res(res), _system(system), _video(video) {
	_titleSprites = 0;
	_playerSprites = 0;
	_titleBitmap = 0;
	_playerBitmap = 0;
	_currentOption = 0;
}

void Menu::loadData() {
	_res->loadDatMenuBuffers();

	if (_res->_datHdr.version == 10) {

		const uint8_t *ptr = _res->_menuBuffer1;
		uint32_t ptrOffset = 0;

		_titleSprites = (DatSpritesGroup *)(ptr + ptrOffset);
		_titleSprites->size = le32toh(_titleSprites->size);
		ptrOffset += sizeof(DatSpritesGroup) + _titleSprites->size;

		_playerSprites = (DatSpritesGroup *)(ptr + ptrOffset);
		_playerSprites->size = le32toh(_playerSprites->size);
		ptrOffset += sizeof(DatSpritesGroup) + _playerSprites->size;

		_titleBitmap = (DatBitmap *)(ptr + ptrOffset);
		_titleBitmap->size = le32toh(_titleBitmap->size);
		ptrOffset += sizeof(DatBitmap) + _titleBitmap->size;

		_playerBitmap = (DatBitmap *)(ptr + ptrOffset);
		_playerBitmap->size = le32toh(_playerBitmap->size);
		ptrOffset += sizeof(DatBitmap) + _playerBitmap->size;
	} else {
		warning("Unhandled .dat version %d", _res->_datHdr.version);
	}
}

void Menu::drawSprite(const DatSpritesGroup *spriteGroup, uint32_t num) {
	const uint8_t *ptr = (const uint8_t *)&spriteGroup[1];
	for (uint32_t i = 0; i < spriteGroup->count; ++i) {
		const uint16_t size = READ_LE_UINT16(ptr + 2);
		if (num == i) {
			_video->decodeSPR(ptr + 8, _video->_frontLayer, ptr[0], ptr[1], 0, READ_LE_UINT16(ptr + 4), READ_LE_UINT16(ptr + 6));
			break;
		}
		ptr += size + 2;
	}
}

void Menu::drawBitmap(const DatBitmap *bitmap) {
	const uint8_t *data = (const uint8_t *)&bitmap[1];
	const uint32_t uncompressedSize = decodeLZW(data, _video->_frontLayer);
	assert(uncompressedSize == Video::W * Video::H);
	const uint8_t *palette = data + bitmap->size;
	_system->setPalette(palette, 256, 6);
	if (bitmap == _titleBitmap) {
		drawSprite(_titleSprites, _currentOption);
	}
	_system->copyRect(0, 0, Video::W, Video::H, _video->_frontLayer, Video::W);
	_system->updateScreen(false);
}

void Menu::mainLoop() {
	loadData();
	while (!_system->inp.quit) {
		if (_system->inp.keyReleased(SYS_INP_UP)) {
			if (_currentOption > 0) {
				--_currentOption;
			}
		}
		if (_system->inp.keyReleased(SYS_INP_DOWN)) {
			if (_currentOption < 3) {
				++_currentOption;
			}
		}
		drawBitmap(_titleBitmap);
		_system->processEvents();
		_system->sleep(15);
	}
}
