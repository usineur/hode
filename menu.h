
#ifndef MENU_H__
#define MENU_H__

#include "intern.h"

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

	PafPlayer *_paf;
	Resource *_res;
	Video *_video;

	DatSpritesGroup *_titleSprites;
	DatSpritesGroup *_playerSprites;
	DatBitmap *_titleBitmap;
	DatBitmap *_playerBitmap;
	int _currentOption;

	Menu(PafPlayer *paf, Resource *res, Video *video);

	void loadData();

	void drawSprite(const DatSpritesGroup *spriteGroup, uint32_t num);
	void drawBitmap(const DatBitmap *bitmap);

	void mainLoop();
};

#endif // MENU_H__
