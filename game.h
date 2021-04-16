/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef GAME_H__
#define GAME_H__

#include "intern.h"
#include "defs.h"
#include "fileio.h"
#include "fs.h"
#include "mixer.h"
#include "random.h"
#include "resource.h"

struct Game;
struct Level;
struct PafPlayer;
struct Video;

struct CheckpointData {
	int16_t xPos;
	int16_t yPos;
	uint16_t flags2;
	uint16_t anim;
	uint8_t screenNum;
	uint8_t spriteNum;
};

enum {
	kObjectDataTypeAndy,
	kObjectDataTypeAnimBackgroundData,
	kObjectDataTypeShoot,
	kObjectDataTypeLvlBackgroundSound,
	kObjectDataTypeMonster1,
	kObjectDataTypeMonster2
};

enum {
	kCheatSpectreFireballNoHit = 1 << 0,
	kCheatOneHitPlasmaCannon = 1 << 1,
	kCheatOneHitSpecialPowers = 1 << 2,
	kCheatWalkOnLava = 1 << 3,
	kCheatGateNoCrush = 1 << 4,
	kCheatLavaNoHit = 1 << 5,
	kCheatRockShadowNoHit = 1 << 6
};

struct Game {
	enum {
		kMaxTasks = 128,
		kMaxVars = 40,
		kMaxLocals = 8,
		kMaxAndyShoots = 8,
		kMaxShootLvlObjectData = 32,
		kMaxSssObjects = 32,
		kMaxMonsterObjects1 = 32,
		kMaxMonsterObjects2 = 64,
		kMaxBackgroundAnims = 64,
		kMaxSprites = 128,
		kMaxLvlObjects = 160,
		kMaxBoundingBoxes = 64,

		kDefaultSoundPanning = 64,
		kDefaultSoundVolume = 128
	};

	static const uint8_t _specialPowersDxDyTable[];
	static const uint8_t _pointDstIndexTable[];
	static const uint8_t _pointRandomizeShiftTable[];
	static const uint8_t _pointSrcIndex2Table[];
	static const uint8_t _pointSrcIndex1Table[];
	static const uint8_t _actionDirectionKeyMaskTable[];
	static const uint8_t _pwr1_screenTransformData[];
	static const uint8_t _pwr2_screenTransformData[];
	static const uint8_t _pwr1_screenTransformLut[];
	static const uint8_t _lava_screenTransformLut[];
	static const uint8_t _pwr2_screenTransformLut[];
	static const uint8_t _pwr1_spritesData[];
	static const uint8_t _isld_spritesData[];
	static const uint8_t _lava_spritesData[];
	static const uint8_t _lar1_spritesData[];
	static const int16_t *_lar_screenMaskOffsets[];
	static uint8_t _lar1_maskData[];

	FileSystem _fs;
	Level *_level;
	Mixer _mix;
	PafPlayer *_paf;
	Random _rnd;
	Resource *_res;
	Video *_video;
	uint32_t _cheats;
	int _frameMs;
	int _difficulty;

	SetupConfig _setupConfig;
	bool _playDemo;
	bool _resumeGame;

	LvlObject *_screenLvlObjectsList[kMaxScreens]; // LvlObject linked list for each screen
	LvlObject *_andyObject;
	LvlObject *_plasmaExplosionObject;
	LvlObject *_plasmaCannonObject;
	LvlObject *_specialAnimLvlObject;
	LvlObject *_currentSoundLvlObject;
	int _currentLevel;
	int _currentLevelCheckpoint;
	bool _endLevel;
	Sprite _spritesTable[kMaxSprites];
	Sprite *_spritesNextPtr; // pointer to the next free entry
	Sprite *_typeSpritesList[kMaxSpriteTypes];
	uint8_t _directionKeyMask;
	uint8_t _actionKeyMask;
	uint8_t _currentRightScreen;
	uint8_t _currentLeftScreen;
	uint8_t _currentScreen;
	int8_t _levelRestartCounter;
	bool _fadePalette;
	bool _hideAndyObjectFlag;
	ShootLvlObjectData _shootLvlObjectDataTable[kMaxShootLvlObjectData];
	ShootLvlObjectData *_shootLvlObjectDataNextPtr; // pointer to the next free entry
	LvlObject *_lvlObjectsList0;
	LvlObject *_lvlObjectsList1;
	LvlObject *_lvlObjectsList2;
	LvlObject *_lvlObjectsList3;
	uint8_t _screenPosTable[5][24 * 32];
	uint8_t _screenTempMaskBuffer[24 * 32];
	uint8_t _screenMaskBuffer[(16 * 6) * 24 * 32]; // level screens mask : 16 horizontal screens x 6 vertical screens
	int _mstAndyCurrentScreenNum;
	uint8_t _shakeScreenDuration;
	const uint8_t *_shakeScreenTable;
	uint8_t _plasmaCannonDirection;
	uint8_t _plasmaCannonPrevDirection;
	uint8_t _plasmaCannonPointsSetupCounter;
	uint8_t _plasmaCannonLastIndex1;
	bool _plasmaCannonExplodeFlag;
	uint8_t _plasmaCannonPointsMask;
	uint8_t _plasmaCannonFirstIndex;
	uint8_t _plasmaCannonLastIndex2;
	uint8_t _plasmaCannonFlags;
	uint8_t _actionDirectionKeyMaskCounter;
	bool _fallingAndyFlag;
	uint8_t _fallingAndyCounter;
	uint8_t _actionDirectionKeyMaskIndex;
	uint8_t _andyActionKeyMaskAnd, _andyActionKeyMaskOr;
	uint8_t _andyDirectionKeyMaskAnd, _andyDirectionKeyMaskOr;
	int32_t _plasmaCannonPosX[129];
	int32_t _plasmaCannonPosY[129];
	int32_t _plasmaCannonXPointsTable1[129];
	int32_t _plasmaCannonYPointsTable1[129];
	int32_t _plasmaCannonXPointsTable2[127];
	int32_t _plasmaCannonYPointsTable2[127];
	ScreenMask _shadowScreenMasksTable[8];

	uint16_t _mstCurrentAnim;
	uint16_t _specialAnimMask;
	int16_t _mstOriginPosX;
	int16_t _mstOriginPosY;
	bool _specialAnimFlag;
	AndyShootData _andyShootsTable[kMaxAndyShoots];
	int _andyShootsCount;
	uint8_t _mstOp68_type, _mstOp68_flags1, _mstOp67_type, _mstOp67_flags1;
	int _mstOp67_x1, _mstOp67_x2, _mstOp67_y1, _mstOp67_y2;
	int8_t _mstOp67_screenNum;
	uint16_t _mstOp68_flags2;
	int _mstOp68_x1, _mstOp68_x2, _mstOp68_y1, _mstOp68_y2;
	int8_t _mstOp68_screenNum;
	uint32_t _mstLevelGatesMask;
	int _runTaskOpcodesCount;
	int32_t _mstVars[kMaxVars];
	uint32_t _mstFlags;
	int _clipBoxOffsetX, _clipBoxOffsetY;
	Task *_currentTask;
	int _mstOp54Counter;
	int _mstOp56Counter;
	bool _mstDisabled;
	LvlObject _declaredLvlObjectsList[kMaxLvlObjects];
	LvlObject *_declaredLvlObjectsNextPtr; // pointer to the next free entry
	int _declaredLvlObjectsListCount;
	AndyLvlObjectData _andyObjectScreenData;
	AnimBackgroundData _animBackgroundDataTable[kMaxBackgroundAnims];
	int _animBackgroundDataCount;
	uint8_t _andyActionKeysFlags;
	int _executeMstLogicCounter;
	int _executeMstLogicPrevCounter;
	Task _tasksTable[kMaxTasks];
	Task *_tasksList;
	Task *_monsterObjects1TasksList;
	Task *_monsterObjects2TasksList;
	int _mstAndyLevelPrevPosX;
	int _mstAndyLevelPrevPosY;
	int _mstAndyLevelPosX;
	int _mstAndyLevelPosY;
	int _mstAndyScreenPosX;
	int _mstAndyScreenPosY;
	int _mstAndyRectNum;
	int _m43Num1;
	int _m43Num2;
	int _m43Num3;
	int _xMstPos1, _yMstPos1;
	int _xMstPos2, _yMstPos2; // xMstDist1, yMstDist1
	int _xMstPos3, _yMstPos3;
	int _mstActionNum;
	uint32_t _mstAndyVarMask;
	int _mstChasingMonstersCount;
	int _mstPosXmin, _mstPosXmax;
	int _mstPosYmin, _mstPosYmax;
	int _mstTemp_x1, _mstTemp_x2, _mstTemp_y1, _mstTemp_y2;
	MonsterObject1 _monsterObjects1Table[kMaxMonsterObjects1];
	MonsterObject2 _monsterObjects2Table[kMaxMonsterObjects2];
	int _mstTickDelay;
	uint8_t _mstCurrentActionKeyMask;
	int _mstCurrentPosX, _mstCurrentPosY;
	int _mstBoundingBoxesCount;
	MstBoundingBox _mstBoundingBoxesTable[kMaxBoundingBoxes];
	Task *_mstCurrentTask;
	MstCollision _mstCollisionTable[2][kMaxMonsterObjects1]; // 0:facingRight, 1:facingLeft
	int _wormHoleSpritesCount;
	WormHoleSprite _wormHoleSpritesTable[6];

	Game(const char *dataPath, const char *savePath, uint32_t cheats);
	~Game();

	// 32*24 pitch=512
	static uint32_t screenMaskOffset(int x, int y) {
		return ((y << 6) & ~511) + (x >> 3);
		// return ((y & ~7) << 6) + (x >> 3)
	}

	// 32*24 pitch=32
	static uint32_t screenGridOffset(int x, int y) {
		return ((y & ~7) << 2) + (x >> 3);
	}

	// benchmark.cpp
	uint32_t benchmarkCpu();

	// game.cpp
	void mainLoop(int level, int checkpoint, bool levelChanged);
	void mixAudio(int16_t *buf, int len);
	void resetShootLvlObjectDataTable();
	void clearShootLvlObjectData(LvlObject *ptr);
	void addShootLvlObject(LvlObject *_edx, LvlObject *ptr);
	void setShakeScreen(int type, int counter);
	void fadeScreenPalette();
	void shakeScreen();
	void transformShadowLayer(int delta);
	void loadTransformLayerData(const uint8_t *data);
	void unloadTransformLayerData();
	void decodeShadowScreenMask(LvlBackgroundData *lvl);
	SssObject *playSound(int num, LvlObject *ptr, int a, int b);
	void removeSound(LvlObject *ptr);
	void setupBackgroundBitmap();
	void addToSpriteList(Sprite *spr);
	void addToSpriteList(LvlObject *ptr);
	int16_t calcScreenMaskDy(int16_t xPos, int16_t yPos, int num);
	void setupScreenPosTable(uint8_t num);
	void setupScreenMask(uint8_t num);
	void resetScreenMask();
	void setScreenMaskRectHelper(int x1, int y1, int x2, int y2, int screenNum);
	void setScreenMaskRect(int x1, int y1, int x2, int y2, int pos);
	void updateScreenMaskBuffer(int x, int y, int type);
	void setupLvlObjectBitmap(LvlObject *ptr);
	void randomizeInterpolatePoints(int32_t *pts, int count);
	int fixPlasmaCannonPointsScreenMask(int num);
	void setupPlasmaCannonPointsHelper();
	void destroyLvlObjectPlasmaExplosion(LvlObject *o);
	void shuffleArray(uint8_t *p, int count);
	void destroyLvlObject(LvlObject *o);
	void setupPlasmaCannonPoints(LvlObject *ptr);
	int testPlasmaCannonPointsDirection(int x1, int y1, int x2, int y2);
	void preloadLevelScreenData(uint8_t num, uint8_t prev);
	void setLvlObjectPosRelativeToObject(LvlObject *ptr1, int num1, LvlObject *ptr2, int num2);
	void setLvlObjectPosRelativeToPoint(LvlObject *ptr, int num, int x, int y);
	void clearLvlObjectsList0();
	void clearLvlObjectsList1();
	void clearLvlObjectsList2();
	void clearLvlObjectsList3();
	LvlObject *addLvlObjectToList0(int num);
	LvlObject *addLvlObjectToList1(int type, int num);
	LvlObject *addLvlObjectToList2(int num);
	LvlObject *addLvlObjectToList3(int num);
	void removeLvlObject(LvlObject *ptr);
	void removeLvlObject2(LvlObject *o);
	void setAndySprite(int num);
	void setupAndyLvlObject();
	void setupScreenLvlObjects(int num);
	void resetDisplay();
	void setupScreen(uint8_t num);
	void resetScreen();
	void restartLevel();
	void playAndyFallingCutscene(int type);
	int8_t updateLvlObjectScreen(LvlObject *ptr);
	void setAndyAnimationForArea(BoundingBox *box, int dx);
	void setAndyLvlObjectPlasmaCannonKeyMask();
	int setAndySpecialAnimation(uint8_t mask);
	int clipBoundingBox(BoundingBox *coords, BoundingBox *box);
	int updateBoundingBoxClippingOffset(BoundingBox *_ecx, BoundingBox *_ebp, const uint8_t *coords, int direction);
	int clipLvlObjectsBoundingBoxHelper(LvlObject *o1, BoundingBox *box1, LvlObject *o2, BoundingBox *box2);
	int clipLvlObjectsBoundingBox(LvlObject *o, LvlObject *ptr, int count);
	int clipLvlObjectsSmall(LvlObject *o, LvlObject *ptr, int count);
	int restoreAndyCollidesLava();
	int updateAndyLvlObject();
	void drawPlasmaCannon();
	void updateBackgroundPsx(int num);
	void drawScreen();
	void updateLvlObjectList(LvlObject **list);
	void updateLvlObjectLists();
	LvlObject *updateAnimatedLvlObjectType0(LvlObject *ptr);
	LvlObject *updateAnimatedLvlObjectType1(LvlObject *ptr);
	LvlObject *updateAnimatedLvlObjectType2(LvlObject *ptr);
	LvlObject *updateAnimatedLvlObject(LvlObject *o);
	void updateAnimatedLvlObjectsLeftRightCurrentScreens();
	void updatePlasmaCannonExplosionLvlObject(LvlObject *ptr);
	void resetPlasmaCannonState();
	void updateAndyMonsterObjects();
	void updateInput();
	void levelMainLoop();
	Level *createLevel();
	void callLevel_postScreenUpdate(int num);
	void callLevel_preScreenUpdate(int num);
	void callLevel_initialize();
	void callLevel_tick();
	void callLevel_terminate();
	void displayLoadingScreen();
	int displayHintScreen(int num, int pause);
	void removeLvlObjectFromList(LvlObject **list, LvlObject *ptr);
	void *getLvlObjectDataPtr(LvlObject *o, int type) const;
	void lvlObjectType0Init(LvlObject *ptr);
	void lvlObjectType1Init(LvlObject *ptr);
	void lvlObjectTypeInit(LvlObject *ptr);
	void lvlObjectType0CallbackHelper1();
	int calcScreenMaskDx(int x, int y, int num);
	void lvlObjectType0CallbackBreathBubbles(LvlObject *ptr);
	void setupSpecialPowers(LvlObject *ptr);
	int lvlObjectType0Callback(LvlObject *ptr);
	int lvlObjectType1Callback(LvlObject *ptr);
	int lvlObjectType7Callback(LvlObject *o);
	int lvlObjectType8Callback(LvlObject *o);
	int lvlObjectList3Callback(LvlObject *o);
	void lvlObjectSpecialPowersCallbackHelper1(LvlObject *o);
	uint8_t lvlObjectCallbackCollideScreen(LvlObject *o);
	int lvlObjectSpecialPowersCallback(LvlObject *o);
	void lvlObjectTypeCallback(LvlObject *o);
	LvlObject *addLvlObject(int type, int x, int y, int screen, int num, int o_anim, int o_flags1, int o_flags2, int actionKeyMask, int directionKeyMask);
	int setLvlObjectPosInScreenGrid(LvlObject *o, int pos);
	LvlObject *declareLvlObject(uint8_t type, uint8_t num);
	void clearDeclaredLvlObjectsList();
	void initLvlObjects();
	void setLvlObjectSprite(LvlObject *ptr, uint8_t _dl, uint8_t num);
	LvlObject *findLvlObject(uint8_t type, uint8_t spriteNum, int screenNum);
	LvlObject *findLvlObject2(uint8_t type, uint8_t dataNum, int screenNum);
	LvlObject *findLvlObjectType2(int spriteNum, int screenNum);
	LvlObject *findLvlObjectBoundingBox(BoundingBox *box);
	void setLavaAndyAnimation(int yPos);
	void updateGatesLar(LvlObject *o, uint8_t *p, int num);
	void updateSwitchesLar(int count, uint8_t *data, BoundingBox *r, uint8_t *gatesData);
	void updateSwitchesLar_checkSpectre(int num, uint8_t *p1, BoundingBox *r, uint8_t *gatesData);
	int updateSwitchesLar_checkAndy(int num, uint8_t *p1, BoundingBox *b, BoundingBox *r, uint8_t *gatesData);
	int updateSwitchesLar_toggle(bool flag, uint8_t dataNum, int screenNum, int switchNum, int anim, const BoundingBox *switchBoundingBox);
	void dumpSwitchesLar(int switchesCount, const uint8_t *switchesData, const BoundingBox *switchesBoundingBox, int gatesCount, const uint8_t *gatesData);
	void updateScreenMaskLar(uint8_t *p, uint8_t flag);
	void updateGateMaskLar(int num);
	int clipAndyLvlObjectLar(BoundingBox *a, BoundingBox *b, bool flag);
	void resetWormHoleSprites();
	void updateWormHoleSprites();
	void loadSetupCfg(bool resume);
	void saveSetupCfg();
	void captureScreenshot();

	// level1_rock.cpp
	int objectUpdate_rock_case0(LvlObject *o);
	void objectUpdate_rockShadow(LvlObject *ptr, uint8_t *p);
	bool plasmaCannonHit(LvlObject *ptr);
	int objectUpdate_rock_case1(LvlObject *o);
	int objectUpdate_rock_case2(LvlObject *o);
	int objectUpdate_rock_case3(LvlObject *o);
	int objectUpdate_rock_case4(LvlObject *o);

	// monsters.cpp
	void mstMonster1ResetData(MonsterObject1 *m);
	void mstMonster1ResetWalkPath(MonsterObject1 *m);
	bool mstUpdateInRange(MstMonsterAction *m48);
	bool addChasingMonster(MstMonsterAction *m48, uint8_t flag);
	void mstMonster1ClearChasingMonster(MonsterObject1 *m);
	void mstTaskSetMonster1BehaviorState(Task *t, MonsterObject1 *m, int num);
	int mstTaskStopMonsterObject1(Task *t);
	void mstMonster1SetScreenPosition(MonsterObject1 *m);
	bool mstMonster1SetWalkingBounds(MonsterObject1 *m);
	bool mstMonster1UpdateWalkPath(MonsterObject1 *m);
	void mstMonster2InitFirefly(MonsterObject2 *m);
	void mstMonster2ResetData(MonsterObject2 *m);
	uint32_t mstMonster1GetNextWalkCode(MonsterObject1 *m);
	int mstTaskSetNextWalkCode(Task *t);

	void mstBoundingBoxClear(MonsterObject1 *m, int dir);
	int mstBoundingBoxCollides1(int num, int x1, int y1, int x2, int y2) const;
	int mstBoundingBoxUpdate(int num, int monster1Index, int x1, int y1, int x2, int y2);
	int mstBoundingBoxCollides2(int monster1Index, int x1, int y1, int x2, int y2) const;

	void mstTaskSetMonster2ScreenPosition(Task *t);
	int getMstDistance(int y, const AndyShootData *p) const;
	void mstTaskUpdateScreenPosition(Task *t);
	void shuffleMstMonsterActionIndex(MstMonsterActionIndex *p);

	void initMstCode();
	void resetMstCode();
	void startMstCode();
	void executeMstCode();
	void mstWalkPathUpdateIndex(MstWalkPath *walkPath, int index);
	int mstWalkPathUpdateWalkNode(MstWalkPath *walkPath, MstWalkNode *walkNode, int num, int index);
	void executeMstCodeHelper1();
	void mstUpdateMonster1ObjectsPosition();
	void mstLvlObjectSetActionDirection(LvlObject *o, const uint8_t *ptr, uint8_t mask1, uint8_t mask2);
	void mstMonster1UpdateGoalPosition(MonsterObject1 *m);
	void mstMonster1MoveTowardsGoal1(MonsterObject1 *m);
	bool mstMonster1TestGoalDirection(MonsterObject1 *m);
	void mstMonster1MoveTowardsGoal2(MonsterObject1 *m);
	int mstTaskStopMonster1(Task *t, MonsterObject1 *m);
	int mstTaskUpdatePositionActionDirection(Task *t, MonsterObject1 *m);
	int mstMonster1FindWalkPathRect(MonsterObject1 *m, MstWalkPath *walkPath, int x, int y);
	bool mstTestActionDirection(MonsterObject1 *m, int num);
	bool lvlObjectCollidesAndy1(LvlObject *o, int type) const;
	bool lvlObjectCollidesAndy2(LvlObject *o, int type) const;
	bool lvlObjectCollidesAndy4(LvlObject *o, int type) const;
	bool lvlObjectCollidesAndy3(LvlObject *o, int type) const;
	bool mstCollidesByFlags(MonsterObject1 *m, uint32_t flags);
	bool mstMonster1Collide(MonsterObject1 *m, const uint8_t *p);
	int mstUpdateTaskMonsterObject1(Task *t);
	int mstUpdateTaskMonsterObject2(Task *t);
	void mstUpdateRefPos();
	void mstUpdateMonstersRect();
	void mstRemoveMonsterObject2(Task *t, Task **tasksList);
	void mstTaskAttack(Task *t, uint32_t codeData, uint8_t flags);
	void mstRemoveMonsterObject1(Task *t, Task **tasksList);
	void mstOp26_removeMstTaskScreen(Task **tasksList, int screenNum);
	void mstOp27_removeMstTaskScreenType(Task **tasksList, int screenNum, int type);
	int mstOp49_setMovingBounds(int a, int b, int c, int d, int screen, Task *t, int num);
	void mstOp52();
	bool mstHasMonsterInRange(const MstMonsterAction *m, uint8_t flag);
	void mstOp53(MstMonsterAction *m);
	void mstOp54();
	int mstOp56_specialAction(Task *t, int code, int num);
	void mstOp57_addWormHoleSprite(int x, int y, int screenNum);
	void mstOp58_addLvlObject(Task *t, int num);
	void mstOp59_addShootSpecialPowers(int x, int y, int screenNum, int state, uint16_t flags);
	void mstOp59_addShootFireball(int x, int y, int screenNum, int type, int state, uint16_t flags);
	void mstTaskResetMonster1WalkPath(Task *t);
	bool mstSetCurrentPos(MonsterObject1 *m, int x, int y);
	void mstMonster1SetGoalHorizontal(MonsterObject1 *m);
	void mstResetCollisionTable();
	void mstTaskRestart(Task *t);
	bool mstMonster1CheckLevelBounds(MonsterObject1 *m, int x, int y, uint8_t dir);
	int mstTaskResetMonster1Direction(Task *t);
	int mstTaskInitMonster1Type1(Task *t);
	int mstTaskInitMonster1Type2(Task *t, int flag);
	void mstOp67_addMonster(Task *t, int y1, int y2, int x1, int x2, int screen, int type, int o_flags1, int o_flags2, int arg1C, int arg20, int arg24);
	void mstOp68_addMonsterGroup(Task *t, const uint8_t *p, int a, int b, int c, int d);
	int mstTaskSetActionDirection(Task *t, int num, int value);

	// Task list
	Task *findFreeTask();
	Task *createTask(const uint8_t *codeData);
	void updateTask(Task *t, int num, const uint8_t *codeData);
	void resetTask(Task *t, const uint8_t *codeData);
	void removeTask(Task **tasksList, Task *t);
	void appendTask(Task **tasksList, Task *t);

	// Task storage
	int getTaskVar(Task *t, int index, int type) const;
	void setTaskVar(Task *t, int index, int type, int value);
	int getTaskAndyVar(int index, Task *t) const;
	int getTaskOtherVar(int index, Task *t) const;
	int getTaskFlag(Task *t, int index, int type) const;

	// Task.run functions
	int mstTask_main(Task *t);
	int mstTask_wait1(Task *t);
	int mstTask_wait2(Task *t);
	int mstTask_wait3(Task *t);
	int mstTask_idle(Task *t);
	int mstTask_mstOp231(Task *t);
	int mstTask_mstOp232(Task *t);
	int mstTask_mstOp233(Task *t);
	int mstTask_mstOp234(Task *t);
	int mstTask_monsterWait1(Task *t);
	int mstTask_monsterWait2(Task *t);
	int mstTask_monsterWait3(Task *t);
	int mstTask_monsterWait4(Task *t);
	int mstTask_monsterWait5(Task *t);
	int mstTask_monsterWait6(Task *t);
	int mstTask_monsterWait7(Task *t);
	int mstTask_monsterWait8(Task *t);
	int mstTask_monsterWait9(Task *t);
	int mstTask_monsterWait10(Task *t);
	int mstTask_monsterWait11(Task *t);

	// sound.cpp
	bool _sssDisabled;
	int16_t _snd_buffer[4096];
	int _snd_bufferOffset, _snd_bufferSize;
	bool _snd_muted;
	int _snd_masterPanning;
	int _snd_masterVolume;
	SssObject _sssObjectsTable[kMaxSssObjects];
	bool _sssObjectsChanged;
	int _sssObjectsCount;
	SssObject *_sssObjectsList1; // playing
	SssObject *_sssObjectsList2; // paused/idle
	SssObject *_lowPrioritySssObject;
	bool _sssUpdatedObjectsTable[kMaxSssObjects];
	int _playingSssObjectsMax;
	int _playingSssObjectsCount;

	void muteSound();
	void unmuteSound();
	void resetSound();
	int getSoundPosition(const SssObject *so);
	void setSoundPanning(SssObject *so, int panning);
	SssObject *findLowPrioritySoundObject() const;
	void removeSoundObjectFromList(SssObject *so);
	void updateSoundObject(SssObject *so);
	void sssOp4_removeSounds(uint32_t flags);
	void sssOp12_removeSounds2(int num, uint8_t source, uint8_t sampleIndex);
	void sssOp16_resumeSound(SssObject *so);
	void sssOp17_pauseSound(SssObject *so);
	const uint8_t *executeSssCode(SssObject *so, const uint8_t *code, bool tempSssObject = false);
	SssObject *addSoundObject(SssPcm *pcm, int priority, uint32_t flags_a, uint32_t flags_b);
	void prependSoundObjectToList(SssObject *so);
	void updateSssGroup2(uint32_t flags);
	SssObject *createSoundObject(int bankIndex, int sampleIndex, uint32_t flags);
	SssObject *startSoundObject(int bankIndex, int sampleIndex, uint32_t flags);
	SssObject *playSoundObject(SssInfo *s, int source, int b);
	void clearSoundObjects();
	void setLowPrioritySoundObject(SssObject *so);
	int getSoundObjectPanning(SssObject *so) const;
	void setSoundObjectPanning(SssObject *so);
	void expireSoundObjects(uint32_t flags);
	void mixSoundObjects17640(bool flag);
	void queueSoundObjectsPcmStride();

	// andy.cpp
	AndyMoveData _andyMoveData;
	int32_t _andyPosX;
	int32_t _andyPosY;
	uint8_t _andyMoveMask;
	bool _andyUpdatePositionFlag;
	const Point16_t *_andyLevelData0x288PosTablePtr;
	uint8_t _andyMoveState[32];
	int _andyMaskBufferPos1;
	int _andyMaskBufferPos2;
	int _andyMaskBufferPos4;
	int _andyMaskBufferPos5;
	int _andyMaskBufferPos3;
	int _andyMaskBufferPos0;
	int _andyMaskBufferPos7;
	int _andyMaskBufferPos6;

	int moveAndyObjectOp1(int op);
	int moveAndyObjectOp2(int op);
	int moveAndyObjectOp3(int op);
	int moveAndyObjectOp4(int op);
	void setupAndyObjectMoveData(LvlObject *ptr);
	void setupAndyObjectMoveState();
	void updateAndyObject(LvlObject *ptr);
};

#endif // GAME_H__
