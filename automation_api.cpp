/*
 * Heart of Darkness engine rewrite
 * Automation API - Unix domain socket server with JSON protocol
 */

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "automation_api.h"
#include "game.h"
#include "video.h"
#include "screenshot.h"
#include "system.h"

AutomationApi::AutomationApi() {
	_game = 0;
	_serverFd = -1;
	_clientFd = -1;
	_enabled = false;
	_stepMode = false;
	_stepCount = 0;
	_injectedFrames = 0;
	_injectedDirection = 0;
	_injectedAction = 0;
	_injectedRawMask = 0;
	memset(_socketPath, 0, sizeof(_socketPath));
	_cmdBufLen = 0;
}

AutomationApi::~AutomationApi() {
	shutdown();
}

void AutomationApi::init(const char *socketPath, Game *game) {
	_game = game;
	strncpy(_socketPath, socketPath, sizeof(_socketPath) - 1);

	unlink(_socketPath);

	_serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (_serverFd < 0) {
		fprintf(stderr, "AutomationApi: socket() failed: %s\n", strerror(errno));
		return;
	}

	// non-blocking
	int flags = fcntl(_serverFd, F_GETFL, 0);
	fcntl(_serverFd, F_SETFL, flags | O_NONBLOCK);

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, _socketPath, sizeof(addr.sun_path) - 1);

	if (bind(_serverFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "AutomationApi: bind(%s) failed: %s\n", _socketPath, strerror(errno));
		close(_serverFd);
		_serverFd = -1;
		return;
	}

	if (listen(_serverFd, 1) < 0) {
		fprintf(stderr, "AutomationApi: listen() failed: %s\n", strerror(errno));
		close(_serverFd);
		_serverFd = -1;
		return;
	}

	_enabled = true;
	fprintf(stdout, "AutomationApi: listening on %s\n", _socketPath);
}

void AutomationApi::shutdown() {
	if (_clientFd >= 0) {
		close(_clientFd);
		_clientFd = -1;
	}
	if (_serverFd >= 0) {
		close(_serverFd);
		_serverFd = -1;
	}
	if (_socketPath[0]) {
		unlink(_socketPath);
	}
	_enabled = false;
}

void AutomationApi::acceptClient() {
	if (_serverFd < 0 || _clientFd >= 0) return;

	int fd = accept(_serverFd, 0, 0);
	if (fd < 0) return;

	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	_clientFd = fd;
	_cmdBufLen = 0;
	fprintf(stdout, "AutomationApi: client connected\n");
}

void AutomationApi::readCommands() {
	if (_clientFd < 0) return;

	char buf[1024];
	ssize_t n = read(_clientFd, buf, sizeof(buf));
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) return;
		close(_clientFd);
		_clientFd = -1;
		_cmdBufLen = 0;
		return;
	}
	if (n == 0) {
		close(_clientFd);
		_clientFd = -1;
		_cmdBufLen = 0;
		fprintf(stdout, "AutomationApi: client disconnected\n");
		return;
	}

	for (ssize_t i = 0; i < n; ++i) {
		if (buf[i] == '\n') {
			_cmdBuf[_cmdBufLen] = 0;
			if (_cmdBufLen > 0) {
				handleCommand(_cmdBuf);
			}
			_cmdBufLen = 0;
		} else if (_cmdBufLen < kMaxCommandSize - 1) {
			_cmdBuf[_cmdBufLen++] = buf[i];
		}
	}
}

void AutomationApi::processCommands() {
	if (!_enabled) return;
	acceptClient();
	readCommands();

	// Apply raw input mask for menu/system-level input
	if (_injectedFrames > 0) {
		g_system->inp.mask |= _injectedRawMask;
		--_injectedFrames;
		if (_injectedFrames == 0) {
			_injectedDirection = 0;
			_injectedAction = 0;
			_injectedRawMask = 0;
		}
	}
}

void AutomationApi::notifyFrameComplete() {
	if (_stepMode && _stepCount > 0) {
		--_stepCount;
	}
}

void AutomationApi::waitForStepCommand() {
	while (_stepMode && _stepCount <= 0 && _enabled) {
		acceptClient();
		readCommands();
		g_system->sleep(5);
		g_system->processEvents();
		if (g_system->inp.quit) {
			_enabled = false;
			return;
		}
	}
}

// Simple JSON string search helper (no external dep)
static const char *jsonGetString(const char *json, const char *key) {
	char pattern[64];
	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	const char *p = strstr(json, pattern);
	if (!p) return 0;
	p += strlen(pattern);
	while (*p == ' ' || *p == ':' || *p == '\t') ++p;
	return p;
}

static int jsonGetInt(const char *json, const char *key, int defaultVal) {
	const char *p = jsonGetString(json, key);
	if (!p) return defaultVal;
	if (*p == '"') ++p;
	return atoi(p);
}

void AutomationApi::handleCommand(const char *json) {
	const char *cmd = jsonGetString(json, "cmd");
	if (!cmd) return;

	if (strncmp(cmd, "\"get_state\"", 11) == 0) {
		handleGetState();
	} else if (strncmp(cmd, "\"input\"", 7) == 0) {
		handleInjectInput(json);
	} else if (strncmp(cmd, "\"screenshot\"", 12) == 0) {
		handleScreenshot();
	} else if (strncmp(cmd, "\"step\"", 6) == 0) {
		handleStep(json);
	} else if (strncmp(cmd, "\"set_level\"", 11) == 0) {
		handleSetLevel(json);
	} else {
		sendResponse("{\"error\":\"unknown command\"}\n");
	}
}

void AutomationApi::handleGetState() {
	if (!_game) return;

	char buf[8192];
	int andyX = 0, andyY = 0, andyScreen = 0, andyAnim = 0, andyFrame = 0;
	int andySprite = 0;
	bool dying = _game->_levelRestartCounter != 0;
	if (_game->_andyObject) {
		andyX = _game->_andyObject->xPos;
		andyY = _game->_andyObject->yPos;
		andyScreen = _game->_andyObject->screenNum;
		andyAnim = _game->_andyObject->anim;
		andyFrame = _game->_andyObject->frame;
		andySprite = _game->_andyObject->spriteNum;
	}

	// Build monster list (only monsters on current screen)
	char monstersBuf[4096];
	int mpos = 0;
	mpos += snprintf(monstersBuf + mpos, sizeof(monstersBuf) - mpos, "[");
	int mcount = 0;
	for (int i = 0; i < Game::kMaxMonsterObjects1; ++i) {
		const MonsterObject1 *m = &_game->_monsterObjects1Table[i];
		if (m->o16 && m->o16->screenNum == _game->_currentScreen) {
			if (mcount > 0) mpos += snprintf(monstersBuf + mpos, sizeof(monstersBuf) - mpos, ",");
			mpos += snprintf(monstersBuf + mpos, sizeof(monstersBuf) - mpos,
				"{\"x\":%d,\"y\":%d,\"type\":1,\"i\":%d}",
				m->xPos, m->yPos, i);
			mcount++;
			if (mpos > 3800) break;
		}
	}
	for (int i = 0; i < Game::kMaxMonsterObjects2; ++i) {
		const MonsterObject2 *m = &_game->_monsterObjects2Table[i];
		if (m->o && m->o->screenNum == _game->_currentScreen) {
			if (mcount > 0) mpos += snprintf(monstersBuf + mpos, sizeof(monstersBuf) - mpos, ",");
			mpos += snprintf(monstersBuf + mpos, sizeof(monstersBuf) - mpos,
				"{\"x\":%d,\"y\":%d,\"type\":2,\"i\":%d}",
				m->xPos, m->yPos, i);
			mcount++;
			if (mpos > 3800) break;
		}
	}
	mpos += snprintf(monstersBuf + mpos, sizeof(monstersBuf) - mpos, "]");

	snprintf(buf, sizeof(buf),
		"{\"andy\":{\"x\":%d,\"y\":%d,\"screen\":%d,\"anim\":%d,\"frame\":%d,"
		"\"sprite\":%d,\"hasCannon\":%s,\"dying\":%s},"
		"\"level\":%d,\"checkpoint\":%d,\"screen\":%d,"
		"\"endLevel\":%s,\"monsters\":%s,\"monsterCount\":%d}\n",
		andyX, andyY, andyScreen, andyAnim, andyFrame,
		andySprite,
		andySprite == 0 ? "true" : "false",
		dying ? "true" : "false",
		_game->_currentLevel,
		_game->_currentLevelCheckpoint,
		_game->_currentScreen,
		_game->_endLevel ? "true" : "false",
		monstersBuf, mcount);
	sendResponse(buf);
}

void AutomationApi::handleInjectInput(const char *json) {
	_injectedDirection = (uint8_t)jsonGetInt(json, "dir", 0);
	_injectedAction = (uint8_t)jsonGetInt(json, "act", 0);
	_injectedFrames = jsonGetInt(json, "frames", 1);
	// Also build raw SYS_INP mask for menu/system input
	_injectedRawMask = 0;
	if (_injectedDirection & 1) _injectedRawMask |= SYS_INP_UP;
	if (_injectedDirection & 2) _injectedRawMask |= SYS_INP_RIGHT;
	if (_injectedDirection & 4) _injectedRawMask |= SYS_INP_DOWN;
	if (_injectedDirection & 8) _injectedRawMask |= SYS_INP_LEFT;
	if (_injectedAction & 1) _injectedRawMask |= SYS_INP_RUN;
	if (_injectedAction & 2) _injectedRawMask |= SYS_INP_JUMP;
	if (_injectedAction & 4) _injectedRawMask |= SYS_INP_SHOOT;
	// Also support direct raw mask
	int raw = jsonGetInt(json, "raw", 0);
	if (raw) _injectedRawMask = (uint8_t)raw;
	// No response for input injection - reduces protocol complexity
}

void AutomationApi::handleScreenshot() {
	if (!_game || !_game->_video) {
		sendResponse("{\"error\":\"no video\"}\n");
		return;
	}
	// Send raw framebuffer dimensions and palette-converted RGB data
	const int w = Video::W;
	const int h = Video::H;
	char header[128];
	snprintf(header, sizeof(header), "{\"width\":%d,\"height\":%d,\"format\":\"rgb\",\"size\":%d}\n",
		w, h, w * h * 3);
	sendResponse(header);

	// Convert indexed to RGB and send
	uint8_t *rgb = (uint8_t *)malloc(w * h * 3);
	if (rgb) {
		const uint8_t *src = _game->_video->_frontLayer;
		const uint8_t *pal = _game->_video->_palette;
		for (int i = 0; i < w * h; ++i) {
			const uint8_t c = src[i];
			rgb[i * 3 + 0] = pal[c * 3 + 0];
			rgb[i * 3 + 1] = pal[c * 3 + 1];
			rgb[i * 3 + 2] = pal[c * 3 + 2];
		}
		sendBinaryResponse(rgb, w * h * 3);
		free(rgb);
	}
}

void AutomationApi::handleStep(const char *json) {
	_stepMode = true;
	_stepCount = jsonGetInt(json, "count", 1);
}

void AutomationApi::handleSetLevel(const char *json) {
	if (!_game) return;
	int level = jsonGetInt(json, "level", -1);
	int checkpoint = jsonGetInt(json, "checkpoint", 0);
	if (level >= 0 && level <= 8) {
		_game->_currentLevel = level;
		_game->_currentLevelCheckpoint = checkpoint;
		_game->_endLevel = true;
	}
}

void AutomationApi::sendResponse(const char *json) {
	if (_clientFd < 0) return;
	int len = strlen(json);
	int sent = 0;
	while (sent < len) {
		ssize_t n = write(_clientFd, json + sent, len - sent);
		if (n <= 0) break;
		sent += n;
	}
}

void AutomationApi::sendBinaryResponse(const uint8_t *data, int size) {
	if (_clientFd < 0) return;
	int sent = 0;
	while (sent < size) {
		ssize_t n = write(_clientFd, data + sent, size - sent);
		if (n <= 0) break;
		sent += n;
	}
}
