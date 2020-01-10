
#ifndef MENU_H__
#define MENU_H__

#include "intern.h"

struct Game;
struct PafPlayer;
struct Resource;
struct Video;

struct DatSpritesGroup {
	uint32_t unk0; // 0
	uint32_t unk4; // 4
	uint32_t size; // 8 following this header
	uint32_t count; // 12
} PACKED; // sizeof == 16

struct DatBitmap {
	uint32_t size; // 0
	uint32_t unk4; // 4
	// 8 lzw + 768 palette
} PACKED; // sizeof == 8

struct Menu {

	Game *_g;
	PafPlayer *_paf;
	Resource *_res;
	Video *_video;

	DatSpritesGroup *_titleSprites;
	DatSpritesGroup *_playerSprites;
	const uint8_t *_titleBitmapData;
	uint32_t _titleBitmapSize;
	const uint8_t *_playerBitmapData;
	uint32_t _playerBitmapSize;
	const uint8_t *_soundData;
	int _currentOption;

	Menu(Game *g, PafPlayer *paf, Resource *res, Video *video);

	void loadData();

	int getSoundNum(int num) const;
	void playSound(int num);

	void drawSprite(const DatSpritesGroup *spriteGroup, uint32_t num);
	void drawBitmap(const uint8_t *bitmapData, uint32_t bitmapSize, const DatSpritesGroup *spritesGroup);

	void mainLoop();
	void handleTitleScreen();
};

#endif // MENU_H__
