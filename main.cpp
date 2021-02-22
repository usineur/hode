/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#if !defined(PSP) && !defined(WII)
#include <SDL.h>
#endif
#include <ctype.h>
#include <getopt.h>
#include <sys/stat.h>

#include "game.h"
#include "menu.h"
#include "mixer.h"
#include "paf.h"
#include "util.h"
#include "resource.h"
#include "system.h"
#include "video.h"

static const char *_title = "Heart of Darkness";

static const char *_configIni = "hode.ini";

static const char *_usage =
	"hode - Heart of Darkness Interpreter\n"
	"Usage: %s [OPTIONS]...\n"
	"  --datapath=PATH   Path to data files (default '.')\n"
	"  --savepath=PATH   Path to save files (default '.')\n"
	"  --level=NUM       Start at level NUM\n"
	"  --checkpoint=NUM  Start at checkpoint NUM\n"
;

static bool _fullscreen = false;
static bool _widescreen = false;

static const bool _runBenchmark = false;
static bool _runMenu = true;
static bool _displayLoadingScreen = true;

static void lockAudio(int flag) {
	if (flag) {
		g_system->lockAudio();
	} else {
		g_system->unlockAudio();
	}
}

static void mixAudio(void *userdata, int16_t *buf, int len) {
	((Game *)userdata)->mixAudio(buf, len);
}

static void setupAudio(Game *g) {
	g->_mix._lock = lockAudio;
	AudioCallback cb;
	cb.proc = mixAudio;
	cb.userdata = g;
	g_system->startAudio(cb);
}

static const char *_defaultDataPath = ".";

static const char *_defaultSavePath = ".";

static const char *_levelNames[] = {
	"rock",
	"fort",
	"pwr1",
	"isld",
	"lava",
	"pwr2",
	"lar1",
	"lar2",
	"dark",
	0
};

static bool configBool(const char *value) {
	return strcasecmp(value, "true") == 0 || (strlen(value) == 2 && (value[0] == 't' || value[0] == '1'));
}

static void handleConfigIni(Game *g, const char *section, const char *name, const char *value) {
	// fprintf(stdout, "config.ini: section '%s' name '%s' value '%s'\n", section, name, value);
	if (strcmp(section, "engine") == 0) {
		if (strcmp(name, "disable_paf") == 0) {
			if (!g->_paf->_skipCutscenes) { // .paf file not found
				g->_paf->_skipCutscenes = configBool(value);
			}
		} else if (strcmp(name, "disable_mst") == 0) {
			g->_mstDisabled = configBool(value);
		} else if (strcmp(name, "disable_sss") == 0) {
			g->_sssDisabled = configBool(value);
		} else if (strcmp(name, "disable_menu") == 0) {
			_runMenu = !configBool(value);
		} else if (strcmp(name, "max_active_sounds") == 0) {
			g->_playingSssObjectsMax = atoi(value);
		} else if (strcmp(name, "difficulty") == 0) {
			g->_difficulty = atoi(value);
		} else if (strcmp(name, "frame_duration") == 0) {
			g->_frameMs = g->_paf->_frameMs = atoi(value);
		} else if (strcmp(name, "loading_screen") == 0) {
			_displayLoadingScreen = configBool(value);
		}
	} else if (strcmp(section, "display") == 0) {
		if (strcmp(name, "scale_factor") == 0) {
			const int scale = atoi(value);
			g_system->setScaler(0, scale);
		} else if (strcmp(name, "scale_algorithm") == 0) {
			g_system->setScaler(value, 0);
		} else if (strcmp(name, "gamma") == 0) {
			g_system->setGamma(atof(value));
		} else if (strcmp(name, "fullscreen") == 0) {
			_fullscreen = configBool(value);
		} else if (strcmp(name, "widescreen") == 0) {
			_widescreen = configBool(value);
		}
	}
}

static void readConfigIni(const char *filename, Game *g) {
	FILE *fp = fopen(filename, "rb");
	if (fp) {
		char *section = 0;
		char buf[256];
		while (fgets(buf, sizeof(buf), fp)) {
			if (buf[0] == '#') {
				continue;
			}
			if (buf[0] == '[') {
				char *p = strchr(&buf[1], ']');
				if (p) {
					*p = 0;
					free(section);
					section = strdup(&buf[1]);
				}
				continue;
			}
			char *p = strchr(buf, '=');
			if (!p) {
				continue;
			}
			*p++ = 0;
			while (*p && isspace(*p)) {
				++p;
			}
			if (*p) {
				char *q = p + strlen(p) - 1;
				while (q > p && isspace(*q)) {
					*q-- = 0;
				}
				handleConfigIni(g, section, buf, p);
			}
		}
		free(section);
		fclose(fp);
	}
}

int main(int argc, char *argv[]) {
	char *dataPath = 0;
	char *savePath = 0;
	int level = 0;
	int checkpoint = 0;
	bool resume = true; // resume game from 'setup.cfg'

	g_debugMask = 0; //kDebug_GAME | kDebug_RESOURCE | kDebug_SOUND | kDebug_MONSTER;
	int cheats = 0;

#ifdef WII
	System_earlyInit();
	static const char *pathsWII[] = {
		"sd:/hode",
		"usb:/hode",
		0
	};
	for (int i = 0; pathsWII[i]; ++i) {
		struct stat st;
		if (stat(pathsWII[i], &st) == 0 && S_ISDIR(st.st_mode)) {
			dataPath = strdup(pathsWII[i]);
			savePath = strdup(pathsWII[i]);
			break;
		}
	}
#endif
	if (System_hasCommandLine()) {
		if (argc == 2) {
			// data path as the only command line argument
			struct stat st;
			if (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) {
				dataPath = strdup(argv[1]);
			}
		}
		while (1) {
			static struct option options[] = {
				{ "datapath",   required_argument, 0, 1 },
				{ "savepath",   required_argument, 0, 2 },
				{ "level",      required_argument, 0, 3 },
				{ "checkpoint", required_argument, 0, 4 },
				{ "debug",      required_argument, 0, 5 },
				{ "cheats",     required_argument, 0, 6 },
				{ 0, 0, 0, 0 },
			};
			int index;
			const int c = getopt_long(argc, argv, "", options, &index);
			if (c == -1) {
				break;
			}
			switch (c) {
			case 1:
				dataPath = strdup(optarg);
				break;
			case 2:
				savePath = strdup(optarg);
				break;
			case 3:
				if (optarg[0] >= '0' && optarg[0] <= '9') {
					level = atoi(optarg);
				} else {
					for (int i = 0; _levelNames[i]; ++i) {
						if (strcmp(_levelNames[i], optarg) == 0) {
							level = i;
							break;
						}
					}
				}
				resume = false;
				break;
			case 4:
				checkpoint = atoi(optarg);
				resume = false;
				break;
			case 5:
				g_debugMask |= atoi(optarg);
				break;
			case 6:
				cheats |= atoi(optarg);
				break;
			default:
				fprintf(stdout, "%s\n", _usage);
				return -1;
			}
		}
	}
	Game *g = new Game(dataPath ? dataPath : _defaultDataPath, savePath ? savePath : _defaultSavePath, cheats);
	readConfigIni(_configIni, g);
	if (_runBenchmark) {
		g->benchmarkCpu();
	}
	// load setup.dat (PC) or setup.dax (PSX)
	g->_res->loadSetupDat();
	const bool isPsx = g->_res->_isPsx;
	g_system->init(_title, Video::W, Video::H, _fullscreen, _widescreen, isPsx);
	setupAudio(g);
	if (isPsx) {
		g->_video->initPsx();
		_runMenu = false;
	}
	if (_displayLoadingScreen) {
		g->displayLoadingScreen();
	}
	do {
		g->loadSetupCfg(resume);
		if (_runMenu && resume) {
			Menu *m = new Menu(g, g->_paf, g->_res, g->_video);
			const bool runGame = m->mainLoop();
			delete m;
			if (!runGame) {
				break;
			}
		}
		bool levelChanged = false;
		while (!g_system->inp.quit && level < kLvl_test) {
			if (_displayLoadingScreen) {
				g->displayLoadingScreen();
			}
			g->mainLoop(level, checkpoint, levelChanged);
			// do not save progress when starting from a specific level checkpoint
			if (resume) {
				g->saveSetupCfg();
			}
			if (g->_res->_isDemo) {
				break;
			}
			level = g->_currentLevel + 1;
			checkpoint = 0;
			levelChanged = true;
		}
	} while (!g_system->inp.quit && resume && !isPsx); // do not return to menu when starting from a specific level checkpoint
	g_system->stopAudio();
	g_system->destroy();
	delete g;
	free(dataPath);
	free(savePath);
	return 0;
}
