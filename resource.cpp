/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "fileio.h"
#include "fs.h"
#include "game.h"
#include "lzw.h"
#include "resource.h"
#include "util.h"

// menu settings and player progress
static const char *_setupCfg = "setup.cfg";

static const char *_setupDat = "SETUP.DAT";
static const char *_setupDax = "SETUP.DAX";

static const char *_prefixes[] = {
	"rock",
	"fort",
	"pwr1",
	"isld",
	"lava",
	"pwr2",
	"lar1",
	"lar2",
	"dark",
	"test"
};

static bool openDat(FileSystem *fs, const char *name, File *f) {
	FILE *fp = fs->openAssetFile(name);
	if (fp) {
		f->setFp(fp);
		return true;
	}
	return false;
}

static void closeDat(FileSystem *fs, File *f) {
	if (f->_fp) {
		fs->closeFile(f->_fp);
		f->setFp(0);
	}
}

static int skipBytesAlign(File *f, int len) {
	const int size = (len + 3) & ~3;
	f->seek(size, SEEK_CUR);
	return size;
}

static int readBytesAlign(File *f, uint8_t *buf, int len) {
	f->read(buf, len);
	if ((len & 3) != 0) {
		f->seek(4 - (len & 3), SEEK_CUR);
	}
	return (len + 3) & ~3;
}

Resource::Resource(FileSystem *fs)
	: _fs(fs), _isPsx(false) {

	memset(_screensGrid, 0, sizeof(_screensGrid));
	memset(_screensBasePos, 0, sizeof(_screensBasePos));
	memset(_screensState, 0, sizeof(_screensState));

	// masks
	_resLevelData0x470CTable = 0;
	_resLevelData0x470CTablePtrHdr = 0;
	_resLevelData0x470CTablePtrData = 0;
	_lvlSssOffset = 0;

	// sprites
	memset(_resLevelData0x2988SizeTable, 0, sizeof(_resLevelData0x2988SizeTable));
	memset(_resLevelData0x2988Table, 0, sizeof(_resLevelData0x2988Table));
	memset(_resLevelData0x2988PtrTable, 0, sizeof(_resLevelData0x2988PtrTable));
	memset(_resLvlSpriteDataPtrTable, 0, sizeof(_resLvlSpriteDataPtrTable));

	// backgrounds
	memset(_resLvlScreenBackgroundDataTable, 0, sizeof(_resLvlScreenBackgroundDataTable));
	memset(_resLvlScreenBackgroundDataPtrTable, 0, sizeof(_resLvlScreenBackgroundDataPtrTable));
	memset(_resLevelData0x2B88SizeTable, 0, sizeof(_resLevelData0x2B88SizeTable));

	memset(_resLvlScreenObjectDataTable, 0, sizeof(_resLvlScreenObjectDataTable));
	memset(&_dummyObject, 0, sizeof(_dummyObject));

	if (sectorAlignedGameData()) {
		_datFile = new SectorFile;
		_lvlFile = new SectorFile;
		_mstFile = new SectorFile;
		_sssFile = new SectorFile;
	} else {
		_datFile = new File;
		_lvlFile = new File;
		_mstFile = new File;
		_sssFile = new File;
	}
	memset(&_datHdr, 0, sizeof(_datHdr));
	memset(&_lvlHdr, 0, sizeof(_lvlHdr));
	memset(&_mstHdr, 0, sizeof(_mstHdr));
	memset(&_sssHdr, 0, sizeof(_sssHdr));

	_loadingImageBuffer = 0;
	_fontBuffer = 0;
	_menuBuffer0 = 0;
	_menuBuffer1 = 0;
}

Resource::~Resource() {
	delete _datFile;
	delete _lvlFile;
	delete _mstFile;
	delete _sssFile;
}

bool Resource::sectorAlignedGameData() {
	FILE *fp = _fs->openAssetFile(_setupDat);
	if (!fp) {
		fp = _fs->openAssetFile(_setupDax);
		if (!fp) {
			error("Unable to open '%s' or '%s'", _setupDat, _setupDax);
			return false;
		}
	}
	bool ret = false;
	uint8_t buf[2048];
	if (fread(buf, 1, sizeof(buf), fp) == sizeof(buf)) {
		ret = fioUpdateCRC(0, buf, sizeof(buf)) == 0;
	}
	_fs->closeFile(fp);
	return ret;
}

void Resource::loadSetupDat() {
	if (!openDat(_fs, _setupDat, _datFile)) {
		_isPsx = openDat(_fs, _setupDax, _datFile);
	}

	_datHdr.version = _datFile->readUint32();
	if (_datHdr.version != 10 && _datHdr.version != 11) {
		warning("Unhandled .dat version %d", _datHdr.version);
		return;
	}

	_datHdr.bufferSize0    = _datFile->readUint32();
	_datHdr.bufferSize1    = _datFile->readUint32();
	_datHdr.sssOffset      = _datFile->readUint32();
	_datHdr.iconsCount     = _datFile->readUint32();
	_datHdr.menusCount     = _datFile->readUint32();
	_datHdr.cutscenesCount = _datFile->readUint32();
	_datHdr.levelsCount    = _datFile->readUint32();
	for (int i = 0; i < kLvl_dark; ++i) { // last level has a single checkpoint
		_datHdr.levelCheckpointsCount[i] = _datFile->readUint32();
	}
	_datHdr.yesNoQuitImage   = _datFile->readUint32();
	_datHdr.soundDataSize    = _datFile->readUint32();
	_datHdr.loadingImageSize = _datFile->readUint32();
	const int hintsCount = (_datHdr.version == 11) ? 46 : 20;
	for (int i = 0; i < hintsCount; ++i) {
		_datHdr.hintsImageOffsetTable[i] = _datFile->readUint32();
	}
	for (int i = 0; i < hintsCount; ++i) {
		_datHdr.hintsImageSizeTable[i] = _datFile->readUint32();
	}
	_datFile->seek(2048, SEEK_SET); // align to the next sector
	_loadingImageBuffer = (uint8_t *)malloc(_datHdr.loadingImageSize);
	if (_loadingImageBuffer) {
		_datFile->read(_loadingImageBuffer, _datHdr.loadingImageSize);

		uint32_t offset = 0;

		if (!_isPsx) {
			// loading image
			uint32_t size = READ_LE_UINT32(_loadingImageBuffer + offset); offset += 8;
			offset += size + 768;

			// loading animation
			size = READ_LE_UINT32(_loadingImageBuffer + offset + 8); offset += 16;
			offset += size;
		}

		// font
		static const int kFontSize = 16 * 16 * 64;
		_fontBuffer = (uint8_t *)malloc(kFontSize);
		if (_fontBuffer) {
			/* size = READ_LE_UINT32(_loadingImageBuffer + offset); */ offset += 4;
			if (_datHdr.version == 11) {
				const uint32_t uncompressedSize = decodeLZW(_loadingImageBuffer + offset, _fontBuffer);
				assert(uncompressedSize == kFontSize);
			} else {
				memcpy(_fontBuffer, _loadingImageBuffer + offset, kFontSize);
			}
		}
	}
	assert(_datHdr.yesNoQuitImage == hintsCount - 3);
	_menuBuffersOffset = _datHdr.hintsImageOffsetTable[_datHdr.yesNoQuitImage + 2];
}

bool Resource::loadDatHintImage(int num, uint8_t *dst, uint8_t *pal) {
	if (!_isPsx) {
		const int offset = _datHdr.hintsImageOffsetTable[num];
		const int size = _datHdr.hintsImageSizeTable[num];
		assert(size == 256 * 192);
		_datFile->seek(offset, SEEK_SET);
		_datFile->read(dst, size);
		_datFile->flush();
		_datFile->read(pal, 768);
		return true;
	}
	return false;
}

bool Resource::loadDatLoadingImage(uint8_t *dst, uint8_t *pal) {
	if (!_isPsx && _loadingImageBuffer) {
		const uint32_t bufferSize = READ_LE_UINT32(_loadingImageBuffer);
		const int size = decodeLZW(_loadingImageBuffer + 8, dst);
		assert(size == 256 * 192);
		// palette follows compressed bitmap
		memcpy(pal, _loadingImageBuffer + 8 + bufferSize, 256 * 3);
		return true;
	}
	return false;
}

void Resource::loadDatMenuBuffers() {
	assert((_datHdr.sssOffset & 0x7FF) == 0);
	_datFile->seek(_datHdr.sssOffset, SEEK_SET);
	loadSssData(_datFile, _datHdr.sssOffset);

	uint32_t baseOffset = _menuBuffersOffset;
	_datFile->seek(baseOffset, SEEK_SET);
	_menuBuffer1 = (uint8_t *)malloc(_datHdr.bufferSize1);
	if (_menuBuffer1) {
		_datFile->read(_menuBuffer1, _datHdr.bufferSize1);
	}
	if (_datHdr.version == 11) {
		baseOffset += fioAlignSizeTo2048(_datHdr.bufferSize1); // align to the next sector
		_datFile->seek(baseOffset, SEEK_SET);
	}
	_menuBuffer0 = (uint8_t *)malloc(_datHdr.bufferSize0);
	if (_menuBuffer0) {
		_datFile->read(_menuBuffer0, _datHdr.bufferSize0);
	}
}

void Resource::unloadDatMenuBuffers() {
	free(_menuBuffer1);
	_menuBuffer1 = 0;
	free(_menuBuffer0);
	_menuBuffer0 = 0;
}

void Resource::loadLevelData(int levelNum) {

	char filename[32];
	const char *levelName = _prefixes[levelNum];

	closeDat(_fs, _lvlFile);
	snprintf(filename, sizeof(filename), "%s_HOD.LVL", levelName);
	if (openDat(_fs, filename, _lvlFile)) {
		loadLvlData(_lvlFile);
	} else {
		error("Unable to open '%s'", filename);
	}

	closeDat(_fs, _mstFile);
	snprintf(filename, sizeof(filename), "%s_HOD.MST", levelName);
	if (openDat(_fs, filename, _mstFile)) {
		loadMstData(_mstFile);
	} else {
		warning("Unable to open '%s'", filename);
		memset(&_mstHdr, 0, sizeof(_mstHdr));
	}

	closeDat(_fs, _sssFile);
	snprintf(filename, sizeof(filename), "%s_HOD.SSS", levelName);
	if (openDat(_fs, filename, _sssFile)) {
		loadSssData(_sssFile);
	} else if (_isPsx) {
		assert((_lvlSssOffset & 0x7FF) == 0);
		_lvlFile->seek(_lvlSssOffset, SEEK_SET);
		loadSssData(_lvlFile, _lvlSssOffset);
	} else {
		warning("Unable to open '%s'", filename);
		memset(&_sssHdr, 0, sizeof(_sssHdr));
	}
}

void Resource::loadLvlScreenObjectData(LvlObject *dat, const uint8_t *src) {
	const uint8_t *start = src;
	dat->xPos = READ_LE_UINT32(src); src += 4;
	dat->yPos = READ_LE_UINT32(src); src += 4;
	dat->screenNum = *src++;
	dat->screenState = *src++;
	dat->dataNum = *src++;
	dat->frame = *src++;
	dat->anim = READ_LE_UINT16(src); src += 2;
	dat->type = *src++;
	dat->spriteNum = *src++;
	dat->flags0 = READ_LE_UINT16(src); src += 2;
	dat->flags1 = READ_LE_UINT16(src); src += 2;
	dat->flags2 = READ_LE_UINT16(src); src += 2;
	dat->objectUpdateType = *src++;
	dat->hitCount = *src++;
	const uint32_t objRef = READ_LE_UINT32(src); src  += 4;
	if (objRef) {
		dat->childPtr = &_dummyObject;
		debug(kDebug_RESOURCE, "loadLvlObj dat %p linkObjRef 0x%x", dat, objRef);
	}
	dat->width = READ_LE_UINT16(src); src += 2;
	dat->height = READ_LE_UINT16(src); src += 2;
	dat->directionKeyMask = *src++;
	dat->actionKeyMask = *src++;
	dat->currentSprite = READ_LE_UINT16(src); src += 2;
	dat->currentSound = READ_LE_UINT16(src); src += 2;
	src += 2; // 0x26
	dat->bitmapBits = 0; src += 4;
	dat->callbackFuncPtr = 0; src += 4;
	dat->dataPtr = 0; src += 4;
	dat->sssObject = 0; src += 4;
	dat->levelData0x2988 = 0; src += 4;
	for (int i = 0; i < 8; ++i) {
		dat->posTable[i].x = READ_LE_UINT16(src); src += 2;
		dat->posTable[i].y = READ_LE_UINT16(src); src += 2;
	}
	dat->nextPtr = 0; src += 4;
	assert((src - start) == 96);
}

static uint32_t resFixPointersLevelData0x2988(uint8_t *src, uint8_t *ptr, LvlObjectData *dat, bool isPsx) {
	uint8_t *base = src;

	dat->unk0 = *src++;
	dat->spriteNum = *src++;
	dat->framesCount = READ_LE_UINT16(src); src += 2;
	dat->hotspotsCount = READ_LE_UINT16(src); src += 2;
	dat->movesCount = READ_LE_UINT16(src); src += 2;
	dat->coordsCount = READ_LE_UINT16(src); src += 2;
	dat->refCount = *src++;
	dat->frame = *src++;
	dat->anim = READ_LE_UINT16(src); src += 2;
	src += 2; // 0xE
	src += 4; // 0x10
	uint32_t movesDataOffset = READ_LE_UINT32(src); src += 4; // 0x14
	src += 4; // 0x18
	uint32_t framesDataOffset = READ_LE_UINT32(src); src += 4; // 0x1C
	src += 4; // 0x20
	uint32_t coordsDataOffset = READ_LE_UINT32(src); src += 4; // 0x24
	uint32_t hotspotsDataOffset = READ_LE_UINT32(src); src += 4; // 0x28

	if (dat->refCount != 0) {
		return 0;
	}

	assert(src == base + kLvlAnimHdrOffset);
	dat->animsInfoData = base;
	dat->refCount = 0xFF;
	dat->framesData = (framesDataOffset == 0) ? 0 : base + framesDataOffset;
	dat->hotspotsData = (hotspotsDataOffset == 0) ? 0 : base + hotspotsDataOffset;
	dat->movesData = (movesDataOffset == 0) ? 0 : base + movesDataOffset;
	dat->coordsData = (coordsDataOffset == 0) ? 0 : base + coordsDataOffset;
	if (kByteSwapData) {
		for (int i = 0; i < dat->hotspotsCount; ++i) {
			LvlAnimHeader *ah = ((LvlAnimHeader *)(base + kLvlAnimHdrOffset)) + i;
			ah->unk0 = le16toh(ah->unk0);
			ah->seqOffset = le32toh(ah->seqOffset);
			if (ah->seqOffset == 0) {
				continue;
			}
			for (int j = 0; j < ah->seqCount; ++j) {
				LvlAnimSeqHeader *ash = ((LvlAnimSeqHeader *)(base + ah->seqOffset)) + j;
				ash->firstFrame = le16toh(ash->firstFrame);
				ash->unk2 = le16toh(ash->unk2);
				ash->sound = le16toh(ash->sound);
				ash->flags0 = le16toh(ash->flags0);
				ash->flags1 = le16toh(ash->flags1);
				ash->unkE = le16toh(ash->unkE);
				ash->offset = le32toh(ash->offset);
				if (ash->offset == 0) {
					continue;
				}
				for (int k = 0; k < ash->count; ++k) {
					LvlAnimSeqFrameHeader *asfh = ((LvlAnimSeqFrameHeader *)(base + ash->offset)) + k;
					asfh->move = le16toh(asfh->move);
					asfh->anim = le16toh(asfh->anim);
				}
			}
		}
		for (int i = 0; i < dat->movesCount; ++i) {
			LvlSprMoveData *smd = ((LvlSprMoveData *)dat->movesData) + i;
			smd->op3 = le16toh(smd->op3);
			smd->op4 = le16toh(smd->op4);
		}
	}

	dat->framesOffsetsTable = ptr;
	if (dat->coordsData) {
		dat->coordsOffsetsTable = ptr + dat->framesCount * 4;
	} else {
		dat->coordsOffsetsTable = 0;
	}

	if (dat->unk0 == 1) { // fixed size offset table
		assert(isPsx);
		dat->framesOffsetsTable = (uint8_t *)malloc(dat->framesCount * sizeof(uint32_t));
		uint32_t framesOffset = 6 * dat->framesCount;
		if (READ_LE_UINT16(dat->framesData + framesOffset) == 0) {
			framesOffset += 2;
		}
		for (int i = 0; i < dat->framesCount; ++i) {
			const int size = READ_LE_UINT16(dat->framesData + i * 6);
			WRITE_LE_UINT32(dat->framesOffsetsTable + i * sizeof(uint32_t), framesOffset);
			framesOffset += size;
		}
		dat->coordsOffsetsTable = ptr;
		return 0;
	}

	uint32_t framesOffset = 0;
	for (int i = 0; i < dat->framesCount; ++i) {
		const int size = READ_LE_UINT16(dat->framesData + framesOffset);
		WRITE_LE_UINT32(dat->framesOffsetsTable + i * sizeof(uint32_t), framesOffset);
		framesOffset += size;
	}
	uint32_t coordsOffset = 0;
	for (int i = 0; i < dat->coordsCount; ++i) {
		const int count = dat->coordsData[coordsOffset];
		WRITE_LE_UINT32(dat->coordsOffsetsTable + i * sizeof(uint32_t), coordsOffset);
		coordsOffset += count * 4 + 1;
	}

	return (dat->framesCount + dat->coordsCount) * sizeof(uint32_t);
}

void Resource::loadLvlSpriteData(int num) {
	assert((unsigned int)num < kMaxSpriteTypes);

	static const uint32_t baseOffset = 0x2988;

	uint8_t buf[4 * 3];
	_lvlFile->seekAlign(baseOffset + num * 16);
	_lvlFile->read(buf, sizeof(buf));
	const uint32_t offset = READ_LE_UINT32(&buf[0]);
	const uint32_t size = READ_LE_UINT32(&buf[4]);
	const uint32_t readSize = READ_LE_UINT32(&buf[8]);
	assert(readSize <= size);
	uint8_t *ptr = (uint8_t *)malloc(size);
	_lvlFile->seek(_isPsx ? _lvlSssOffset + offset : offset, SEEK_SET);
	_lvlFile->read(ptr, readSize);

	LvlObjectData *dat = &_resLevelData0x2988Table[num];
	const uint32_t readOffsetsSize = resFixPointersLevelData0x2988(ptr, ptr + readSize, dat, _isPsx);
	const uint32_t allocatedOffsetsSize = size - readSize;
	assert(allocatedOffsetsSize == readOffsetsSize);

	_resLevelData0x2988PtrTable[dat->spriteNum] = dat;
	_resLvlSpriteDataPtrTable[num] = ptr;
	_resLevelData0x2988SizeTable[num] = size;
}

const uint8_t *Resource::getLvlScreenMaskDataPtr(int num) const {
	assert((unsigned int)num < kMaxScreens * 4);
	const uint32_t offset = READ_LE_UINT32(_resLevelData0x470CTablePtrHdr + num * 8);
	return (offset != 0) ? _resLevelData0x470CTable + offset : 0;
}

const uint8_t *Resource::getLvlScreenPosDataPtr(int num) const {
	assert((unsigned int)num < kMaxScreens * 4);
	const uint32_t offset = READ_LE_UINT32(_resLevelData0x470CTablePtrHdr + num * 8 + 4);
	return (offset != 0) ? _resLevelData0x470CTable + offset : 0;
}

void Resource::loadLvlScreenMaskData() {
	_lvlFile->seekAlign(0x4708);
	const uint32_t offset = _lvlFile->readUint32();
	const uint32_t size = _lvlFile->readUint32();
	_resLevelData0x470CTable = (uint8_t *)malloc(size);
	_lvlFile->seek(offset, SEEK_SET);
	_lvlFile->read(_resLevelData0x470CTable, size);
	_resLevelData0x470CTablePtrHdr = _resLevelData0x470CTable;
	_resLevelData0x470CTablePtrData = _resLevelData0x470CTable + (kMaxScreens * 4) * (2 * sizeof(uint32_t));
	// .sss is embedded in .lvl on PSX
	_lvlSssOffset = offset + fioAlignSizeTo2048(size);
}

static const uint32_t _lvlTag = 0x484F4400; // 'HOD\x00'

void Resource::loadLvlData(File *fp) {

	assert(fp == _lvlFile);

	unloadLvlData();

	const uint32_t tag = _lvlFile->readUint32();
	if (tag != _lvlTag) {
		error("Unhandled .lvl tag 0x%x", tag);
		closeDat(_fs, _lvlFile);
		return;
	}

	_lvlHdr.screensCount = _lvlFile->readByte();
	_lvlHdr.staticLvlObjectsCount = _lvlFile->readByte();
	_lvlHdr.otherLvlObjectsCount = _lvlFile->readByte();
	_lvlHdr.spritesCount = _lvlFile->readByte();
	debug(kDebug_RESOURCE, "Resource::loadLvlData() %d %d %d %d", _lvlHdr.screensCount, _lvlHdr.staticLvlObjectsCount, _lvlHdr.otherLvlObjectsCount, _lvlHdr.spritesCount);

	_lvlFile->seekAlign(0x8);
	for (int i = 0; i < _lvlHdr.screensCount; ++i) {
		_lvlFile->read(_screensGrid[i], 4);
	}
	_lvlFile->seekAlign(0xA8);
	for (int i = 0; i < _lvlHdr.screensCount; ++i) {
		LvlScreenVector *dat = &_screensBasePos[i];
		dat->u = _lvlFile->readUint32();
		dat->v = _lvlFile->readUint32();
	}
	_lvlFile->seekAlign(0x1E8);
	for (int i = 0; i < _lvlHdr.screensCount; ++i) {
		LvlScreenState *dat = &_screensState[i];
		dat->s0 = _lvlFile->readByte();
		dat->s1 = _lvlFile->readByte();
		dat->s2 = _lvlFile->readByte();
		dat->s3 = _lvlFile->readByte();
	}
	_lvlFile->seekAlign(0x288);
	static const int kSizeOfLvlObject = 96;
	for (int i = 0; i < (0x2988 - 0x288) / kSizeOfLvlObject; ++i) {
		LvlObject *dat = &_resLvlScreenObjectDataTable[i];
		uint8_t buf[kSizeOfLvlObject];
		_lvlFile->read(buf, kSizeOfLvlObject);
		loadLvlScreenObjectData(dat, buf);
	}

	loadLvlScreenMaskData();

	memset(_resLevelData0x2B88SizeTable, 0, sizeof(_resLevelData0x2B88SizeTable));
	memset(_resLevelData0x2988SizeTable, 0, sizeof(_resLevelData0x2988SizeTable));
	memset(_resLevelData0x2988PtrTable, 0, sizeof(_resLevelData0x2988PtrTable));

	for (int i = 0; i < _lvlHdr.spritesCount; ++i) {
		loadLvlSpriteData(i);
	}
}

void Resource::unloadLvlData() {
	free(_resLevelData0x470CTable);
	_resLevelData0x470CTable = 0;
	for (unsigned int i = 0; i < kMaxScreens; ++i) {
		unloadLvlScreenBackgroundData(i);
	}
	for (unsigned int i = 0; i < kMaxSpriteTypes; ++i) {
		LvlObjectData *dat = &_resLevelData0x2988Table[i];
		if (dat->unk0 == 1) {
			free(dat->framesOffsetsTable);
			dat->framesOffsetsTable = 0;
		}
		free(_resLvlSpriteDataPtrTable[i]);
		_resLvlSpriteDataPtrTable[i] = 0;
	}
}

static uint32_t resFixPointersLevelData0x2B88(const uint8_t *src, uint8_t *ptr, uint8_t *offsetsPtr, LvlBackgroundData *dat, bool isPsx) {
	const uint8_t *start = src;

	dat->backgroundCount = *src++;
	dat->currentBackgroundId = *src++;
	dat->maskCount = *src++;
	dat->currentMaskId = *src++;
	dat->shadowCount = *src++;
	dat->currentShadowId = *src++;
	dat->soundCount = *src++;
	dat->currentSoundId = *src++;
	dat->dataUnk3Count = *src++;
	dat->unk9 = *src++;
	dat->dataUnk45Count = *src++;
	dat->unkB = *src++;
	dat->backgroundPaletteId = READ_LE_UINT16(src); src += 2;
	dat->backgroundBitmapId = READ_LE_UINT16(src); src += 2;
	for (int i = 0; i < 4; ++i) {
		const uint32_t offs = READ_LE_UINT32(src); src += 4;
		dat->backgroundPaletteTable[i] = (offs != 0) ? ptr + offs : 0;
	}
	for (int i = 0; i < 4; ++i) {
		const uint32_t offs = READ_LE_UINT32(src); src += 4;
		dat->backgroundBitmapTable[i] = (offs != 0) ? ptr + offs : 0;
	}
	for (int i = 0; i < 4; ++i) {
		const uint32_t offs = READ_LE_UINT32(src); src += 4;
		dat->dataUnk0Table[i] = (offs != 0) ? ptr + offs : 0;
	}
	for (int i = 0; i < 4; ++i) {
		const uint32_t offs = READ_LE_UINT32(src); src += 4;
		dat->backgroundMaskTable[i] = (offs != 0) ? ptr + offs : 0;
	}
	for (int i = 0; i < 4; ++i) {
		const uint32_t offs = READ_LE_UINT32(src); src += 4;
		dat->backgroundSoundTable[i] = (offs != 0) ? ptr + offs : 0;
	}
	for (int i = 0; i < 8; ++i) {
		const uint32_t offs = READ_LE_UINT32(src); src += 4;
		dat->backgroundAnimationTable[i] = (offs != 0) ? ptr + offs : 0;
	}
	uint32_t offsetsSize = 0;
	for (int i = 0; i < 8; ++i) {
		const uint32_t offs = READ_LE_UINT32(src); src += 4;
		if (offs != 0) {
			dat->backgroundLvlObjectDataTable[i] = (LvlObjectData *)malloc(sizeof(LvlObjectData));
			offsetsSize += resFixPointersLevelData0x2988(ptr + offs, offsetsPtr + offsetsSize, dat->backgroundLvlObjectDataTable[i], isPsx);
		} else {
			dat->backgroundLvlObjectDataTable[i] = 0;
		}
	}
	assert((src - start) == 160);
	return offsetsSize;
}

void Resource::loadLvlScreenBackgroundData(int num) {
	assert((unsigned int)num < kMaxScreens);

	static const uint32_t baseOffset = 0x2B88;

	uint8_t buf[4 * 3];
	_lvlFile->seekAlign(baseOffset + num * 16);
	_lvlFile->read(buf, sizeof(buf));
	const uint32_t offset = READ_LE_UINT32(&buf[0]);
	const uint32_t size = READ_LE_UINT32(&buf[4]);
	const uint32_t readSize = READ_LE_UINT32(&buf[8]);
	assert(readSize <= size);
	uint8_t *ptr = (uint8_t *)malloc(size);
	_lvlFile->seek(_isPsx ? _lvlSssOffset + offset : offset, SEEK_SET);
	_lvlFile->read(ptr, readSize);

	uint8_t hdr[160];
	_lvlFile->seekAlign(baseOffset + kMaxScreens * 16 + num * 160);
	_lvlFile->read(hdr, 160);
	LvlBackgroundData *dat = &_resLvlScreenBackgroundDataTable[num];
	const uint32_t readOffsetsSize = resFixPointersLevelData0x2B88(hdr, ptr, ptr + readSize, dat, _isPsx);
	const uint32_t allocatedOffsetsSize = size - readSize;
	assert(allocatedOffsetsSize == readOffsetsSize);

	_resLvlScreenBackgroundDataPtrTable[num] = ptr;
	_resLevelData0x2B88SizeTable[num] = size;
}

void Resource::unloadLvlScreenBackgroundData(int num) {
	if (_resLevelData0x2B88SizeTable[num] != 0) {
		free(_resLvlScreenBackgroundDataPtrTable[num]);
		_resLvlScreenBackgroundDataPtrTable[num] = 0;
		_resLevelData0x2B88SizeTable[num] = 0;

		LvlBackgroundData *dat = &_resLvlScreenBackgroundDataTable[num];
		for (int i = 0; i < 4; ++i) {
			free(dat->backgroundLvlObjectDataTable[i]);
		}
		memset(dat, 0, sizeof(LvlBackgroundData));
	}
}

bool Resource::isLvlSpriteDataLoaded(int num) const {
	return _resLevelData0x2988SizeTable[num] != 0;
}

bool Resource::isLvlBackgroundDataLoaded(int num) const {
	return _resLevelData0x2B88SizeTable[num] != 0;
}

void Resource::incLvlSpriteDataRefCounter(LvlObject *ptr) {
	LvlObjectData *dat = _resLevelData0x2988PtrTable[ptr->spriteNum];
	assert(dat);
	++dat->refCount;
	ptr->levelData0x2988 = dat;
}

void Resource::decLvlSpriteDataRefCounter(LvlObject *ptr) {
	LvlObjectData *dat = _resLevelData0x2988PtrTable[ptr->spriteNum];
	if (dat) {
		--dat->refCount;
	}
}

const uint8_t *Resource::getLvlSpriteFramePtr(LvlObjectData *dat, int frame, uint16_t *w, uint16_t *h) const {
	assert(frame < dat->framesCount);
	const uint8_t *p = dat->framesData;
	if (dat->unk0 == 1) {
		p += frame * 6;
		const uint16_t size = READ_LE_UINT16(p);
		*w = READ_LE_UINT16(p + 2);
		*h = READ_LE_UINT16(p + 4);
		if (size > 8) {
			return dat->framesData + READ_LE_UINT32(dat->framesOffsetsTable + frame * sizeof(uint32_t));
		}
	} else {
		p += READ_LE_UINT32(dat->framesOffsetsTable + frame * sizeof(uint32_t));
		const uint16_t size = READ_LE_UINT16(p);
		*w = READ_LE_UINT16(p + 2);
		*h = READ_LE_UINT16(p + 4);
		if (size > 8) {
			return p + 6;
		}
	}
	return 0;
}

const uint8_t *Resource::getLvlSpriteCoordPtr(LvlObjectData *dat, int num) const {
	assert(num < dat->coordsCount);
	return dat->coordsData + READ_LE_UINT32(dat->coordsOffsetsTable + num * sizeof(uint32_t));
}

int Resource::findScreenGridIndex(int screenNum) const {
	for (int i = 0; i < 4; ++i) {
		if (_screensGrid[_currentScreenResourceNum][i] == screenNum) {
			return i;
		}
	}
	if (_currentScreenResourceNum == screenNum) {
		return 4;
	}
	return -1;
}

void Resource::loadSssData(File *fp, const uint32_t baseOffset) {

	assert(fp == _sssFile || fp == _datFile || fp == _lvlFile);

	if (_sssHdr.bufferSize != 0) {
		const int count = MIN(_sssHdr.pcmCount, _sssHdr.preloadPcmCount);
		for (int i = 0; i < count; ++i) {
			free(_sssPcmTable[i].ptr);
			_sssPcmTable[i].ptr = 0;
		}
		unloadSssData();
		_sssHdr.bufferSize = 0;
		_sssHdr.infosDataCount = 0;
	}
	_sssHdr.version = fp->readUint32();
	if (_sssHdr.version != 6 && _sssHdr.version != 10 && _sssHdr.version != 12) {
		warning("Unhandled .sss version %d", _sssHdr.version);
		closeDat(_fs, _sssFile);
		return;
	}

	_sssHdr.bufferSize = fp->readUint32();
	_sssHdr.preloadPcmCount = fp->readUint32();
	_sssHdr.preloadInfoCount = fp->readUint32();
	debug(kDebug_RESOURCE, "_sssHdr.bufferSize %d _sssHdr.preloadPcmCount %d _sssHdr.preloadInfoCount %d", _sssHdr.bufferSize, _sssHdr.preloadPcmCount, _sssHdr.preloadInfoCount);
	_sssHdr.infosDataCount = fp->readUint32();
	_sssHdr.filtersDataCount = fp->readUint32();
	_sssHdr.banksDataCount = fp->readUint32();
	debug(kDebug_RESOURCE, "_sssHdr.infosDataCount %d _sssHdr.filtersDataCount %d _sssHdr.banksDataCount %d", _sssHdr.infosDataCount, _sssHdr.filtersDataCount, _sssHdr.banksDataCount);
	_sssHdr.samplesDataCount = fp->readUint32();
	_sssHdr.codeSize = fp->readUint32();
	debug(kDebug_RESOURCE, "_sssHdr.samplesDataCount %d _sssHdr.codeSize %d", _sssHdr.samplesDataCount, _sssHdr.codeSize);
	if (_sssHdr.version == 10 || _sssHdr.version == 12) {
		_sssHdr.preloadData1Count = fp->readUint32() & 255; // pcm
		_sssHdr.preloadData2Count = fp->readUint32() & 255; // sprites
		_sssHdr.preloadData3Count = fp->readUint32() & 255; // mst
	}
	_sssHdr.pcmCount = fp->readUint32();

	const int bufferSize = _sssHdr.bufferSize + _sssHdr.filtersDataCount * 52 + _sssHdr.banksDataCount * 56;
	debug(kDebug_RESOURCE, "bufferSize %d", bufferSize);

	// fp->flush();
	fp->seek(baseOffset + 2048, SEEK_SET); // align to the next sector

	// _sssBuffer1
	int bytesRead = 0;

	_sssInfosData.allocate(_sssHdr.infosDataCount);
	for (int i = 0; i < _sssHdr.infosDataCount; ++i) {
		_sssInfosData[i].sssBankIndex = fp->readUint16(); // index _sssBanksData
		_sssInfosData[i].sampleIndex = fp->readByte();
		_sssInfosData[i].targetVolume = fp->readByte();
		_sssInfosData[i].targetPriority = fp->readByte();
		_sssInfosData[i].targetPanning = fp->readByte();
		_sssInfosData[i].concurrencyMask = fp->readByte();
		fp->skipByte(); // padding to 8 bytes
		bytesRead += 8;
	}
	_sssDefaultsData.allocate(_sssHdr.filtersDataCount);
	for (int i = 0; i < _sssHdr.filtersDataCount; ++i) {
		_sssDefaultsData[i].defaultVolume   = fp->readByte();
		_sssDefaultsData[i].defaultPriority = fp->readByte();
		_sssDefaultsData[i].defaultPanning  = fp->readByte();
		fp->skipByte(); // padding to 4 bytes
		bytesRead += 4;
	}
	_sssBanksData.allocate(_sssHdr.banksDataCount);
	for (int i = 0; i < _sssHdr.banksDataCount; ++i) {
		_sssBanksData[i].flags = fp->readByte();
		_sssBanksData[i].count = fp->readByte();
		assert(_sssBanksData[i].count <= 4); // matches sizeof(_sssDataUnk6.unk0)
		_sssBanksData[i].sssFilter = fp->readUint16();
		_sssBanksData[i].firstSampleIndex = fp->readUint32();
		debug(kDebug_RESOURCE, "SssBank #%d count %d codeOffset 0x%x", i, _sssBanksData[i].count, _sssBanksData[i].firstSampleIndex);
		bytesRead += 8;
	}
	_sssSamplesData.allocate(_sssHdr.samplesDataCount);
	for (int i = 0; i < _sssHdr.samplesDataCount; ++i) {
		_sssSamplesData[i].pcm = fp->readUint16();
		_sssSamplesData[i].framesCount = fp->readUint16();
		_sssSamplesData[i].initVolume = fp->readByte();
		_sssSamplesData[i].unk5 = fp->readByte();
		_sssSamplesData[i].initPriority = fp->readByte();
		_sssSamplesData[i].initPanning = fp->readByte();
		_sssSamplesData[i].codeOffset1 = fp->readUint32();
		_sssSamplesData[i].codeOffset2 = fp->readUint32();
		_sssSamplesData[i].codeOffset3 = fp->readUint32();
		_sssSamplesData[i].codeOffset4 = fp->readUint32();
		debug(kDebug_RESOURCE, "SssSample #%d pcm %d frames %d", i, _sssSamplesData[i].pcm, _sssSamplesData[i].framesCount);
		bytesRead += 24;
	}
	_sssCodeData = (uint8_t *)malloc(_sssHdr.codeSize);
	fp->read(_sssCodeData, _sssHdr.codeSize);
	bytesRead += _sssHdr.codeSize;
	if (_sssHdr.version == 10 || _sssHdr.version == 12) {

		// _sssPreloadData1
		fp->seek(_sssHdr.preloadData1Count * 4, SEEK_CUR);
		bytesRead += _sssHdr.preloadData1Count * 4;
		// _sssPreloadData2
		fp->seek(_sssHdr.preloadData2Count * 4, SEEK_CUR);
		bytesRead += _sssHdr.preloadData2Count * 4;
		// _sssPreloadData3
		fp->seek(_sssHdr.preloadData3Count * 4, SEEK_CUR);
		bytesRead += _sssHdr.preloadData3Count * 4;

		_sssPreload1Table.allocate(_sssHdr.preloadData1Count);
		const bool is16Bits = (_sssHdr.version == 12);
		for (int i = 0; i < _sssHdr.preloadData1Count; ++i) {
			const int count = is16Bits ? fp->readUint16() : fp->readByte();
			debug(kDebug_RESOURCE, "sssPreloadData1 #%d count %d", i, count);
			_sssPreload1Table[i].count = count;
			const int tableSize = is16Bits ? count * 2 : count;
			_sssPreload1Table[i].ptr = (uint8_t *)malloc(tableSize);
			fp->read(_sssPreload1Table[i].ptr, tableSize);
			bytesRead += tableSize + (is16Bits ? 2 : 1);
		}
		for (int i = 0; i < _sssHdr.preloadData2Count; ++i) {
			const int count = fp->readByte();
			fp->seek(count, SEEK_CUR);
			debug(kDebug_RESOURCE, "sssPreloadData2 #%d count %d", i, count);
			bytesRead += count + 1;
		}
		for (int i = 0; i < _sssHdr.preloadData3Count; ++i) {
			const int count = fp->readByte();
			fp->seek(count, SEEK_CUR);
			debug(kDebug_RESOURCE, "sssPreloadData3 #%d count %d", i, count);
			bytesRead += count + 1;
		}
		{
			const int count = fp->readByte();
			fp->seek(count, SEEK_CUR);
			bytesRead += count + 1;
		}
		// _sssPreloadInfosData = data;
	}
	// data += _sssHdr.preloadInfoCount * 8;
	_sssPreloadInfosData.allocate(_sssHdr.preloadInfoCount);
	for (int i = 0; i < _sssHdr.preloadInfoCount; ++i) {
		_sssPreloadInfosData[i].count = fp->readUint32();
		fp->readUint32();
		debug(kDebug_RESOURCE, "_sssPreloadInfosData #%d/%d count %d offset 0x%x", i, _sssHdr.preloadInfoCount, _sssPreloadInfosData[i].count);
		bytesRead += 8;
	}
	if (_sssHdr.version == 10 || _sssHdr.version == 12) {
		static const int kSizeOfPreloadInfoData_V10 = 32;
		for (int i = 0; i < _sssHdr.preloadInfoCount; ++i) {
			const int count = _sssPreloadInfosData[i].count;
			_sssPreloadInfosData[i].data = (SssPreloadInfoData *)malloc(count * sizeof(SssPreloadInfoData));
			for (int j = 0; j < count; ++j) {
				SssPreloadInfoData *preloadInfoData = &_sssPreloadInfosData[i].data[j];
				preloadInfoData->pcmBlockOffset = fp->readUint16();
				preloadInfoData->pcmBlockSize = fp->readUint16();
				fp->seek(12, SEEK_CUR);
				preloadInfoData->screenNum = fp->readByte();
				const int preload3Index = fp->readByte(); // mst
				assert(preload3Index < _sssHdr.preloadData3Count);
				preloadInfoData->preload3Index = preload3Index;
				const int preload1Index = fp->readByte(); // pcm
				assert(preload1Index < _sssHdr.preloadData1Count);
				preloadInfoData->preload1Index = preload1Index;
				const int preload2Index = fp->readByte(); // lvl
				assert(preload2Index < _sssHdr.preloadData2Count);
				preloadInfoData->preload2Index = preload2Index;
				fp->seek(8, SEEK_CUR);
				preloadInfoData->unk1C = fp->readUint32();
				bytesRead += kSizeOfPreloadInfoData_V10;
			}
			for (int j = 0; j < count; ++j) {
				const int len = _sssPreloadInfosData[i].data[j].unk1C * 4;
				bytesRead += skipBytesAlign(fp, len);
			}
		}
	} else if (_sssHdr.version == 6) {
		static const int kSizeOfPreloadInfoData_V6 = 68;
		for (int i = 0; i < _sssHdr.preloadInfoCount; ++i) {
			const int count = _sssPreloadInfosData[i].count;
			uint8_t *p = (uint8_t *)malloc(kSizeOfPreloadInfoData_V6 * count);
			fp->read(p, kSizeOfPreloadInfoData_V6 * count);
			bytesRead += kSizeOfPreloadInfoData_V6 * count;
			for (int j = 0; j < count; ++j) {
				static const int8_t offsets[8] = { 0x2C, 0x30, 0x34, 0x04, 0x08, 0x0C, 0x10, 0x14 };
				static const int8_t lengths[8] = {    2,    1,    1,    2,    2,    1,    1,    1 };
				for (int k = 0; k < 8; ++k) {
					const int len = READ_LE_UINT32(p + j * kSizeOfPreloadInfoData_V6 + offsets[k]) * lengths[k];
					bytesRead += skipBytesAlign(fp, len);
				}
			}
			free(p);
		}
		_sssPreloadInfosData.deallocate();
		_sssHdr.preloadInfoCount = 0;
	}

	_sssPcmTable.allocate(_sssHdr.pcmCount);
	uint32_t sssPcmOffset = baseOffset;
	for (int i = 0; i < _sssHdr.pcmCount; ++i) {
		_sssPcmTable[i].ptr = 0; fp->skipUint32();
		_sssPcmTable[i].offset = fp->readUint32();
		_sssPcmTable[i].totalSize = fp->readUint32();
		_sssPcmTable[i].strideSize = fp->readUint32();
		_sssPcmTable[i].strideCount = fp->readUint16();
		_sssPcmTable[i].flags = fp->readUint16();
		debug(kDebug_RESOURCE, "sssPcmTable #%d/%d offset 0x%x size %d", i, _sssHdr.pcmCount, _sssPcmTable[i].offset, _sssPcmTable[i].totalSize);
		if (_sssPcmTable[i].totalSize != 0) {
			assert((_sssPcmTable[i].totalSize % _sssPcmTable[i].strideSize) == 0);
			assert(_sssPcmTable[i].totalSize == _sssPcmTable[i].strideSize * _sssPcmTable[i].strideCount);
			if (sssPcmOffset != 0) {
				assert(fp != _datFile || _sssPcmTable[i].offset == 0x2800); // .dat
				_sssPcmTable[i].offset += sssPcmOffset;
				sssPcmOffset += _sssPcmTable[i].totalSize;
			}
		}
		bytesRead += 20;
	}

	// allocate structure but skip read as table is cleared and initialized in clearSoundObjects()
	static const int kSizeOfSssFilter = 52;
	fp->seek(_sssHdr.filtersDataCount * kSizeOfSssFilter, SEEK_CUR);
	_sssFilters.allocate(_sssHdr.filtersDataCount);
	bytesRead += _sssHdr.filtersDataCount * kSizeOfSssFilter;

	_sssDataUnk6.allocate(_sssHdr.banksDataCount);
	for (int i = 0; i < _sssHdr.banksDataCount; ++i) {
		_sssDataUnk6[i].unk0[0] = fp->readUint32();
		_sssDataUnk6[i].unk0[1] = fp->readUint32();
		_sssDataUnk6[i].unk0[2] = fp->readUint32();
		_sssDataUnk6[i].unk0[3] = fp->readUint32();
		_sssDataUnk6[i].mask    = fp->readUint32();
		bytesRead += 20;
		debug(kDebug_RESOURCE, "sssDataUnk6 #%d/%d unk10 0x%x", i, _sssHdr.banksDataCount);
	}

	const int lutSize = _sssHdr.banksDataCount * sizeof(uint32_t);
	for (int i = 0; i < 3; ++i) {
		// allocate structures but skip read as tables are initialized in clearSoundObjects()
		_sssGroup1[i] = (uint32_t *)malloc(lutSize);
		_sssGroup2[i] = (uint32_t *)malloc(lutSize);
		_sssGroup3[i] = (uint32_t *)malloc(lutSize);
		fp->seek(lutSize * 3, SEEK_CUR);
		bytesRead += lutSize * 3;
	}
	// _sssPreloadedPcmTotalSize = 0;

	checkSssCode(_sssCodeData, _sssHdr.codeSize);
	for (int i = 0; i < _sssHdr.banksDataCount; ++i) {
		if (_sssBanksData[i].count != 0) {
			// const int num = _sssBanksData[i].firstSampleIndex;
			// _sssBanksData[i].codeOffset = &_sssSamplesData[num];
		} else {
			_sssBanksData[i].firstSampleIndex = kNone;
		}
	}
	// fixup _sssSamplesData[i].codeOffset
	debug(kDebug_RESOURCE, "bufferSize %d bytesRead %d", bufferSize, bytesRead);
	if (bufferSize != bytesRead) {
		error("Unexpected number of bytes read %d (%d)", bytesRead, bufferSize);
	}
	// preload PCM (_sssHdr.preloadPcmCount or setup.dat)
	if (fp == _datFile) {
		fp->seek(_sssPcmTable[0].offset, SEEK_SET);
		for (int i = 0; i < _sssHdr.pcmCount; ++i) {
			loadSssPcm(fp, &_sssPcmTable[i]);
		}
	}
	for (int i = 0; i < _sssHdr.banksDataCount; ++i) {
		uint32_t mask = 1;
		_sssDataUnk6[i].mask = 0;
		const SssSample *codeOffset = &_sssSamplesData[_sssBanksData[i].firstSampleIndex];
		uint32_t *ptr = _sssDataUnk6[i].unk0;
		for (int j = 0; j < _sssBanksData[i].count; ++j) {
			for (int k = 0; k < codeOffset->unk5; ++k) {
				if (mask == 0) {
					break;
				}
				*ptr |= mask;
				mask <<= 1;
			}
			++codeOffset;
			_sssDataUnk6[i].mask |= *ptr;
			++ptr;
		}
	}
	resetSssFilters();
	// clearSoundObjects();
	clearSssGroup3();
}

void Resource::unloadSssData() {
	for (unsigned int i = 0; i < _sssPreload1Table.count; ++i) {
		free(_sssPreload1Table[i].ptr);
		_sssPreload1Table[i].ptr = 0;
	}
	for (unsigned int i = 0; i < _sssPreloadInfosData.count; ++i) {
		free(_sssPreloadInfosData[i].data);
		_sssPreloadInfosData[i].data = 0;
	}
	for (unsigned int i = 0; i < _sssPcmTable.count; ++i) {
		free(_sssPcmTable[i].ptr);
		_sssPcmTable[i].ptr = 0;
	}
	for (int i = 0; i < 3; ++i) {
		free(_sssGroup1[i]);
		_sssGroup1[i] = 0;
		free(_sssGroup2[i]);
		_sssGroup2[i] = 0;
		free(_sssGroup3[i]);
		_sssGroup3[i] = 0;
	}
	free(_sssCodeData);
	_sssCodeData = 0;
}

void Resource::checkSssCode(const uint8_t *buf, int size) const {
	static const uint8_t opcodesLength[] = {
		4, 0, 4, 0, 4, 12, 8, 0, 16, 4, 4, 4, 4, 8, 12, 0, 4, 8, 8, 4, 4, 4, 8, 4, 8, 4, 8, 16, 8, 4
	};
	int offset = 0;
	while (offset < size) {
		const int len = opcodesLength[buf[offset]];
		if (len == 0) {
			warning("Invalid .sss opcode %d", buf[offset]);
			break;
		}
		offset += len;
	}
	assert(offset == size);
}

static int8_t sext8(uint8_t x, int bits) {
	const int shift = 8 - bits;
	return ((int8_t)(x << shift)) >> shift;
}

static int _pcmL1, _pcmL0;

static void decodeSssSpuAdpcmUnit(const uint8_t *src, int16_t *dst) { // src: 16bytes, dst: 112bytes
	static const int16_t K0_1024[] = { 0, 960, 1840, 1568, 1952 };
	static const int16_t K1_1024[] = { 0,   0, -832, -880, -960 };
	const uint8_t param = *src++;
	const int shift = 12 - (param & 15);
	assert(shift >= 0);
	const int filter = param >> 4;
	assert(filter < 5);
	const int flag = *src++;
	assert(flag < 7);
	for (int i = 0; i < 14; ++i) {
		const uint8_t b = *src++;
		const int t1 = sext8(b & 15, 4);
		const int s1 = (t1 << shift) + ((_pcmL0 * K0_1024[filter] + _pcmL1 * K1_1024[filter] + 512) >> 10);
		_pcmL1 = _pcmL0;
		_pcmL0 = s1;
		dst[0] = dst[1] = CLIP(_pcmL0, -32768, 32767);
		dst += 2;
		const int t2 = sext8(b >> 4, 4);
		const int s2 = (t2 << shift) + ((_pcmL0 * K0_1024[filter] + _pcmL1 * K1_1024[filter] + 512) >> 10);
		_pcmL1 = _pcmL0;
		_pcmL0 = s2;
		dst[0] = dst[1] = CLIP(_pcmL0, -32768, 32767);
		dst += 2;
	}
}

uint32_t Resource::getSssPcmSize(const SssPcm *pcm) const {
	if (_isPsx) {
		assert(pcm->strideSize == 512);
		const uint32_t size = pcm->strideCount * 512;
		return (size / 16) * 56 * sizeof(int16_t);
	}
	return (pcm->strideSize - 256 * sizeof(int16_t)) * pcm->strideCount * sizeof(int16_t);
}

void Resource::loadSssPcm(File *fp, SssPcm *pcm) {
	assert(!pcm->ptr);
	if (pcm->totalSize != 0) {
		const uint32_t decompressedSize = getSssPcmSize(pcm);
		debug(kDebug_SOUND, "Loading PCM %p decompressedSize %d", pcm, decompressedSize);
		int16_t *p = (int16_t *)malloc(decompressedSize);
		if (!p) {
			warning("Failed to allocate %d bytes for PCM", decompressedSize);
			return;
		}
		pcm->ptr = p;
		if (_isPsx) {
			for (int i = 0; i < pcm->strideCount; ++i) {
				uint8_t buf[512];
				fp->read(buf, sizeof(buf));
				_pcmL1 = _pcmL0 = 0;
				for (int j = 0; j < 512; j += 16) {
					decodeSssSpuAdpcmUnit(buf + j, p);
					p += 56;
				}
			}
			return;
		}
		if (fp != _datFile) {
			fp->seek(pcm->offset, SEEK_SET);
		}
		for (int i = 0; i < pcm->strideCount; ++i) {
			uint8_t samples[256 * sizeof(int16_t)];
			fp->read(samples, sizeof(samples));
			for (unsigned int j = 256 * sizeof(int16_t); j < pcm->strideSize; ++j) {
				*p++ = READ_LE_UINT16(samples + fp->readByte() * sizeof(int16_t));
			}
		}
		assert((p - pcm->ptr) * sizeof(int16_t) == decompressedSize);
	}
}

void Resource::clearSssGroup3() {
	if (_sssHdr.banksDataCount != 0) {
		for (int i = 0; i < 3; ++i) {
			memset(_sssGroup3[i], 0, _sssHdr.banksDataCount * sizeof(uint32_t));
		}
	}
}

void Resource::resetSssFilters() {
	for (int i = 0; i < _sssHdr.filtersDataCount; ++i) {
		memset(&_sssFilters[i], 0, sizeof(SssFilter));
		const int volume = _sssDefaultsData[i].defaultVolume;
		_sssFilters[i].volumeCurrent = volume << 16;
		_sssFilters[i].volume = volume;
		const int panning = _sssDefaultsData[i].defaultPanning;
		_sssFilters[i].panningCurrent = panning << 16;
		_sssFilters[i].panning = panning;
		const int priority = _sssDefaultsData[i].defaultPriority;
		_sssFilters[i].priorityCurrent = priority;
		_sssFilters[i].priority = priority;
	}
}

void Resource::preloadSssPcmList(const SssPreloadInfoData *preloadInfoData) {
	File *fp = _sssFile;
	if (_isPsx) {
		const uint16_t offset = preloadInfoData->pcmBlockOffset;
		if (offset == 0xFFFF) {
			return;
		}
		_lvlFile->seek(offset * 2048, SEEK_SET);
		fp = _lvlFile;
	}
	const uint8_t num = preloadInfoData->preload1Index;
	const SssPreloadList *preloadList = &_sssPreload1Table[num];
	const bool is16Bits = (_sssHdr.version == 12);
	for (int i = 0; i < preloadList->count; ++i) {
		const int num = is16Bits ? READ_LE_UINT16(preloadList->ptr + i * 2) : preloadList->ptr[i];
		if (!_sssPcmTable[num].ptr) {
			loadSssPcm(fp, &_sssPcmTable[num]);
		} else if (_isPsx) {
			fp->seek(_sssPcmTable[num].strideCount * 512, SEEK_CUR);
		}
	}
}

void Resource::loadMstData(File *fp) {
	assert(fp == _mstFile);

	if (_mstHdr.dataSize != 0) {
		unloadMstData();
		_mstHdr.dataSize = 0;
	}

	_mstHdr.version = fp->readUint32();
	if (_mstHdr.version != 160) {
		warning("Unhandled .mst version %d", _mstHdr.version);
		closeDat(_fs, _mstFile);
		return;
	}

	_mstHdr.dataSize = fp->readUint32();
	_mstHdr.walkBoxDataCount = fp->readUint32();
	_mstHdr.walkCodeDataCount = fp->readUint32();
	_mstHdr.movingBoundsIndexDataCount = fp->readUint32();
	_mstHdr.levelCheckpointCodeDataCount = fp->readUint32();
	_mstHdr.screenAreaDataCount = fp->readUint32();
	_mstHdr.screenAreaIndexDataCount = fp->readUint32();
	_mstHdr.behaviorIndexDataCount = fp->readUint32();
	_mstHdr.monsterActionIndexDataCount = fp->readUint32();
	_mstHdr.walkPathDataCount = fp->readUint32();
	_mstHdr.infoMonster2Count = fp->readUint32();
	_mstHdr.behaviorDataCount = fp->readUint32();
	_mstHdr.attackBoxDataCount = fp->readUint32();
	_mstHdr.monsterActionDataCount = fp->readUint32();
	_mstHdr.infoMonster1Count = fp->readUint32();
	_mstHdr.movingBoundsDataCount = fp->readUint32();
	_mstHdr.shootDataCount = fp->readUint32();
	_mstHdr.shootIndexDataCount = fp->readUint32();
	_mstHdr.actionDirectionDataCount = fp->readUint32();
	_mstHdr.op223DataCount = fp->readUint32();
	_mstHdr.op226DataCount = fp->readUint32();
	_mstHdr.op227DataCount = fp->readUint32();
	_mstHdr.op234DataCount = fp->readUint32();
	_mstHdr.op2DataCount = fp->readUint32();
	_mstHdr.op197DataCount = fp->readUint32();
	_mstHdr.op211DataCount = fp->readUint32();
	_mstHdr.op240DataCount = fp->readUint32();
	_mstHdr.unk0x70 = fp->readUint32();
	_mstHdr.unk0x74 = fp->readUint32();
	_mstHdr.op204DataCount = fp->readUint32();
	_mstHdr.codeSize = fp->readUint32();
	_mstHdr.screensCount = fp->readUint32();
	debug(kDebug_RESOURCE, "_mstHdr.version %d _mstHdr.codeSize %d", _mstHdr.version, _mstHdr.codeSize);

	fp->seek(2048, SEEK_SET); // align to the next sector

	int bytesRead = 0;

	_mstPointOffsets.allocate(_mstHdr.screensCount);
	for (int i = 0; i < _mstHdr.screensCount; ++i) {
		_mstPointOffsets[i].xOffset = fp->readUint32();
		_mstPointOffsets[i].yOffset = fp->readUint32();
		bytesRead += 8;
	}

	_mstWalkBoxData.allocate(_mstHdr.walkBoxDataCount);
	for (int i = 0; i < _mstHdr.walkBoxDataCount; ++i) {
		_mstWalkBoxData[i].right  = fp->readUint32();
		_mstWalkBoxData[i].left   = fp->readUint32();
		_mstWalkBoxData[i].bottom = fp->readUint32();
		_mstWalkBoxData[i].top    = fp->readUint32();
		_mstWalkBoxData[i].flags[0] = fp->readByte();
		_mstWalkBoxData[i].flags[1] = fp->readByte();
		_mstWalkBoxData[i].flags[2] = fp->readByte();
		_mstWalkBoxData[i].flags[3] = fp->readByte();
		bytesRead += 20;
	}

	_mstWalkCodeData.allocate(_mstHdr.walkCodeDataCount);
	for (int i = 0; i < _mstHdr.walkCodeDataCount; ++i) {
		fp->skipUint32();
		_mstWalkCodeData[i].codeDataCount = fp->readUint32();
		_mstWalkCodeData[i].codeData = (uint32_t *)malloc(_mstWalkCodeData[i].codeDataCount * sizeof(uint32_t));
		fp->skipUint32();
		_mstWalkCodeData[i].indexDataCount = fp->readUint32();
		if (_mstWalkCodeData[i].indexDataCount != 0) {
			_mstWalkCodeData[i].indexData = (uint8_t *)malloc(_mstWalkCodeData[i].indexDataCount);
		} else {
			_mstWalkCodeData[i].indexData = 0;
		}
		bytesRead += 16;
	}
	for (int i = 0; i < _mstHdr.walkCodeDataCount; ++i) {
		for (uint32_t j = 0; j < _mstWalkCodeData[i].codeDataCount; ++j) {
			_mstWalkCodeData[i].codeData[j] = fp->readUint32();
			bytesRead += 4;
		}
		if (_mstWalkCodeData[i].indexDataCount != 0) {
			bytesRead += readBytesAlign(fp, _mstWalkCodeData[i].indexData, _mstWalkCodeData[i].indexDataCount);
		}
	}

	_mstMovingBoundsIndexData.allocate(_mstHdr.movingBoundsIndexDataCount);
	for (int i = 0; i < _mstHdr.movingBoundsIndexDataCount; ++i) {
		_mstMovingBoundsIndexData[i].indexUnk49 = fp->readUint32();
		_mstMovingBoundsIndexData[i].unk4 = fp->readUint32();
		_mstMovingBoundsIndexData[i].unk8 = fp->readUint32();
		bytesRead += 12;
	}

	_mstTickDelay    = fp->readUint32();
	_mstTickCodeData = fp->readUint32();
	bytesRead += 8;

	_mstLevelCheckpointCodeData.allocate(_mstHdr.levelCheckpointCodeDataCount);
	for (int i = 0; i < _mstHdr.levelCheckpointCodeDataCount; ++i) {
		_mstLevelCheckpointCodeData[i] = fp->readUint32();
		bytesRead += 4;
	}

	_mstScreenAreaData.allocate(_mstHdr.screenAreaDataCount);
	for (int i = 0; i < _mstHdr.screenAreaDataCount; ++i) {
		MstScreenArea *msac = &_mstScreenAreaData[i];
		msac->x1 = fp->readUint32();
		msac->x2 = fp->readUint32();
		msac->y1 = fp->readUint32();
		msac->y2 = fp->readUint32();
		msac->nextByPos = fp->readUint32();
		msac->prev = fp->readUint32();
		msac->nextByValue = fp->readUint32();
		msac->unk0x1C = fp->readByte();
		msac->unk0x1D = fp->readByte();
		msac->unk0x1E = fp->readUint16();
		msac->codeData = fp->readUint32();
		bytesRead += 36;
	}

	_mstScreenAreaByValueIndexData.allocate(_mstHdr.screenAreaIndexDataCount);
	for (int i = 0; i < _mstHdr.screenAreaIndexDataCount; ++i) {
		_mstScreenAreaByValueIndexData[i] = fp->readUint32();
		bytesRead += 4;
	}

	_mstScreenAreaByPosIndexData.allocate(_mstHdr.screensCount);
	for (int i = 0; i < _mstHdr.screensCount; ++i) {
		_mstScreenAreaByPosIndexData[i] = fp->readUint32();
		bytesRead += 4;
	}

	_mstUnk41.allocate(_mstHdr.screensCount);
	for (int i = 0; i < _mstHdr.screensCount; ++i) {
		_mstUnk41[i] = fp->readUint32();
		bytesRead += 4;
	}

	_mstBehaviorIndexData.allocate(_mstHdr.behaviorIndexDataCount);
	for (int i = 0; i < _mstHdr.behaviorIndexDataCount; ++i) {
		fp->skipUint32();
		_mstBehaviorIndexData[i].count1 = fp->readUint32();
		_mstBehaviorIndexData[i].behavior = (uint32_t *)malloc(_mstBehaviorIndexData[i].count1 * sizeof(uint32_t));
		fp->skipUint32();
		_mstBehaviorIndexData[i].dataCount = fp->readUint32();
		if (_mstBehaviorIndexData[i].dataCount != 0) {
			_mstBehaviorIndexData[i].data = (uint8_t *)malloc(_mstBehaviorIndexData[i].dataCount);
		} else {
			_mstBehaviorIndexData[i].data = 0;
		}
		bytesRead += 16;
	}
	for (int i = 0; i < _mstHdr.behaviorIndexDataCount; ++i) {
		for (uint32_t j = 0; j < _mstBehaviorIndexData[i].count1; ++j) {
			_mstBehaviorIndexData[i].behavior[j] = fp->readUint32();
			bytesRead += 4;
		}
		if (_mstBehaviorIndexData[i].dataCount != 0) {
			bytesRead += readBytesAlign(fp, _mstBehaviorIndexData[i].data, _mstBehaviorIndexData[i].dataCount);
		}
	}

	_mstMonsterActionIndexData.allocate(_mstHdr.monsterActionIndexDataCount);
	for (int i = 0; i < _mstHdr.monsterActionIndexDataCount; ++i) {
		fp->skipUint32();
		_mstMonsterActionIndexData[i].count1 = fp->readUint32();
		_mstMonsterActionIndexData[i].indexUnk48 = (uint32_t *)malloc(_mstMonsterActionIndexData[i].count1 * sizeof(uint32_t));
		fp->skipUint32();
		_mstMonsterActionIndexData[i].dataCount = fp->readUint32();
		if (_mstMonsterActionIndexData[i].dataCount != 0) {
			_mstMonsterActionIndexData[i].data = (uint8_t *)malloc(_mstMonsterActionIndexData[i].dataCount);
		} else {
			_mstMonsterActionIndexData[i].data = 0;
		}
		bytesRead += 16;
	}
	for (int i = 0; i < _mstHdr.monsterActionIndexDataCount; ++i) {
		for (uint32_t j = 0; j < _mstMonsterActionIndexData[i].count1; ++j) {
			_mstMonsterActionIndexData[i].indexUnk48[j] = fp->readUint32();
			bytesRead += 4;
		}
		if (_mstMonsterActionIndexData[i].dataCount != 0) {
			bytesRead += readBytesAlign(fp, _mstMonsterActionIndexData[i].data, _mstMonsterActionIndexData[i].dataCount);
		}
	}

	_mstWalkPathData.allocate(_mstHdr.walkPathDataCount);
	for (int i = 0; i < _mstHdr.walkPathDataCount; ++i) {
		fp->skipUint32();
		fp->skipUint32();
		_mstWalkPathData[i].mask  = fp->readUint32();
		_mstWalkPathData[i].count = fp->readUint32();
		bytesRead += 16;
	}
	for (int i = 0; i < _mstHdr.walkPathDataCount; ++i) {
		const int count = _mstWalkPathData[i].count;
		_mstWalkPathData[i].data = (MstWalkNode *)malloc(sizeof(MstWalkNode) * count);
		for (int j = 0; j < count; ++j) {
			uint8_t data[104];
			fp->read(data, sizeof(data));
			bytesRead += 104;
			_mstWalkPathData[i].data[j].x1 = READ_LE_UINT32(data);
			_mstWalkPathData[i].data[j].x2 = READ_LE_UINT32(data + 4);
			_mstWalkPathData[i].data[j].y1 = READ_LE_UINT32(data + 8);
			_mstWalkPathData[i].data[j].y2 = READ_LE_UINT32(data + 12);
			_mstWalkPathData[i].data[j].walkBox = READ_LE_UINT32(data + 16); // sizeof == 20
			_mstWalkPathData[i].data[j].walkCodeStage1 = READ_LE_UINT32(data + 20); // sizeof == 16
			_mstWalkPathData[i].data[j].walkCodeStage2 = READ_LE_UINT32(data + 24); // sizeof == 16
			_mstWalkPathData[i].data[j].movingBoundsIndex1 = READ_LE_UINT32(data + 28); // sizeof == 12
			_mstWalkPathData[i].data[j].movingBoundsIndex2 = READ_LE_UINT32(data + 32); // sizeof == 12
			_mstWalkPathData[i].data[j].walkCodeReset[0] = READ_LE_UINT32(data + 36); // sizeof == 16
			_mstWalkPathData[i].data[j].walkCodeReset[1] = READ_LE_UINT32(data + 40); // sizeof == 16
			for (int k = 0; k < 4; ++k) {
				_mstWalkPathData[i].data[j].coords[k][0] = READ_LE_UINT32(data + 0x2C + k * 8);
				_mstWalkPathData[i].data[j].coords[k][1] = READ_LE_UINT32(data + 0x30 + k * 8);
			}
			_mstWalkPathData[i].data[j].neighborWalkNode[0] = READ_LE_UINT32(data + 76); // sizeof == 104
			_mstWalkPathData[i].data[j].neighborWalkNode[1] = READ_LE_UINT32(data + 80); // sizeof == 104
			_mstWalkPathData[i].data[j].neighborWalkNode[2] = READ_LE_UINT32(data + 84); // sizeof == 104
			_mstWalkPathData[i].data[j].neighborWalkNode[3] = READ_LE_UINT32(data + 88); // sizeof == 104
			_mstWalkPathData[i].data[j].nextWalkNode = READ_LE_UINT32(data + 92); // sizeof == 104
			if (count != 0) {
				_mstWalkPathData[i].data[j].unk60[0] = (uint8_t *)malloc(count);
				_mstWalkPathData[i].data[j].unk60[1] = (uint8_t *)malloc(count);
			} else {
				_mstWalkPathData[i].data[j].unk60[0] = 0;
				_mstWalkPathData[i].data[j].unk60[1] = 0;
			}
		}
		_mstWalkPathData[i].walkNodeData = (uint32_t *)malloc(_mstHdr.screensCount * sizeof(uint32_t));
		for (int j = 0; j < _mstHdr.screensCount; ++j) {
			_mstWalkPathData[i].walkNodeData[j] = fp->readUint32();
			bytesRead += 4;
		}
		for (int j = 0; j < count; ++j) {
			for (int k = 0; k < 2; ++k) {
				bytesRead += readBytesAlign(fp, _mstWalkPathData[i].data[j].unk60[k], count);
			}
		}
	}

	_mstInfoMonster2Data.allocate(_mstHdr.infoMonster2Count);
	for (int i = 0; i < _mstHdr.infoMonster2Count; ++i) {
		_mstInfoMonster2Data[i].type = fp->readByte();
		_mstInfoMonster2Data[i].shootMask = fp->readByte();
		_mstInfoMonster2Data[i].anim = fp->readUint16();
		_mstInfoMonster2Data[i].codeData = fp->readUint32();
		_mstInfoMonster2Data[i].codeData2 = fp->readUint32();
		bytesRead += 12;
	}

	_mstBehaviorData.allocate(_mstHdr.behaviorDataCount);
	for (int i = 0; i < _mstHdr.behaviorDataCount; ++i) {
		fp->skipUint32();
		_mstBehaviorData[i].count = fp->readUint32();
		bytesRead += 8;
	}
	for (int i = 0; i < _mstHdr.behaviorDataCount; ++i) {
		_mstBehaviorData[i].data  = (MstBehaviorState *)malloc(_mstBehaviorData[i].count * sizeof(MstBehaviorState));
		for (uint32_t j = 0; j < _mstBehaviorData[i].count; ++j) {
			uint8_t data[44];
			fp->read(data, sizeof(data));
			bytesRead += 44;
			_mstBehaviorData[i].data[j].indexMonsterInfo = READ_LE_UINT32(data);
			_mstBehaviorData[i].data[j].anim        = READ_LE_UINT16(data + 0x04);
			_mstBehaviorData[i].data[j].unk6        = READ_LE_UINT16(data + 0x06);
			_mstBehaviorData[i].data[j].unk8        = READ_LE_UINT32(data + 0x08);
			_mstBehaviorData[i].data[j].energy      = READ_LE_UINT32(data + 0x0C);
			_mstBehaviorData[i].data[j].unk10       = READ_LE_UINT32(data + 0x10);
			_mstBehaviorData[i].data[j].count       = READ_LE_UINT32(data + 0x14);
			_mstBehaviorData[i].data[j].unk18       = READ_LE_UINT32(data + 0x18);
			_mstBehaviorData[i].data[j].indexUnk51  = READ_LE_UINT32(data + 0x1C);
			_mstBehaviorData[i].data[j].walkPath    = READ_LE_UINT32(data + 0x20);
			_mstBehaviorData[i].data[j].attackBox   = READ_LE_UINT32(data + 0x24);
			_mstBehaviorData[i].data[j].codeData    = READ_LE_UINT32(data + 0x28);
		}
	}

	_mstAttackBoxData.allocate(_mstHdr.attackBoxDataCount);
	for (int i = 0; i < _mstHdr.attackBoxDataCount; ++i) {
		fp->skipUint32();
		_mstAttackBoxData[i].count = fp->readUint32();
		bytesRead += 8;
	}
	for (int i = 0; i < _mstHdr.attackBoxDataCount; ++i) {
		_mstAttackBoxData[i].data = (uint8_t *)malloc(_mstAttackBoxData[i].count * 20);
		fp->read(_mstAttackBoxData[i].data, _mstAttackBoxData[i].count * 20);
		bytesRead += _mstAttackBoxData[i].count * 20;
	}

	_mstMonsterActionData.allocate(_mstHdr.monsterActionDataCount);
	for (int i = 0; i < _mstHdr.monsterActionDataCount; ++i) {
		MstMonsterAction *m = &_mstMonsterActionData[i];
		m->unk0 = fp->readUint16();
		m->unk2 = fp->readUint16();
		m->unk4 = fp->readByte();
		m->direction = fp->readByte();
		m->unk6 = fp->readByte();
		m->unk7 = fp->readByte();
		m->codeData = fp->readUint32();
		m->area = 0; fp->skipUint32();
		m->areaCount = fp->readUint32();
		fp->seek(16, SEEK_CUR);
		m->count[0] = fp->readUint32();
		m->count[1] = fp->readUint32();
		bytesRead += 44;
	}
	for (int i = 0; i < _mstHdr.monsterActionDataCount; ++i) {
		MstMonsterAction *m = &_mstMonsterActionData[i];
		for (int j = 0; j < 2; ++j) {
			const int count = m->count[j];
			if (count != 0) {
				m->data1[j] = (uint32_t *)malloc(count * sizeof(uint32_t));
				for (int k = 0; k < count; ++k) {
					m->data1[j][k] = fp->readUint32();
				}
				bytesRead += count * 4;
				m->data2[j] = (uint32_t *)malloc(count * sizeof(uint32_t));
				for (int k = 0; k < count; ++k) {
					m->data2[j][k] = fp->readUint32();
				}
				bytesRead += count * 4;
			} else {
				m->data1[j] = 0;
				m->data2[j] = 0;
			}
		}
		MstMonsterArea *m12 = (MstMonsterArea *)malloc(m->areaCount * sizeof(MstMonsterArea));
		for (int j = 0; j < m->areaCount; ++j) {
			m12[j].unk0  = fp->readByte();
			fp->skipByte();
			fp->skipByte();
			fp->skipByte();
			m12[j].data  = 0; fp->skipUint32();
			m12[j].count = fp->readUint32();
			bytesRead += 12;
		}
		for (int j = 0; j < m->areaCount; ++j) {
			m12[j].data = (MstMonsterAreaAction *)malloc(m12[j].count * sizeof(MstMonsterAreaAction));
			for (uint32_t k = 0; k < m12[j].count; ++k) {
				uint8_t data[28];
				fp->read(data, sizeof(data));
				m12[j].data[k].unk0 = READ_LE_UINT32(data);
				m12[j].data[k].indexUnk51 = READ_LE_UINT32(data + 0x4);
				m12[j].data[k].xPos = READ_LE_UINT32(data + 0x8);
				m12[j].data[k].yPos = READ_LE_UINT32(data + 0xC);
				m12[j].data[k].codeData = READ_LE_UINT32(data + 0x10);
				m12[j].data[k].codeData2 = READ_LE_UINT32(data + 0x14);
				m12[j].data[k].unk18 = data[0x18];
				m12[j].data[k].direction = data[0x19];
				m12[j].data[k].screenNum = data[0x1A];
				m12[j].data[k].monster1Index = data[0x1B];
				bytesRead += 28;
			}
		}
		m->area = m12;
	}

	const int mapDataSize = _mstHdr.infoMonster1Count * kMonsterInfoDataSize;
	_mstMonsterInfos = (uint8_t *)malloc(mapDataSize);
	fp->read(_mstMonsterInfos, mapDataSize);
	bytesRead += mapDataSize;

	_mstMovingBoundsData.allocate(_mstHdr.movingBoundsDataCount);
	for (int i = 0; i < _mstHdr.movingBoundsDataCount; ++i) {
		_mstMovingBoundsData[i].indexMonsterInfo = fp->readUint32();
		fp->skipUint32();
		_mstMovingBoundsData[i].count1  = fp->readUint32();
		fp->skipUint32();
		_mstMovingBoundsData[i].indexDataCount = fp->readUint32();
		_mstMovingBoundsData[i].unk14   = fp->readByte();
		_mstMovingBoundsData[i].unk15   = fp->readByte();
		_mstMovingBoundsData[i].unk16   = fp->readByte();
		_mstMovingBoundsData[i].unk17   = fp->readByte();
		bytesRead += 24;
	}
	for (int i = 0; i < _mstHdr.movingBoundsDataCount; ++i) {
		_mstMovingBoundsData[i].data1 = (MstMovingBoundsUnk1 *)malloc(_mstMovingBoundsData[i].count1 * sizeof(MstMovingBoundsUnk1));
		const int start = _mstMovingBoundsData[i].indexMonsterInfo;
		assert(start < _mstHdr.infoMonster1Count);
		for (uint32_t j = 0; j < _mstMovingBoundsData[i].count1; ++j) {
			fp->skipUint32();
			_mstMovingBoundsData[i].data1[j].unk4 = fp->readUint32();
			_mstMovingBoundsData[i].data1[j].unk8 = fp->readByte();
			_mstMovingBoundsData[i].data1[j].unk9 = fp->readByte();
			_mstMovingBoundsData[i].data1[j].unkA = fp->readByte();
			_mstMovingBoundsData[i].data1[j].unkB = fp->readByte();
			_mstMovingBoundsData[i].data1[j].unkC = fp->readByte();
			_mstMovingBoundsData[i].data1[j].unkD = fp->readByte();
			_mstMovingBoundsData[i].data1[j].unkE = fp->readByte();
			_mstMovingBoundsData[i].data1[j].unkF = fp->readByte();
			const uint32_t num = _mstMovingBoundsData[i].data1[j].unk4;
			assert(num < 32);
			_mstMovingBoundsData[i].data1[j].offsetMonsterInfo = start * kMonsterInfoDataSize + num * 28;
			bytesRead += 16;
		}
		if (_mstMovingBoundsData[i].indexDataCount != 0) {
			_mstMovingBoundsData[i].indexData = (uint8_t *)malloc(_mstMovingBoundsData[i].indexDataCount);
			fp->read(_mstMovingBoundsData[i].indexData, _mstMovingBoundsData[i].indexDataCount);
			bytesRead += _mstMovingBoundsData[i].indexDataCount;
		} else {
			_mstMovingBoundsData[i].indexData = 0;
		}
	}

	_mstShootData.allocate(_mstHdr.shootDataCount);
	for (int i = 0; i < _mstHdr.shootDataCount; ++i) {
		_mstShootData[i].data  = 0; fp->skipUint32();
		_mstShootData[i].count = fp->readUint32();
		bytesRead += 8;
	}
	for (int i = 0; i < _mstHdr.shootDataCount; ++i) {
		_mstShootData[i].data = (MstShootAction *)malloc(_mstShootData[i].count * sizeof(MstShootAction));
		for (uint32_t j = 0; j < _mstShootData[i].count; ++j) {
			_mstShootData[i].data[j].codeData = fp->readUint32();
			_mstShootData[i].data[j].unk4 = fp->readUint32();
			_mstShootData[i].data[j].unk8 = fp->readUint32();
			_mstShootData[i].data[j].xPos = fp->readUint32();
			_mstShootData[i].data[j].yPos = fp->readUint32();
			_mstShootData[i].data[j].width = fp->readUint32();
			_mstShootData[i].data[j].height = fp->readUint32();
			_mstShootData[i].data[j].hSize = fp->readUint32();
			_mstShootData[i].data[j].vSize = fp->readUint32();
			_mstShootData[i].data[j].unk24 = fp->readUint32();
			bytesRead += 40;
		}
	}

	_mstShootIndexData.allocate(_mstHdr.shootIndexDataCount);
	for (int i = 0; i < _mstHdr.shootIndexDataCount; ++i) {
		_mstShootIndexData[i].indexUnk50 = fp->readUint32();
		assert(_mstShootIndexData[i].indexUnk50 < (uint32_t)_mstHdr.shootDataCount);
		_mstShootIndexData[i].indexUnk50Unk1 = 0; fp->skipUint32();
		_mstShootIndexData[i].count = fp->readUint32();
		bytesRead += 12;
	}
	for (int i = 0; i < _mstHdr.shootIndexDataCount; ++i) {
		_mstShootIndexData[i].indexUnk50Unk1 = (uint32_t *)malloc(_mstShootIndexData[i].count * 9 * sizeof(uint32_t));
		for (uint32_t j = 0; j < _mstShootIndexData[i].count * 9; ++j) {
			_mstShootIndexData[i].indexUnk50Unk1[j] = fp->readUint32();
			assert(_mstShootIndexData[i].indexUnk50Unk1[j] < _mstShootData[_mstShootIndexData[i].indexUnk50].count);
			bytesRead += 4;
		}
	}
	_mstActionDirectionData.allocate(_mstHdr.actionDirectionDataCount);
	for (int i = 0; i < _mstHdr.actionDirectionDataCount; ++i) {
		_mstActionDirectionData[i].unk0 = fp->readByte();
		_mstActionDirectionData[i].unk1 = fp->readByte();
		_mstActionDirectionData[i].unk2 = fp->readByte();
		_mstActionDirectionData[i].unk3 = fp->readByte();
		bytesRead += 4;
	}

	_mstOp223Data.allocate(_mstHdr.op223DataCount);
	for (int i = 0; i < _mstHdr.op223DataCount; ++i) {
		_mstOp223Data[i].indexVar1 = fp->readUint16();
		_mstOp223Data[i].indexVar2 = fp->readUint16();
		_mstOp223Data[i].indexVar3 = fp->readUint16();
		_mstOp223Data[i].indexVar4 = fp->readUint16();
		_mstOp223Data[i].unk8      = fp->readByte();
		_mstOp223Data[i].unk9      = fp->readByte();
		_mstOp223Data[i].indexVar5 = fp->readByte();
		_mstOp223Data[i].unkB      = fp->readByte();
		_mstOp223Data[i].unkC      = fp->readUint16();
		_mstOp223Data[i].unkE      = fp->readUint16();
		_mstOp223Data[i].maskVars  = fp->readUint32();
		bytesRead += 20;
	}

	_mstOp226Data.allocate(_mstHdr.op226DataCount);
	for (int i = 0; i < _mstHdr.op226DataCount; ++i) {
		_mstOp226Data[i].unk0 = fp->readByte();
		_mstOp226Data[i].unk1 = fp->readByte();
		_mstOp226Data[i].unk2 = fp->readByte();
		_mstOp226Data[i].unk3 = fp->readByte();
		_mstOp226Data[i].unk4 = fp->readByte();
		_mstOp226Data[i].unk5 = fp->readByte();
		_mstOp226Data[i].unk6 = fp->readByte();
		_mstOp226Data[i].unk7 = fp->readByte();
		bytesRead += 8;
	}

	_mstOp227Data.allocate(_mstHdr.op227DataCount);
	for (int i = 0; i < _mstHdr.op227DataCount; ++i) {
		_mstOp227Data[i].indexVar1 = fp->readUint16();
		_mstOp227Data[i].indexVar2 = fp->readUint16();
		_mstOp227Data[i].compare   = fp->readByte();
		_mstOp227Data[i].maskVars  = fp->readByte();
		_mstOp227Data[i].codeData  = fp->readUint16();
		bytesRead += 8;
	}

	_mstOp234Data.allocate(_mstHdr.op234DataCount);
	for (int i = 0; i < _mstHdr.op234DataCount; ++i) {
		_mstOp234Data[i].indexVar1 = fp->readUint16();
		_mstOp234Data[i].indexVar2 = fp->readUint16();
		_mstOp234Data[i].compare   = fp->readByte();
		_mstOp234Data[i].maskVars  = fp->readByte();
		_mstOp234Data[i].codeData  = fp->readUint16();
		bytesRead += 8;
	}

	_mstOp2Data.allocate(_mstHdr.op2DataCount);
	for (int i = 0; i < _mstHdr.op2DataCount; ++i) {
		_mstOp2Data[i].indexVar1 = fp->readUint32();
		_mstOp2Data[i].indexVar2 = fp->readUint32();
		_mstOp2Data[i].maskVars  = fp->readByte();
		_mstOp2Data[i].unk9      = fp->readByte();
		_mstOp2Data[i].unkA      = fp->readByte();
		_mstOp2Data[i].unkB      = fp->readByte();
		bytesRead += 12;
	}

	_mstOp197Data.allocate(_mstHdr.op197DataCount);
	for (int i = 0; i < _mstHdr.op197DataCount; ++i) {
		_mstOp197Data[i].unk0 = fp->readUint16();
		_mstOp197Data[i].unk2 = fp->readUint16();
		_mstOp197Data[i].unk4 = fp->readUint16();
		_mstOp197Data[i].unk6 = fp->readUint16();
		_mstOp197Data[i].maskVars = fp->readUint32();
		_mstOp197Data[i].indexUnk49 = fp->readUint16();
		_mstOp197Data[i].unkE = fp->readByte();
		_mstOp197Data[i].unkF = fp->readByte();
		bytesRead += 16;
	}

	_mstOp211Data.allocate(_mstHdr.op211DataCount);
	for (int i = 0; i < _mstHdr.op211DataCount; ++i) {
		_mstOp211Data[i].indexVar1 = fp->readUint16();
		_mstOp211Data[i].indexVar2 = fp->readUint16();
		_mstOp211Data[i].unk4      = fp->readUint16();
		_mstOp211Data[i].unk6      = fp->readUint16();
		_mstOp211Data[i].unk8      = fp->readByte();
		_mstOp211Data[i].unk9      = fp->readByte();
		_mstOp211Data[i].unkA      = fp->readByte();
		_mstOp211Data[i].unkB      = fp->readByte();
		_mstOp211Data[i].indexVar3 = fp->readByte();
		_mstOp211Data[i].unkD      = fp->readByte();
		_mstOp211Data[i].maskVars  = fp->readUint16();
		bytesRead += 16;
	}

	_mstOp240Data.allocate(_mstHdr.op240DataCount);
	for (int i = 0; i < _mstHdr.op240DataCount; ++i) {
		_mstOp240Data[i].flags    = fp->readUint32();
		_mstOp240Data[i].codeData = fp->readUint32();
		bytesRead += 8;
	}

	_mstUnk60.allocate(_mstHdr.unk0x70);
	for (int i = 0; i < _mstHdr.unk0x70; ++i) {
		_mstUnk60[i] = fp->readUint32();
		bytesRead += 4;
	}

	fp->seek(_mstHdr.unk0x74 * 4, SEEK_CUR); // _mstUnk61
	bytesRead += _mstHdr.unk0x74 * 4;

	_mstOp204Data.allocate(_mstHdr.op204DataCount);
	for (int i = 0; i < _mstHdr.op204DataCount; ++i) {
		_mstOp204Data[i].arg0 = fp->readUint32();
		_mstOp204Data[i].arg1 = fp->readUint32();
		_mstOp204Data[i].arg2 = fp->readUint32();
		_mstOp204Data[i].arg3 = fp->readUint32();
		bytesRead += 16;
	}

	_mstCodeData = (uint8_t *)malloc(_mstHdr.codeSize * 4);
	fp->read(_mstCodeData, _mstHdr.codeSize * 4);
	bytesRead += _mstHdr.codeSize * 4;

	if (bytesRead != _mstHdr.dataSize) {
		warning("Unexpected .mst bytesRead %d dataSize %d", bytesRead, _mstHdr.dataSize);
	}
}

void Resource::unloadMstData() {
	for (int i = 0; i < _mstHdr.walkCodeDataCount; ++i) {
		free(_mstWalkCodeData[i].codeData);
		_mstWalkCodeData[i].codeData = 0;
		free(_mstWalkCodeData[i].indexData);
		_mstWalkCodeData[i].indexData = 0;
	}
	for (int i = 0; i < _mstHdr.behaviorIndexDataCount; ++i) {
		free(_mstBehaviorIndexData[i].behavior);
		_mstBehaviorIndexData[i].behavior = 0;
		free(_mstBehaviorIndexData[i].data);
		_mstBehaviorIndexData[i].data = 0;
	}
	for (int i = 0; i < _mstHdr.monsterActionIndexDataCount; ++i) {
		free(_mstMonsterActionIndexData[i].indexUnk48);
		_mstMonsterActionIndexData[i].indexUnk48 = 0;
		free(_mstMonsterActionIndexData[i].data);
		_mstMonsterActionIndexData[i].data = 0;
	}
	for (int i = 0; i < _mstHdr.walkPathDataCount; ++i) {
		free(_mstWalkPathData[i].data);
		_mstWalkPathData[i].data = 0;
		free(_mstWalkPathData[i].walkNodeData);
		_mstWalkPathData[i].walkNodeData = 0;
	}
	for (int i = 0; i < _mstHdr.behaviorDataCount; ++i) {
		free(_mstBehaviorData[i].data);
		_mstBehaviorData[i].data = 0;
	}
	for (int i = 0; i < _mstHdr.attackBoxDataCount; ++i) {
		free(_mstAttackBoxData[i].data);
		_mstAttackBoxData[i].data = 0;
	}
	for (int i = 0; i < _mstHdr.monsterActionDataCount; ++i) {
		MstMonsterAction *m = &_mstMonsterActionData[i];
		for (int j = 0; j < 2; ++j) {
			free(m->data1[j]);
			m->data1[j] = 0;
			free(m->data2[j]);
			m->data2[j] = 0;
		}
		free(m->area);
		m->area = 0;
	}

	free(_mstMonsterInfos);
	_mstMonsterInfos = 0;

	for (int i = 0; i < _mstHdr.movingBoundsDataCount; ++i) {
		free(_mstMovingBoundsData[i].data1);
		_mstMovingBoundsData[i].data1 = 0;
		free(_mstMovingBoundsData[i].indexData);
		_mstMovingBoundsData[i].indexData = 0;
	}
	for (int i = 0; i < _mstHdr.shootDataCount; ++i) {
		free(_mstShootData[i].data);
		_mstShootData[i].data = 0;
	}
	for (int i = 0; i < _mstHdr.shootIndexDataCount; ++i) {
		free(_mstShootIndexData[i].indexUnk50Unk1);
		_mstShootIndexData[i].indexUnk50Unk1 = 0;
	}

	free(_mstCodeData);
	_mstCodeData = 0;
}

const MstScreenArea *Resource::findMstCodeForPos(int num, int xPos, int yPos) const {
	uint32_t i = _mstScreenAreaByPosIndexData[num];
	while (i != kNone) {
		const MstScreenArea *msac = &_mstScreenAreaData[i];
		if (msac->x1 <= xPos && msac->x2 >= xPos && msac->unk0x1D != 0 && msac->y1 <= yPos && msac->y2 >= yPos) {
			return msac;
		}
		i = msac->nextByPos;
	}
	return 0;
}

void Resource::flagMstCodeForPos(int num, uint8_t value) {
	uint32_t i = _mstScreenAreaByValueIndexData[num];
	while (i != kNone) {
		MstScreenArea *msac = &_mstScreenAreaData[i];
		msac->unk0x1D = value;
		i = msac->nextByValue;
	}
}

enum {
	kModeSave,
	kModeLoad
};

static uint8_t _checksum;

static void persistUint8(FILE *fp, const uint8_t &val) {
	fputc(val, fp);
	_checksum ^= val;
}

static void persistUint8(FILE *fp, uint8_t &val) {
	val = fgetc(fp);
	_checksum ^= val;
}

static void persistUint32(FILE *fp, const uint32_t &val) {
	for (int i = 0; i < 4; ++i) {
		const uint8_t b = (val >> (i * 8)) & 0xFF;
		fputc(b, fp);
		_checksum ^= b;
	}
}

static void persistUint32(FILE *fp, uint32_t &val) {
	val = 0;
	for (int i = 0; i < 4; ++i) {
		const uint8_t b = fgetc(fp);
		val |= b << (i * 8);
		_checksum ^= b;
	}
}

template <int M, typename T>
static void persistSetupCfg(FILE *fp, T *config) {
	_checksum = 0;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 10; ++j) {
			persistUint8(fp, config->players[i].progress[j]);
		}
		persistUint8(fp, config->players[i].levelNum);
		persistUint8(fp, config->players[i].checkpointNum);
		persistUint32(fp, config->players[i].cutscenesMask);
		persistUint8(fp, config->players[i].difficulty);
		for (int j = 0; j < 32; ++j) {
			persistUint8(fp, config->players[i].controls[j]);
		}
		persistUint8(fp, config->players[i].stereo);
		persistUint8(fp, config->players[i].volume);
		persistUint8(fp, config->players[i].currentLevel);
	}
	persistUint8(fp, config->unkD0);
	persistUint8(fp, config->currentPlayer);
	const uint8_t checksum = _checksum;
	persistUint8(fp, config->unkD2);
	if (M == kModeSave) {
		persistUint8(fp, checksum);
	} else {
		persistUint8(fp, config->checksum);
		if (M == kModeLoad && checksum != config->checksum) {
			warning("Invalid checksum 0x%x (0x%x) for 'setup.cfg'", config->checksum, checksum);
		}
	}
}

bool Resource::writeSetupCfg(const SetupConfig *config) {
	FILE *fp = _fs->openSaveFile(_setupCfg, true);
	if (fp) {
		persistSetupCfg<kModeSave>(fp, config);
		_fs->closeFile(fp);
		return true;
	}
	warning("Failed to save '%s'", _setupCfg);
	return false;
}

bool Resource::readSetupCfg(SetupConfig *config) {
	FILE *fp = _fs->openSaveFile(_setupCfg, false);
	if (fp) {
		persistSetupCfg<kModeLoad>(fp, config);
		_fs->closeFile(fp);
		return true;
	}
	return false;
}
