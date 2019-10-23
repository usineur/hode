/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <sys/param.h>
#include "fileio.h"
#include "util.h"

File::File()
	: _fp(0) {
}

File::~File() {
}

void File::setFp(FILE *fp) {
	_fp = fp;
}

void File::seekAlign(int pos) {
	fseek(_fp, pos, SEEK_SET);
}

void File::seek(int pos, int whence) {
	fseek(_fp, pos, whence);
}

int File::read(uint8_t *ptr, int size) {
	return fread(ptr, 1, size, _fp);
}

uint8_t File::readByte() {
	uint8_t buf;
	read(&buf, 1);
	return buf;
}

uint16_t File::readUint16() {
	uint8_t buf[2];
	read(buf, 2);
	return READ_LE_UINT16(buf);
}

uint32_t File::readUint32() {
	uint8_t buf[4];
	read(buf, 4);
	return READ_LE_UINT32(buf);
}

void File::flush() {
}

SectorFile::SectorFile() {
	memset(_buf, 0, sizeof(_buf));
	_bufPos = _bufLen = 0;
}

int fioAlignSizeTo2048(int size) {
	return ((size + 2043) / 2044) * 2048;
}

uint32_t fioUpdateCRC(uint32_t sum, const uint8_t *buf, uint32_t size) {
	assert((size & 3) == 0);
	size >>= 2;
	while (size--) {
		sum ^= READ_LE_UINT32(buf); buf += 4;
	}
	return sum;
}

void SectorFile::refillBuffer() {
	int size = fread(_buf, 1, 2048, _fp);
	if (size == 2048) {
		uint32_t crc = fioUpdateCRC(0, _buf, 2048);
		assert(crc == 0);
		size -= 4;
	}
	_bufPos = 0;
	_bufLen = size;
}

void SectorFile::seekAlign(int pos) {
	pos += (pos / 2048) * 4;
	const int alignPos = (pos / 2048) * 2048;
	fseek(_fp, alignPos, SEEK_SET);
	refillBuffer();
	const int skipCount = pos - alignPos;
	_bufPos += skipCount;
	_bufLen -= skipCount;
}

void SectorFile::seek(int pos, int whence) {
	if (whence == SEEK_SET) {
		assert((pos & 2047) == 0);
		_bufLen = _bufPos = 0;
		File::seek(pos, SEEK_SET);
	} else {
		assert(whence == SEEK_CUR && pos >= 0);
		if (pos < _bufLen) {
			_bufLen -= pos;
			_bufPos += pos;
		} else {
			pos -= _bufLen;
			const int count = fioAlignSizeTo2048(pos) / 2048;
			if (count > 1) {
				const int alignPos = (count - 1) * 2048;
				fseek(_fp, alignPos, SEEK_CUR);
			}
			refillBuffer();
			_bufPos = pos % 2044;
			_bufLen = 2044 - _bufPos;
		}
	}
}

int SectorFile::read(uint8_t *ptr, int size) {
	if (size >= _bufLen) {
		const int count = fioAlignSizeTo2048(size) / 2048;
		for (int i = 0; i < count; ++i) {
			memcpy(ptr, _buf + _bufPos, _bufLen);
			ptr += _bufLen;
			size -= _bufLen;
			refillBuffer();
			if (_bufLen == 0 || size < _bufLen) {
				break;
			}
		}
	}
	if (_bufLen != 0 && size != 0) {
		memcpy(ptr, _buf + _bufPos, size);
		_bufLen -= size;
		_bufPos += size;
	}
	return 0;
}

void SectorFile::flush() {
	const int currentPos = ftell(_fp);
	assert((currentPos & 2047) == 0);
	_bufLen = _bufPos = 0;
}

void fioDumpData(const char *filename, const uint8_t *data, int size) {
	FILE *fp = fopen(filename, "wb");
	if (fp) {
		const int count = fwrite(data, 1, size, fp);
		if (count != size) {
			warning("Failed to write %d bytes, count %d", size, count);
		}
		fclose(fp);
	}
}

int fioReadData(const char *filename, uint8_t *buf, int size) {
	FILE *fp = fopen(filename, "rb");
	if (fp) {
		const int count = fread(buf, 1, size, fp);
		if (count != size) {
			warning("Failed to read %d bytes, count %d", size, count);
		}
		fclose(fp);
		return count;
	}
	return 0;
}
