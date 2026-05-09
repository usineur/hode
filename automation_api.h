/*
 * Heart of Darkness engine rewrite
 * Automation API for programmatic game control
 */

#ifndef AUTOMATION_API_H__
#define AUTOMATION_API_H__

#include "intern.h"

struct Game;

struct AutomationApi {
	enum {
		kMaxResponseSize = 65536,
		kMaxCommandSize = 4096
	};

	Game *_game;
	int _serverFd;
	int _clientFd;
	bool _enabled;
	bool _stepMode;
	int _stepCount;
	int _injectedFrames;
	uint8_t _injectedDirection;
	uint8_t _injectedAction;
	char _socketPath[256];
	char _cmdBuf[kMaxCommandSize];
	int _cmdBufLen;

	AutomationApi();
	~AutomationApi();

	void init(const char *socketPath, Game *game);
	void shutdown();
	void processCommands();
	void notifyFrameComplete();
	void waitForStepCommand();

	bool hasInjectedInput() const { return _injectedFrames > 0; }
	uint8_t getDirectionMask() const { return _injectedDirection; }
	uint8_t getActionMask() const { return _injectedAction; }
	uint8_t getRawInputMask() const { return _injectedRawMask; }

	// For menu: inject raw SYS_INP_* mask directly
	uint8_t _injectedRawMask;

private:
	void acceptClient();
	void readCommands();
	void handleCommand(const char *json);
	void handleGetState();
	void handleInjectInput(const char *json);
	void handleScreenshot();
	void handleStep(const char *json);
	void handleSetLevel(const char *json);
	void sendResponse(const char *json);
	void sendBinaryResponse(const uint8_t *data, int size);
};

#endif // AUTOMATION_API_H__
