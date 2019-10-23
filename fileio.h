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

	virtual void seekAlign(int pos);
	virtual void seek(int pos, int whence);
	virtual int read(uint8_t *ptr, int size);
	uint8_t readByte();
	uint16_t readUint16();
	uint32_t readUint32();
	virtual void flush();
};

struct SectorFile : File {

	enum {
		kFioBufferSize = 2048
	};

	uint8_t _buf[kFioBufferSize];
	int _bufPos;
	int _bufLen;

	SectorFile();

	void refillBuffer();
	virtual void seekAlign(int pos);
	virtual void seek(int pos, int whence);
	virtual int read(uint8_t *ptr, int size);
	virtual void flush();
};

int fioAlignSizeTo2048(int size);
uint32_t fioUpdateCRC(uint32_t sum, const uint8_t *buf, uint32_t size);
void fioDumpData(const char *filename, const uint8_t *buf, int size);
int fioReadData(const char *filename, uint8_t *buf, int size);

#endif // FILEIO_H__

