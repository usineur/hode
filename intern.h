/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef INTERN_H__
#define INTERN_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#ifdef _WIN32
#define le16toh(x) x
#define le32toh(x) x
#define htole16(x) x
#define htole32(x) x
#else
#include <endian.h>
#endif

#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))
#define PACKED __attribute__((packed))

inline uint16_t READ_LE_UINT16(const void *ptr) {
	if (1 && (((uintptr_t)ptr) & 1) != 0) {
		uint16_t value;
		memcpy(&value, ptr, sizeof(uint16_t));
		return le16toh(value);
	} else {
		return le16toh(*(const uint16_t *)ptr);
	}
}

inline uint32_t READ_LE_UINT32(const void *ptr) {
	if (1 && (((uintptr_t)ptr) & 3) != 0) {
		uint32_t value;
		memcpy(&value, ptr, sizeof(uint32_t));
		return le32toh(value);
	} else {
		return le32toh(*(const uint32_t *)ptr);
	}
}

inline void WRITE_LE_UINT16(void *ptr, uint16_t v) {
	if (1 && (((uintptr_t)ptr) & 1) != 0) {
		const uint16_t value = htole16(v);
		memcpy(ptr, &value, sizeof(uint16_t));
	} else {
		*((uint16_t *)ptr) = htole16(v);
	}
}

inline void WRITE_LE_UINT32(void *ptr, uint32_t v) {
	if (1 && (((uintptr_t)ptr) & 3) != 0) {
		const uint32_t value = htole32(v);
		memcpy(ptr, &value, sizeof(uint32_t));
	} else {
		*((uint32_t *)ptr) = htole32(v);
	}
}

#undef MIN
template<typename T>
inline T MIN(T v1, T v2) {
	return (v1 < v2) ? v1 : v2;
}

#undef MAX
template<typename T>
inline T MAX(T v1, T v2) {
	return (v1 > v2) ? v1 : v2;
}

template<typename T>
inline T ABS(T t) {
	return (t < 0) ? -t : t;
}

template<typename T>
inline T CLIP(T t, T tmin, T tmax) {
	return (t < tmin ? tmin : (t > tmax ? tmax : t));
}

template<typename T>
inline void SWAP(T &a, T &b) {
	T tmp = a; a = b; b = tmp;
}

inline int merge_bits(int dbit, int sbit, int mask) {
	return ((sbit ^ dbit) & mask) ^ dbit;
//	return (dbit & ~mask) | (sbit & mask);
}

inline bool rect_contains(int left, int top, int right, int bottom, int x, int y) {
	return x >= left && x <= right && y >= top && y <= bottom;
}

inline bool rect_intersects(int left1, int top1, int right1, int bottom1, int left2, int top2, int right2, int bottom2) {
	return right2 >= left1 && left2 <= right1 && bottom2 >= top1 && top2 <= bottom1;
}

#endif // INTERN_H__
