// TODO: Move much of this code to vfs.cpp
#pragma once


#include <string.h>
#include <string>

#include "base/basictypes.h"
#include "file/vfs.h"
#include "file/file_util.h"

// Direct readers. deallocate using delete [].
uint8_t *ReadLocalFile(const char *filename, size_t *size);

class AssetReader {
public:
	virtual ~AssetReader() {}
	// use delete[]
	virtual uint8_t *ReadAsset(const char *path, size_t *size) = 0;
	// Filter support is optional but nice to have
	virtual bool GetFileListing(const char *path, std::vector<FileInfo> *listing, const char *filter = 0) = 0;
	virtual bool GetFileInfo(const char *path, FileInfo *info) = 0;
	virtual std::string toString() const = 0;
};

class DirectoryAssetReader : public AssetReader {
public:
	DirectoryAssetReader(const char *path) {
		strncpy(path_, path, ARRAY_SIZE(path_));
		path_[ARRAY_SIZE(path_) - 1] = '\0';
	}
	// use delete[]
	virtual uint8_t *ReadAsset(const char *path, size_t *size);
	virtual bool GetFileListing(const char *path, std::vector<FileInfo> *listing, const char *filter);
	virtual bool GetFileInfo(const char *path, FileInfo *info);
	virtual std::string toString() const {
		return path_;
	}

private:
	char path_[512];
};

