/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifdef __ANDROID__
#define LOG_TAG "HodJni"
#include <android/log.h>
#elif defined(__WIN32__) || defined(__WINRT__)
#include <windows.h>
#endif
#include <stdarg.h>
#include "util.h"

int g_debugMask;

void debug(int mask, const char *msg, ...) {
	char buf[1024];
	if (mask & g_debugMask) {
		va_list va;
		va_start(va, msg);
		vsprintf(buf, msg, va);
		va_end(va);
		printf("%s\n", buf);
		fflush(stdout);
#ifdef __ANDROID__
		__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);
#elif defined(__WINRT__)
		OutputDebugStringA(buf);
#endif
	}
}

void error(const char *msg, ...) {
	char buf[1024];
	va_list va;
	va_start(va, msg);
	vsprintf(buf, msg, va);
	va_end(va);
	fprintf(stderr, "ERROR: %s!\n", buf);
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", buf);
#elif defined(__WINRT__)
	char errorBuf[1024];
	sprintf(errorBuf, "ERROR: %s!\n", buf);
	OutputDebugStringA(errorBuf);
#endif
	exit(-1);
}

void warning(const char *msg, ...) {
	char buf[1024];
	va_list va;
	va_start(va, msg);
	vsprintf(buf, msg, va);
	va_end(va);
	fprintf(stderr, "WARNING: %s!\n", buf);
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_WARN, LOG_TAG, "%s", buf);
#elif defined(__WINRT__)
	char warnBuf[1024];
	sprintf(warnBuf, "WARNING: %s!\n", buf);
	OutputDebugStringA(warnBuf);
#endif
}

