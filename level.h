
#ifndef LEVEL_H__
#define LEVEL_H__

struct CheckpointData;
struct Game;
struct LvlObject;
struct Resource;
struct PafPlayer;
struct Video;

struct Level {
	virtual ~Level() {
	}

	void setPointers(Game *g, LvlObject *andyObject, PafPlayer *paf, Resource *r, Video *video) {
		_g = g;
		_andyObject = andyObject;
		_paf = paf;
		_res = r;
		_video = video;
	}

	virtual const CheckpointData *getCheckpointData(int num) const = 0;
	virtual const uint8_t *getScreenRestartData() const = 0;
	virtual void initialize() {}
	virtual void terminate() {}
	virtual void tick() {}
	virtual void preScreenUpdate(int screenNum) = 0;
	virtual void postScreenUpdate(int screenNum) = 0;
	virtual void setupScreenCheckpoint(int screenNum) {}

	Game *_g;
	LvlObject *_andyObject;
	PafPlayer *_paf;
	Resource *_res;
	Video *_video;

	uint8_t _screenCounterTable[kMaxScreens];
	int _checkpoint;
};

Level *Level_rock_create();
Level *Level_fort_create();
Level *Level_pwr1_create();
Level *Level_isld_create();
Level *Level_lava_create();
Level *Level_pwr2_create();
Level *Level_lar1_create();
Level *Level_lar2_create();
Level *Level_dark_create();

#endif
