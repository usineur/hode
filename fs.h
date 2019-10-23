
#ifndef FS_H__
#define FS_H__

#include <stdio.h>

struct FileSystem {

	const char *_dataPath;
	int _filesCount;
	char **_filesList;

	FileSystem(const char *dataPath);
	~FileSystem();

	FILE *openFile(const char *filename);
	void closeFile(FILE *);

	void addFilePath(const char *path);
	void listFiles(const char *dir);
};

#endif // FS_H__
