#include <stdio.h>
#include <set>
#include <algorithm>


#include "base/basictypes.h"
#include "base/logging.h"
#include "file/zip_read.h"

// The return is non-const because - why not?
uint8_t *ReadLocalFile(const char *filename, size_t *size) {
	FILE *file = fopen(filename, "rb");
	if (!file) {
		*size = 0;
		return nullptr;
	}
	fseek(file, 0, SEEK_END);
	size_t f_size = ftell(file);
	if ((long)f_size < 0) {
		*size = 0;
		fclose(file);
		return nullptr;
	}
	fseek(file, 0, SEEK_SET);
	uint8_t *contents = new uint8_t[f_size+1];
	if (fread(contents, 1, f_size, file) != f_size) {
		delete [] contents;
		contents = nullptr;
		*size = 0;
	} else {
		contents[f_size] = 0;
		*size = f_size;
	}
	fclose(file);
	return contents;
}

uint8_t *DirectoryAssetReader::ReadAsset(const char *path, size_t *size) {
	char new_path[2048];
	new_path[0] = '\0';
	// Check if it already contains the path
	if (strlen(path) > strlen(path_) && 0 == memcmp(path, path_, strlen(path_))) {
	}
	else {
		strcpy(new_path, path_);
	}
	strcat(new_path, path);
	return ReadLocalFile(new_path, size);
}

bool DirectoryAssetReader::GetFileListing(const char *path, std::vector<FileInfo> *listing, const char *filter = 0)
{
	char new_path[2048];
	new_path[0] = '\0';
	// Check if it already contains the path
	if (strlen(path) > strlen(path_) && 0 == memcmp(path, path_, strlen(path_))) {
	}
	else {
		strcpy(new_path, path_);
	}
	strcat(new_path, path);
	FileInfo info;
	if (!getFileInfo(new_path, &info))
		return false;

	if (info.isDirectory)
	{
		getFilesInDir(new_path, listing, filter);
		return true;
	}
	else
	{
		return false;
	}
}

bool DirectoryAssetReader::GetFileInfo(const char *path, FileInfo *info) 
{
	char new_path[2048];
	new_path[0] = '\0';
	// Check if it already contains the path
	if (strlen(path) > strlen(path_) && 0 == memcmp(path, path_, strlen(path_))) {
	}
	else {
		strcpy(new_path, path_);
	}
	strcat(new_path, path);
	return getFileInfo(new_path, info);	
}

struct VFSEntry {
	const char *prefix;
	AssetReader *reader;
};

static VFSEntry entries[16];
static int num_entries = 0;

void VFSRegister(const char *prefix, AssetReader *reader) {
	entries[num_entries].prefix = prefix;
	entries[num_entries].reader = reader;
	DLOG("Registered VFS for prefix %s: %s", prefix, reader->toString().c_str());
	num_entries++;
}

void VFSShutdown() {
	for (int i = 0; i < num_entries; i++) {
		delete entries[i].reader;
	}
	num_entries = 0;
}

uint8_t *VFSReadFile(const char *filename, size_t *size) {
	if (filename[0] == '/') {
		// Local path, not VFS.
		ILOG("Not a VFS path: %s . Reading local file.", filename);
		return ReadLocalFile(filename, size);
	}

	int fn_len = (int)strlen(filename);
	bool fileSystemFound = false;
	for (int i = 0; i < num_entries; i++) {
		int prefix_len = (int)strlen(entries[i].prefix);
		if (prefix_len >= fn_len) continue;
		if (0 == memcmp(filename, entries[i].prefix, prefix_len)) {
			fileSystemFound = true;
			// ILOG("Prefix match: %s (%s) -> %s", entries[i].prefix, filename, filename + prefix_len);
			uint8_t *data = entries[i].reader->ReadAsset(filename + prefix_len, size);
			if (data)
				return data;
			else
				continue;
			// Else try the other registered file systems.
		}
	}
	if (!fileSystemFound) {
		ELOG("Missing filesystem for %s", filename);
	}  // Otherwise, the file was just missing. No need to log.
	return 0;
}

bool VFSGetFileListing(const char *path, std::vector<FileInfo> *listing, const char *filter) {
#ifdef _WIN32
	if (path[1] == ':') {
#else
	if (path[0] == '/') {
#endif
		// Local path, not VFS.
		ILOG("Not a VFS path: %s . Reading local directory.", path);
		getFilesInDir(path, listing, filter);
		return true;
	}
	
	int fn_len = (int)strlen(path);
	bool fileSystemFound = false;
	for (int i = 0; i < num_entries; i++) {
		int prefix_len = (int)strlen(entries[i].prefix);
		if (prefix_len >= fn_len) continue;
		if (0 == memcmp(path, entries[i].prefix, prefix_len)) {
			fileSystemFound = true;
			if (entries[i].reader->GetFileListing(path + prefix_len, listing, filter)) {
				return true;
			}
		}
	}

	if (!fileSystemFound) {
		ELOG("Missing filesystem for %s", path);
	}  // Otherwise, the file was just missing. No need to log.
	return false;
}

bool VFSGetFileInfo(const char *path, FileInfo *info) {
#ifdef _WIN32
	if (path[1] == ':') {
#else
	if (path[0] == '/') {
#endif
		// Local path, not VFS.
		ILOG("Not a VFS path: %s . Getting local file info.", path);
		return getFileInfo(path, info);
	}

	bool fileSystemFound = false;
	int fn_len = (int)strlen(path);
	for (int i = 0; i < num_entries; i++) {
		int prefix_len = (int)strlen(entries[i].prefix);
		if (prefix_len >= fn_len) continue;
		if (0 == memcmp(path, entries[i].prefix, prefix_len)) {
			fileSystemFound = true;
			if (entries[i].reader->GetFileInfo(path + prefix_len, info))
				return true;
			else
				continue;
		}
	}
	if (!fileSystemFound) {
		ELOG("Missing filesystem for %s", path);
	}  // Otherwise, the file was just missing. No need to log.
	return false;
}
