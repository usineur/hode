/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef FILEIO_H__
#define FILEIO_H__

#include "intern.h"

struct File {

	FILE *_fp;

	File();
	virtual ~File();

	void setFp(FILE *fp);

	virtual void seekAlign(uint32_t pos);
	virtual void seek(int pos, int whence);
	virtual int read(uint8_t *ptr, int size);
	uint8_t readByte();
	uint16_t readUint16();
	uint32_t readUint32();

	void skipByte()   { seek(1, SEEK_CUR); }
	void skipUint16() { seek(2, SEEK_CUR); }
	void skipUint32() { seek(4, SEEK_CUR); }
};

struct SectorFile : File {

	enum {
		kFioBufferSize = 2048
	};

	uint8_t _buf[kFioBufferSize];
	int _bufPos;

	SectorFile();

	void refillBuffer(uint8_t *ptr = 0);
	virtual void seekAlign(uint32_t pos);
	virtual void seek(int pos, int whence);
	virtual int read(uint8_t *ptr, int size);
};

int fioAlignSizeTo2048(int size);
uint32_t fioUpdateCRC(uint32_t sum, const uint8_t *buf, uint32_t size);

#endif // FILEIO_H__

