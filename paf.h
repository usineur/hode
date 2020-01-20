/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef PAF_PLAYER_H__
#define PAF_PLAYER_H__

#include "intern.h"
#include "fileio.h"

struct PafHeader {
	uint32_t preloadFrameBlocksCount;
	uint32_t *frameBlocksCountTable;
	uint32_t *framesOffsetTable;
	uint32_t *frameBlocksOffsetTable;
	int32_t framesCount;
	int32_t frameBlocksCount;
	uint32_t startOffset;
	uint32_t readBufferSize;
	int32_t maxVideoFrameBlocksCount;
	int32_t maxAudioFrameBlocksCount;
};

// names taken from the PSX filenames
enum {
	kPafAnimation_intro = 0,
	kPafAnimation_cine14l = 1,
	kPafAnimation_rapt = 2,
	kPafAnimation_glisse = 3,
	kPafAnimation_meeting = 4,
	kPafAnimation_island = 5,
	kPafAnimation_islefall = 6,
	kPafAnimation_vicious = 7,
	kPafAnimation_together = 8,
	kPafAnimation_power = 9,
	kPafAniamtion_back = 10,
	kPafAnimation_dogfree1 = 11,
	kPafAnimation_dogfree2 = 12,
	kPafAnimation_meteor = 13,
	kPafAnimation_cookie = 14,
	kPafAnimation_plot = 15,
	kPafAnimation_puzzle = 16,
	kPafAnimation_lstpiece = 17,
	kPafAnimation_dogfall = 18,
	kPafAnimation_lastfall = 19,
	kPafAnimation_end = 20,
	kPafAnimation_cinema = 21,
	kPafAnimation_CanyonAndyFallingCannon = 22, // kPafAnimation_ghct
	kPafAnimation_CanyonAndyFalling = 23, // kPafAnimation_chute
	kPafAnimation_IslandAndyFalling = 24 // kPafAnimation_chute_i
};

struct FileSystem;

struct PafAudioQueue {
	int16_t *buffer; // stereo samples
	int offset, size;
	PafAudioQueue *next;
};

struct PafPlayer {

	enum {
		kMaxVideosCount = 50,
		kBufferBlockSize = 2048,
		kVideoWidth = 256,
		kVideoHeight = 192,
		kPageBufferSize = 256 * 256,
		kFramesPerSec = 10, // 10hz
		kAudioSamples = 2205,
		kAudioStrideSize = 4922 // 256 * sizeof(int16_t) + 2205 * 2
	};

	bool _skipCutscenes;
	FileSystem *_fs;
	File _file;
	int _videoNum;
	uint32_t _videoOffset;
	PafHeader _pafHdr;
	int _currentPageBuffer;
	uint8_t *_pageBuffers[4];
	uint8_t _paletteBuffer[256 * 3];
	uint8_t _bufferBlock[kBufferBlockSize];
	uint8_t *_demuxVideoFrameBlocks;
	uint8_t *_demuxAudioFrameBlocks;
	uint32_t _audioBufferOffsetRd;
	uint32_t _audioBufferOffsetWr;
	PafAudioQueue *_audioQueue, *_audioQueueTail;
	uint32_t _flushAudioSize;
	uint32_t _playedMask;

	PafPlayer(FileSystem *fs);
	~PafPlayer();

	void preload(int num);
	void play(int num);
	void unload(int num = -1);

	bool readPafHeader();
	uint32_t *readPafHeaderTable(int count);

	void decodeVideoFrame(const uint8_t *src);
	uint8_t *getVideoPageOffset(uint8_t a, uint8_t b);
	void decodeVideoFrameOp0(const uint8_t *base, const uint8_t *src, uint8_t code);
	void decodeVideoFrameOp1(const uint8_t *src);
	void decodeVideoFrameOp2(const uint8_t *src);
	void decodeVideoFrameOp4(const uint8_t *src);

	void decodeAudioFrame(const uint8_t *src, uint32_t offset, uint32_t size);

	void mix(int16_t *buf, int samples);
	void mainLoop();
};

#endif // PAF_PLAYER_H__
