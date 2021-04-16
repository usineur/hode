/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef RESOURCE_H__
#define RESOURCE_H__

#include "defs.h"
#include "intern.h"

struct DatHdr {
	uint32_t version; // 0x0
	uint32_t bufferSize0; // 0x4
	uint32_t bufferSize1; // 0x8
	uint32_t sssOffset; // 0xC
	int32_t iconsCount; // 0x10
	int32_t menusCount; // 0x14
	int32_t cutscenesCount; // 0x18
	int32_t levelsCount; // 0x1C
	int32_t levelCheckpointsCount[8]; // 0x20..0x3C
	int32_t yesNoQuitImage; // 0x40
	uint32_t soundDataSize; // 0x44
	uint32_t loadingImageSize; // 0x48
	uint32_t hintsImageOffsetTable[46];
	uint32_t hintsImageSizeTable[46];
};

struct LvlHdr {
	uint8_t screensCount;
	uint8_t staticLvlObjectsCount;
	uint8_t otherLvlObjectsCount;
	uint8_t spritesCount;
};

struct LvlScreenVector {
	int32_t u;
	int32_t v;
};

struct LvlScreenState {
	uint8_t s0; // current state
	uint8_t s1; // states count
	uint8_t s2; // lvlObjects initialized
	uint8_t s3; // current mask
};

struct LvlBackgroundData {
	uint8_t backgroundCount;
	uint8_t currentBackgroundId;
	uint8_t maskCount;
	uint8_t currentMaskId;
	uint8_t shadowCount;
	uint8_t currentShadowId;
	uint8_t soundCount;
	uint8_t currentSoundId;
	uint8_t dataUnk3Count;
	uint8_t unk9;
	uint8_t dataUnk45Count;
	uint8_t unkB;
	uint16_t backgroundPaletteId;
	uint16_t backgroundBitmapId;
	uint8_t *backgroundPaletteTable[4];
	uint8_t *backgroundBitmapTable[4];
	uint8_t *dataUnk0Table[4]; // unused
	uint8_t *backgroundMaskTable[4];
	uint8_t *backgroundSoundTable[4];
	uint8_t *backgroundAnimationTable[8];
	LvlObjectData *backgroundLvlObjectDataTable[8];
};

struct MstHdr {
	int version;
	int dataSize;
	int walkBoxDataCount; // 8
	int walkCodeDataCount; // C
	int movingBoundsIndexDataCount; // 10
	int levelCheckpointCodeDataCount; // 14
	int screenAreaDataCount; // 18
	int screenAreaIndexDataCount; // 1C
	int behaviorIndexDataCount; // 20
	int monsterActionIndexDataCount; // 24
	int walkPathDataCount; // 28
	int infoMonster2Count; // 2C
	int behaviorDataCount; // 30
	int attackBoxDataCount; // 34
	int monsterActionDataCount; // 38
	int infoMonster1Count; // 3C
	int movingBoundsDataCount; // 40
	int shootDataCount; // 44
	int shootIndexDataCount; // 48
	int actionDirectionDataCount; // 4C
	int op223DataCount; // 50
	int op226DataCount; // 54
	int op227DataCount; // 58
	int op234DataCount; // 5C
	int op2DataCount; // 60
	int op197DataCount; // 64
	int op211DataCount; // 68
	int op240DataCount; // 6C
	int unk0x70; // mstUnk60DataCount
	int unk0x74; // mstUnk61DataCount
	int op204DataCount; // 78
	int codeSize; // sizeof(uint32_t)
	int screensCount; // 80
};

struct MstPointOffset {
	int32_t xOffset;
	int32_t yOffset;
}; // sizeof == 8

struct MstScreenArea {
	int32_t x1; // 0
	int32_t x2; // 4
	int32_t y1; // 8
	int32_t y2; // 0xC
	uint32_t nextByPos; // 0x10 _mstScreenAreaByPosIndexData
	uint32_t prev; // 0x14 unused
	uint32_t nextByValue; // 0x18 _mstScreenAreaByValueIndexData
	uint8_t unk0x1C; // 0x1C _mstScreenAreaByValueIndexData
	uint8_t unk0x1D; // 0x1D value
	uint16_t unk0x1E; // 0x1E unused
	uint32_t codeData; // 0x20, offset _mstCodeData
}; // sizeof == 36

struct MstWalkBox { // u34
	int32_t right; // 0
	int32_t left; //  4
	int32_t bottom; // 8
	int32_t top; // 0xC
	uint8_t flags[4]; // 0x10
}; // sizeof == 20

struct MstWalkCode { // u35
	uint32_t *codeData; // 0, offset _mstCodeData
	uint32_t codeDataCount; // 4
	uint8_t *indexData; // 8
	uint32_t indexDataCount; // C
}; // sizeof == 16

struct MstMovingBoundsIndex { // u36
	uint32_t indexUnk49; // indexes _mstMovingBoundsData
	uint32_t unk4; // indexes _mstMovingBoundsData.data1
	int32_t unk8;
}; // sizeof == 12

struct MstBehaviorIndex { // u42
	uint32_t *behavior; // 0 indexes _mstBehaviorData
	uint32_t count1; // 4
	uint8_t *data; // 8 lut - indexes .behavior
	uint32_t dataCount; // C
}; // sizeof == 16

struct MstMonsterActionIndex { // u43
	uint32_t *indexUnk48; // indexes _mstMonsterActionData
	uint32_t count1; // 4
	uint8_t *data; // 8 lut - indexes .indexUnk48
	uint32_t dataCount; // C
}; // sizeof == 16

struct MstWalkNode { // u44u1
	int32_t x1; // 0
	int32_t x2; // 4
	int32_t y1; // 8
	int32_t y2; // 0xC
	uint32_t walkBox; // 0x10
	uint32_t walkCodeStage1; // 0x14
	uint32_t walkCodeStage2; // 0x18
	uint32_t movingBoundsIndex1; // 0x1C
	uint32_t movingBoundsIndex2; // 0x20
	uint32_t walkCodeReset[2]; // 0x24
	int32_t coords[4][2]; // 0x2C, 0x34, 0x3C, 0x44
	uint32_t neighborWalkNode[4]; // per direction 0x4C
	uint32_t nextWalkNode; // next 0x5C
	uint8_t *unk60[2]; // 0x60
}; // sizeof == 104

struct MstWalkPath { // u44
	MstWalkNode *data;
	uint32_t *walkNodeData; // indexed by screen number
	uint32_t mask; // 0x8
	uint32_t count; // 0xC
}; // sizeof == 16

struct MstInfoMonster2 { // u45
	uint8_t type; // 0x0
	uint8_t shootMask; // 0x1
	uint16_t anim; // 0x2
	uint32_t codeData; // 4 init
	uint32_t codeData2; // 8 shoot
}; // sizeof == 12

struct MstBehaviorState { // u46u1
	uint32_t indexMonsterInfo; // 0, indexes _mstMonsterInfos
	uint16_t anim; // 4
	uint16_t unk6; // 6
	uint32_t unk8; // 0x8
	uint32_t energy; // 0xC
	uint32_t unk10; // 0x10
	uint32_t count; // 0x14
	uint32_t unk18; // 0x18
	uint32_t indexUnk51; // 0x1C indexes _mstShootIndexData
	uint32_t walkPath; // 0x20 indexes _mstWalkPathData
	uint32_t attackBox; // 0x24 indexes _mstAttackBoxData
	uint32_t codeData; // 0x28 indexes _mstCodeData
}; // sizeof == 44

struct MstBehavior { // u46
	MstBehaviorState *data;
	uint32_t count;
}; // sizeof == 8

struct MstAttackBox { // u47
	uint8_t *data; // sizeof == 20 (uint32_t * 5)
	uint32_t count;
}; // sizeof == 8

struct MstMonsterAreaAction {
	uint32_t indexMonsterInfo; // 0x0, indexes _mstMonsterInfos
	uint32_t indexUnk51; // 0x4
	int32_t xPos; // 0x8
	int32_t yPos; // 0xC
	uint32_t codeData; // 0x10
	uint32_t codeData2; // 0x14 stop
	uint8_t unk18; // 0x18
	uint8_t direction; // 0x19
	int8_t screenNum; // 0x1A
	uint8_t monster1Index; // 0x1B
}; // sizeof == 28

struct MstMonsterArea {
	uint8_t unk0;
	MstMonsterAreaAction *data; // 0x4 sizeof == 28
	uint32_t count;
}; // sizeof == 12

struct MstMonsterAction { // u48
	uint16_t xRange;
	uint16_t yRange;
	uint8_t unk4;
	uint8_t direction;
	uint8_t unk6;
	uint8_t unk7;
	uint32_t codeData; // 0x8 indexes _mstCodeData
	MstMonsterArea *area; // 0xC
	int areaCount; // 0x10
	uint32_t *data1[2]; // 0x14, 0x18
	uint32_t *data2[2]; // 0x1C, 0x20
	uint32_t count[2]; // 0x24, 0x28
}; // sizeof == 44

struct MstMovingBoundsUnk1 { // u49u1
	uint32_t offsetMonsterInfo; // 0, offset _mstMonsterInfos[32]
	uint32_t unk4;
	uint8_t unk8;
	uint8_t unk9;
	uint8_t unkA;
	uint8_t unkB;
	uint8_t unkC;
	uint8_t unkD;
	uint8_t unkE;
	uint8_t unkF;
}; // sizeof == 16

struct MstMovingBounds { // u49
	uint32_t indexMonsterInfo; // 0, indexes _mstMonsterInfos
	MstMovingBoundsUnk1 *data1; // 0x4, sizeof == 16
	uint32_t count1; // 0x8
	uint8_t *indexData; // 0xC
	uint32_t indexDataCount; // 0x10
	uint8_t unk14; // 0x14
	uint8_t unk15; // 0x15
	uint8_t unk16; // 0x16
	uint8_t unk17; // 0x17
}; // sizeof == 24

struct MstShootAction { // u50u1
	uint32_t codeData;
	uint32_t unk4;
	uint32_t dirMask;
	uint32_t xPos; // C
	uint32_t yPos; // 10
	uint32_t width; // 14
	uint32_t height; // 18
	uint32_t hSize; // 1C
	uint32_t vSize; // 20
	int32_t unk24;
}; // sizeof == 40

struct MstShoot { // u50
	MstShootAction *data;
	uint32_t count;
}; // sizeof == 8

struct MstShootIndex { // u51
	uint32_t indexUnk50;
	uint32_t *indexUnk50Unk1; // sizeof == 40
	uint32_t count;
}; // sizeof == 12

struct MstActionDirectionMask { // u52
	uint8_t unk0;
	uint8_t unk1;
	uint8_t unk2;
	uint8_t unk3;
}; // sizeof == 4

struct MstOp240Data {
	uint32_t flags;
	uint32_t codeData;
}; // sizeof == 8

struct MstOp223Data {
	int16_t indexVar1;
	int16_t indexVar2; // 2
	int16_t indexVar3; // 4
	int16_t indexVar4; // 6
	uint8_t type; // 8
	uint8_t flags1;
	int8_t indexVar5; // A
	int8_t unkB; // B
	uint16_t flags2; // C
	uint16_t unkE; // E
	uint32_t maskVars; // 0x10
}; // sizeof == 20

struct MstOp227Data {
	int16_t indexVar1; // 0
	int16_t indexVar2; // 2
	uint8_t compare; // 4
	uint8_t maskVars; // 5
	uint16_t codeData; // 6
}; // sizeof == 8

struct MstOp234Data {
	int16_t indexVar1; // 0
	int16_t indexVar2; // 2
	uint8_t compare; // 4
	uint8_t maskVars; // 5
	uint32_t codeData;
}; // sizeof == 8

struct MstOp2Data {
	int32_t indexVar1; // 0
	int32_t indexVar2; // 4
	uint8_t maskVars; // 8
	uint8_t unk9;
	uint8_t unkA;
	uint8_t unkB;
}; // sizeof == 12

struct MstOp226Data {
	uint8_t unk0;
	uint8_t unk1;
	uint8_t unk2;
	uint8_t unk3;
	uint8_t unk4;
	uint8_t unk5;
	uint8_t unk6;
	uint8_t unk7;
}; // sizeof == 8

struct MstOp204Data {
	uint32_t arg0; // 0
	uint32_t arg1; // 4
	uint32_t arg2; // 8
	uint32_t arg3; // C
}; // sizeof == 16

struct MstOp197Data {
	int16_t unk0;
	int16_t unk2;
	int16_t unk4;
	int16_t unk6;
	uint32_t maskVars;
	uint16_t indexUnk49;
	int8_t unkE;
	int8_t unkF;
}; // sizeof == 16

struct MstOp211Data {
	int16_t indexVar1; // 0
	int16_t indexVar2; // 2
	uint16_t unk4; // 4
	int16_t unk6; // 6
	uint8_t unk8; // 8
	uint8_t unk9; // 9
	uint8_t unkA; // A
	uint8_t unkB; // B
	int8_t indexVar3; // C
	uint8_t unkD; // D
	uint16_t maskVars; // E
}; // sizeof == 16

struct SssHdr {
	int version;
	int bufferSize;
	int preloadPcmCount;
	int preloadInfoCount;
	int infosDataCount;
	int filtersDataCount;
	int banksDataCount;
	int samplesDataCount;
	int codeSize;
	int preloadData1Count; // 24
	int preloadData2Count; // 28
	int preloadData3Count; // 2C
	int pcmCount; // 30
};

struct SssInfo {
	uint16_t sssBankIndex; // 0 indexes _sssBanksData
	int8_t sampleIndex; // 2
	uint8_t targetVolume; // 3
	int8_t targetPriority; // 4
	int8_t targetPanning; // 5
	uint8_t concurrencyMask; // 6
};

struct SssDefaults {
	uint8_t defaultVolume;
	int8_t defaultPriority;
	int8_t defaultPanning;
};

struct SssBank {
	uint8_t flags; // 0 flags0
	int8_t count; // 1 codeOffsetCount
	uint16_t sssFilter; // 2 indexes _sssFilters
	uint32_t firstSampleIndex; // 4 indexes _sssSamplesData
};

struct SssSample {
	uint16_t pcm; // indexes _sssPcmTable
	uint16_t framesCount;
	uint8_t initVolume; // 0x4
	uint8_t unk5; // unused
	int8_t initPriority; // 0x6
	int8_t initPanning; // 0x7
	uint32_t codeOffset1; // 0x8 indexes _sssCodeData
	uint32_t codeOffset2; // 0xC indexes _sssCodeData
	uint32_t codeOffset3; // 0x10 indexes _sssCodeData
	uint32_t codeOffset4; // 0x14 indexes _sssCodeData
}; // sizeof == 24

struct SssPreloadList {
	int count;
	int ptrSize;
	uint8_t *ptr; // uint16_t for v6 or v12, uint8_t for v10
};

struct SssPreloadInfoData {
	uint16_t pcmBlockOffset;
	uint16_t pcmBlockSize;
	uint8_t screenNum;
	uint8_t preload3Index;
	uint8_t preload1Index;
	uint8_t preload2Index;
	uint32_t unk1C;
	SssPreloadList preload1Data_V6;
}; // sizeof == 32 (v10,v12) 68 (v6)

struct SssPreloadInfo {
	uint32_t count;
	SssPreloadInfoData *data;
};

struct SssFilter {
	int32_t volume;
	int32_t volumeCurrent; // fp16
	int32_t volumeDelta;
	int32_t volumeSteps;
	int32_t panning;
	int32_t panningCurrent; // fp16
	int32_t panningDelta;
	int32_t panningSteps;
	int32_t priority;
	int32_t priorityCurrent;
	bool changed; // 0x30
}; // sizeof == 52

struct SssPcm {
	int16_t *ptr;    // 0 PCM data
	uint32_t offset; // 4 offset in .sss
	uint32_t totalSize;   // 8 size in .sss (256 int16_t words + followed by indexes)
	uint32_t strideSize;  // 12
	uint16_t strideCount; // 16
	uint16_t flags;       // 18 1:stereo
	uint32_t pcmSize;     // decompressed PCM size in bytes
};

struct SssUnk6 {
	uint32_t unk0[4]; // 0
	uint32_t mask; // 10
};

struct Dem {
	uint32_t randSeed;
	uint32_t keyMaskLen;
	uint8_t level;
	uint8_t checkpoint;
	uint8_t difficulty;
	uint8_t randRounds;
	uint8_t *actionKeyMask;
	uint8_t *directionKeyMask;
};

template <typename T>
struct ResStruct {
	T *ptr;
	unsigned int count;

	ResStruct()
		: ptr(0), count(0) {
	}
	~ResStruct() {
		deallocate();
	}

	void deallocate() {
		free(ptr);
		ptr = 0;
		count = 0;
	}
	void allocate(unsigned int size) {
		free(ptr);
		count = size;
		ptr = (T *)malloc(size * sizeof(T));
	}

	const T& operator[](int i) const {
		assert((unsigned int)i < count);
		return ptr[i];
	}
	T& operator[](int i) {
		assert((unsigned int)i < count);
		return ptr[i];
	}
};

struct FileSystem;

struct Resource {
	enum {
		V1_0,
		V1_1,
		V1_2  // 1.2 or newer
	};

	FileSystem *_fs;

	DatHdr _datHdr;
	File *_datFile;
	LvlHdr _lvlHdr;
	File *_lvlFile;
	MstHdr _mstHdr;
	File *_mstFile;
	SssHdr _sssHdr;
	File *_sssFile;

	bool _isPsx;
	bool _isDemo;
	int _version;

	uint8_t *_loadingImageBuffer;
	uint8_t *_fontBuffer;
	uint8_t _fontDefaultColor;
	uint8_t *_menuBuffer0;
	uint8_t *_menuBuffer1;
	uint32_t _menuBuffersOffset;

	Dem _dem;
	uint32_t _demOffset;

	uint8_t _currentScreenResourceNum;

	uint8_t _screensGrid[kMaxScreens][4];
	LvlScreenVector _screensBasePos[kMaxScreens];
	LvlScreenState _screensState[kMaxScreens];
	uint8_t *_resLevelData0x470CTable; // masks
	uint8_t *_resLevelData0x470CTablePtrHdr;
	uint8_t *_resLevelData0x470CTablePtrData;
	uint32_t _lvlSpritesOffset;
	uint32_t _lvlBackgroundsOffset;
	uint32_t _lvlMasksOffset;
	uint32_t _lvlSssOffset; // .sss offset (PSX)
	uint32_t _resLevelData0x2988SizeTable[kMaxSpriteTypes]; // sprites
	LvlObjectData _resLevelData0x2988Table[kMaxSpriteTypes];
	LvlObjectData *_resLevelData0x2988PtrTable[kMaxSpriteTypes];
	uint8_t *_resLvlSpriteDataPtrTable[kMaxSpriteTypes];
	uint32_t _resLevelData0x2B88SizeTable[kMaxScreens]; // backgrounds
	LvlBackgroundData _resLvlScreenBackgroundDataTable[kMaxScreens];
	uint8_t *_resLvlScreenBackgroundDataPtrTable[kMaxScreens];

	LvlObject _resLvlScreenObjectDataTable[104];
	LvlObject _dummyObject; // (LvlObject *)0xFFFFFFFF

	ResStruct<SssInfo> _sssInfosData;
	ResStruct<SssDefaults> _sssDefaultsData;
	ResStruct<SssBank> _sssBanksData;
	ResStruct<SssSample> _sssSamplesData;
	ResStruct<SssPreloadList> _sssPreload1Table; // pcm
	ResStruct<SssPreloadInfo> _sssPreloadInfosData; // indexed by screen number
	ResStruct<SssFilter> _sssFilters;
	ResStruct<SssPcm> _sssPcmTable;
	ResStruct<SssUnk6> _sssDataUnk6;
	uint32_t *_sssGroup1[3];
	uint32_t *_sssGroup2[3];
	uint32_t *_sssGroup3[3];
	uint8_t *_sssCodeData;

	ResStruct<MstPointOffset> _mstPointOffsets;
	ResStruct<MstWalkBox> _mstWalkBoxData;
	ResStruct<MstWalkCode> _mstWalkCodeData;
	ResStruct<MstMovingBoundsIndex> _mstMovingBoundsIndexData;
	uint32_t _mstTickDelay;
	uint32_t _mstTickCodeData;
	ResStruct<uint32_t> _mstLevelCheckpointCodeData;
	ResStruct<MstScreenArea> _mstScreenAreaData;
	ResStruct<uint32_t> _mstScreenAreaByValueIndexData;
	ResStruct<uint32_t> _mstScreenAreaByPosIndexData;
	ResStruct<uint32_t> _mstUnk41;
	ResStruct<MstBehaviorIndex> _mstBehaviorIndexData;
	ResStruct<MstMonsterActionIndex> _mstMonsterActionIndexData;
	ResStruct<MstWalkPath> _mstWalkPathData;
	ResStruct<MstInfoMonster2> _mstInfoMonster2Data;
	ResStruct<MstBehavior> _mstBehaviorData;
	ResStruct<MstAttackBox> _mstAttackBoxData;
	ResStruct<MstMonsterAction> _mstMonsterActionData;
	uint8_t *_mstMonsterInfos; // sizeof == 948
	ResStruct<MstMovingBounds> _mstMovingBoundsData;
	ResStruct<MstShoot> _mstShootData;
	ResStruct<MstShootIndex> _mstShootIndexData;
	ResStruct<MstActionDirectionMask> _mstActionDirectionData;
	ResStruct<MstOp223Data> _mstOp223Data;
	ResStruct<MstOp227Data> _mstOp227Data;
	ResStruct<MstOp234Data> _mstOp234Data;
	ResStruct<MstOp2Data> _mstOp2Data;
	ResStruct<MstOp197Data> _mstOp197Data;
	ResStruct<MstOp211Data> _mstOp211Data;
	ResStruct<MstOp240Data> _mstOp240Data;
	ResStruct<uint32_t> _mstUnk60; // indexes _mstCodeData
	ResStruct<MstOp204Data> _mstOp204Data;
	uint8_t *_mstCodeData;
	ResStruct<MstOp226Data> _mstOp226Data;

	Resource(FileSystem *fs);
	~Resource();

	bool sectorAlignedGameData();

	void loadSetupDat();
	bool loadDatHintImage(int num, uint8_t *dst, uint8_t *pal);
	bool loadDatLoadingImage(uint8_t *dst, uint8_t *pal);
	void loadDatMenuBuffers();
	void unloadDatMenuBuffers();

	void loadLevelData(int levelNum);

	void loadLvlScreenObjectData(LvlObject *dat, const uint8_t *src);
	void loadLvlData(File *fp);
	void unloadLvlData();
	void loadLvlSpriteData(int num, const uint8_t *buf = 0);
	const uint8_t *getLvlScreenMaskDataPtr(int num) const;
	const uint8_t *getLvlScreenPosDataPtr(int num) const;
	void loadLvlScreenMaskData();
	void loadLvlScreenBackgroundData(int num, const uint8_t *buf = 0);
	void unloadLvlScreenBackgroundData(int num);
	bool isLvlSpriteDataLoaded(int num) const;
	bool isLvlBackgroundDataLoaded(int num) const;
	void incLvlSpriteDataRefCounter(LvlObject *ptr);
	void decLvlSpriteDataRefCounter(LvlObject *ptr);
	const uint8_t *getLvlSpriteFramePtr(LvlObjectData *dat, int frame, uint16_t *w, uint16_t *h) const;
	const uint8_t *getLvlSpriteCoordPtr(LvlObjectData *dat, int num) const;
	int findScreenGridIndex(int screenNum) const;

	void loadSssData(File *fp, const uint32_t baseOffset = 0);
	void unloadSssData();
	void checkSssCode(const uint8_t *buf, int size) const;
	void loadSssPcm(File *fp, SssPcm *pcm);
	void clearSssGroup3();
	void resetSssFilters();
	void preloadSssPcmList(const SssPreloadInfoData *preloadInfoData);

	void loadMstData(File *fp);
	void unloadMstData();
	const MstScreenArea *findMstCodeForPos(int num, int xPos, int yPos) const;
	void flagMstCodeForPos(int num, uint8_t value);

	bool loadHodDem();
	void unloadHodDem();

	bool writeSetupCfg(const SetupConfig *config);
	bool readSetupCfg(SetupConfig *config);
	void setDefaultsSetupCfg(SetupConfig *config, int num);
};

#endif // RESOURCE_H__
