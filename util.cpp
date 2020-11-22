/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifdef __ANDROID__
#define LOG_TAG "HodJni"
#include <android/log.h>
#endif
#include <stdio.h>
#include <stdarg.h>
extern void System_printLog(FILE *, const char *s);
extern void System_fatalError(const char *s);

int g_debugMask;

void debug(int mask, const char *msg, ...) {
	char buf[1024];
	if (mask & g_debugMask) {
		va_list va;
		va_start(va, msg);
		vsprintf(buf, msg, va);
		va_end(va);
#ifdef __ANDROID__
		__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);
#endif
		System_printLog(stdout, buf);
	}
}

void error(const char *msg, ...) {
	char buf[1024];
	va_list va;
	va_start(va, msg);
	vsprintf(buf, msg, va);
	va_end(va);
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", buf);
#endif
	System_fatalError(buf);
}

void warning(const char *msg, ...) {
	char buf[1024];
	va_list va;
	va_start(va, msg);
	vsprintf(buf, msg, va);
	va_end(va);
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_WARN, LOG_TAG, "%s", buf);
#endif
	System_printLog(stderr, buf);
}
