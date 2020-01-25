
#ifndef FS_H__
#define FS_H__

#include <stdio.h>

struct FileSystem {

	const char *_dataPath;
	const char *_savePath;
	int _filesCount;
	char **_filesList;

	FileSystem(const char *dataPath, const char *savePath);
	~FileSystem();

	FILE *openAssetFile(const char *filename);
	FILE *openSaveFile(const char *filename, bool write);
	int closeFile(FILE *);

	void addFilePath(const char *path);
	void listFiles(const char *dir);
};

#endif // FS_H__
