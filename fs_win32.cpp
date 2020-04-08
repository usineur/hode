// Credits: JayFoxRox - https://github.com/JayFoxRox/hode/blob/nxdk/fs_win32.cpp

#include <assert.h>
#include <windows.h>
#include <stdlib.h>
#include "fs.h"
#include "util.h"

static const char* _suffixes[] = {
	"setup.dat",
	"setup.dax",
	".paf",
	"_hod.lvl",
	"_hod.sss",
	"_hod.mst",
	0
};

static bool matchGameData(const char* path) {
	const int len = strlen(path);
	for (int i = 0; _suffixes[i]; ++i) {
		const int suffixLen = strlen(_suffixes[i]);
		if (len >= suffixLen && strcasecmp(path + len - suffixLen, _suffixes[i]) == 0) {
			return true;
		}
	}
	return false;
}

FileSystem::FileSystem(const char* dataPath, const char* savePath)
	: _dataPath(dataPath), _savePath(savePath), _filesCount(0), _filesList(0) {
	listFiles(dataPath);
}

FileSystem::~FileSystem() {
	for (int i = 0; i < _filesCount; ++i) {
		free(_filesList[i]);
	}
	free(_filesList);
}

FILE* FileSystem::openAssetFile(const char* name) {
	FILE* fp = 0;
	for (int i = 0; i < _filesCount; ++i) {
		const char* p = strrchr(_filesList[i], '\\');
		assert(p);
		if (strcasecmp(name, p + 1) == 0) {
			fp = fopen(_filesList[i], "rb");
			break;
		}
	}
	return fp;
}

FILE* FileSystem::openSaveFile(const char* filename, bool write) {
	char path[_MAX_PATH];
	snprintf(path, sizeof(path), "%s\\%s", _savePath, filename);
	return fopen(path, write ? "wb" : "rb");
}

int FileSystem::closeFile(FILE* fp) {
	const int err = ferror(fp);
	fclose(fp);
	return err;
}

void FileSystem::addFilePath(const char* path) {
	_filesList = (char**)realloc(_filesList, (_filesCount + 1) * sizeof(char*));
	if (_filesList) {
		_filesList[_filesCount] = _strdup(path);
		++_filesCount;
	}
}

void FileSystem::listFiles(const char* dir) {
	char filePath[_MAX_PATH];
	WIN32_FIND_DATAA de;
	snprintf(filePath, sizeof(filePath), "%s\\*", dir);
	HANDLE d = FindFirstFileA(filePath, &de);
	if (d != INVALID_HANDLE_VALUE) {
		BOOL found = TRUE;
		while(FindNextFileA(d, &de) != 0) {
			if (de.cFileName[0] == '.')
			{
				continue;
			}
			snprintf(filePath, sizeof(filePath), "%s\\%s", dir, de.cFileName);
			if (de.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				listFiles(filePath);
			}
			else if (matchGameData(filePath)) {
				addFilePath(filePath);
			}
		}
		FindClose(d);
	}
}