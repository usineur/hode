
#include <assert.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/param.h>
#include "fs.h"
#include "util.h"

static const char *_suffixes[] = {
	"setup.dat",
	".paf",
	"_hod.lvl",
	"_hod.sss",
	"_hod.mst",
	0
};

static bool matchGameData(const char *path) {
	const int len = strlen(path);
	for (int i = 0; _suffixes[i]; ++i) {
		const int suffixLen = strlen(_suffixes[i]);
		if (len >= suffixLen && strcasecmp(path + len - suffixLen, _suffixes[i]) == 0) {
			return true;
		}
	}
	return false;
}

FileSystem::FileSystem(const char *dataPath)
	: _dataPath(dataPath), _filesCount(0), _filesList(0) {
	listFiles(dataPath);
}

FileSystem::~FileSystem() {
	for (int i = 0; i < _filesCount; ++i) {
		free(_filesList[i]);
	}
	free(_filesList);
}

FILE *FileSystem::openFile(const char *name) {
	FILE *fp = 0;
	for (int i = 0; i < _filesCount; ++i) {
		const char *p = strrchr(_filesList[i], '/');
		assert(p);
		if (strcasecmp(name, p + 1) == 0) {
			fp = fopen(_filesList[i], "rb");
			break;
		}
	}
	return fp;
}

void FileSystem::closeFile(FILE *fp) {
	fclose(fp);
}

void FileSystem::addFilePath(const char *path) {
	_filesList = (char **)realloc(_filesList, (_filesCount + 1) * sizeof(char *));
	if (_filesList) {
		_filesList[_filesCount] = strdup(path);
		++_filesCount;
	}
}

void FileSystem::listFiles(const char *dir) {
	DIR *d = opendir(dir);
	if (d) {
		dirent *de;
		while ((de = readdir(d)) != NULL) {
			if (de->d_name[0] == '.') {
				continue;
			}
			char filePath[MAXPATHLEN];
			snprintf(filePath, sizeof(filePath), "%s/%s", dir, de->d_name);
			struct stat st;
			if (stat(filePath, &st) == 0) {
				if (S_ISDIR(st.st_mode)) {
					listFiles(filePath);
				} else if (matchGameData(filePath)) {
					addFilePath(filePath);
				}
			}
		}
		closedir(d);
	}
}
