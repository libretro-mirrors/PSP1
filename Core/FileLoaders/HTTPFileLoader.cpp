// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <algorithm>
#include "base/stringutil.h"
#include "Common/Common.h"
#include "Core/FileLoaders/HTTPFileLoader.h"

HTTPFileLoader::HTTPFileLoader(const std::string &filename)
	: filesize_(0), filepos_(0), url_(filename), filename_(filename), connected_(false) {
	if (!client_.Resolve(url_.Host().c_str(), url_.Port())) {
		// TODO: Should probably set some flag?
		return;
	}

	Connect();
	int err = client_.SendRequest("HEAD", url_.Resource().c_str());
	if (err < 0) {
		Disconnect();
		return;
	}

	Buffer readbuf;
	std::vector<std::string> responseHeaders;
	int code = client_.ReadResponseHeaders(&readbuf, responseHeaders);
	if (code != 200) {
		// Leave size at 0, invalid.
		ERROR_LOG(LOADER, "HTTP request failed, got %03d for %s", code, filename.c_str());
		Disconnect();
		return;
	}

	// TODO: Expire cache via ETag, etc.
	bool acceptsRange = false;
	for (std::string header : responseHeaders) {
		if (startsWithNoCase(header, "Content-Length:")) {
			size_t size_pos = header.find_first_of(' ');
			if (size_pos != header.npos) {
				size_pos = header.find_first_not_of(' ', size_pos);
			}
			if (size_pos != header.npos)
				filesize_ = atoll(&header[size_pos]);
		}
		if (startsWithNoCase(header, "Accept-Ranges:")) {
			std::string lowerHeader = header;
			std::transform(lowerHeader.begin(), lowerHeader.end(), lowerHeader.begin(), tolower);
			// TODO: Delimited.
			if (lowerHeader.find("bytes") != lowerHeader.npos) {
				acceptsRange = true;
			}
		}
	}

	// TODO: Keepalive instead.
	Disconnect();

	if (!acceptsRange) {
		WARN_LOG(LOADER, "HTTP server did not advertise support for range requests.");
	}
	if (filesize_ == 0) {
		ERROR_LOG(LOADER, "Could not determine file size for %s", filename.c_str());
	}

	// If we didn't end up with a filesize_ (e.g. chunked response), give up.  File invalid.
}

HTTPFileLoader::~HTTPFileLoader() {
	Disconnect();
}

bool HTTPFileLoader::Exists() {
	return url_.Valid() && filesize_ > 0;
}

bool HTTPFileLoader::IsDirectory() {
	// Only files.
	return false;
}

s64 HTTPFileLoader::FileSize() {
	return filesize_;
}

std::string HTTPFileLoader::Path() const {
	return filename_;
}

void HTTPFileLoader::Seek(s64 absolutePos) {
	filepos_ = absolutePos;
}

size_t HTTPFileLoader::ReadAt(s64 absolutePos, size_t bytes, void *data) {
	s64 absoluteEnd = std::min(absolutePos + (s64)bytes, filesize_);
	if (absolutePos >= filesize_ || bytes == 0) {
		// Read outside of the file or no read at all, just fail immediately.
		return 0;
	}

	Connect();

	char requestHeaders[4096];
	// Note that the Range header is *inclusive*.
	snprintf(requestHeaders, sizeof(requestHeaders),
		"Range: bytes=%lld-%lld\r\n", absolutePos, absoluteEnd - 1);

	int err = client_.SendRequest("GET", url_.Resource().c_str(), requestHeaders, nullptr);
	if (err < 0) {
		Disconnect();
		return 0;
	}

	Buffer readbuf;
	std::vector<std::string> responseHeaders;
	int code = client_.ReadResponseHeaders(&readbuf, responseHeaders);
	if (code != 206) {
		ERROR_LOG(LOADER, "HTTP server did not respond with range, received code=%03d", code);
		Disconnect();
		return 0;
	}

	// TODO: Expire cache via ETag, etc.
	// We don't support multipart/byteranges responses.
	bool supportedResponse = false;
	for (std::string header : responseHeaders) {
		if (startsWithNoCase(header, "Content-Range:")) {
			// TODO: More correctness.  Whitespace can be missing or different.
			s64 first = -1, last = -1, total = -1;
			std::string lowerHeader = header;
			std::transform(lowerHeader.begin(), lowerHeader.end(), lowerHeader.begin(), tolower);
			if (sscanf(lowerHeader.c_str(), "content-range: bytes %lld-%lld/%lld", &first, &last, &total) >= 2) {
				if (first == absolutePos && last == absoluteEnd - 1) {
					supportedResponse = true;
				} else {
					ERROR_LOG(LOADER, "Unexpected HTTP range: got %lld-%lld, wanted %lld-%lld.", first, last, absolutePos, absoluteEnd - 1);
				}
			} else {
				ERROR_LOG(LOADER, "Unexpected HTTP range response: %s", header.c_str());
			}
		}
	}

	// TODO: Would be nice to read directly.
	Buffer output;
	int res = client_.ReadResponseEntity(&readbuf, responseHeaders, &output);
	if (res != 0) {
		ERROR_LOG(LOADER, "Unable to read HTTP response entity: %d", res);
		// Let's take anything we got anyway.  Not worse than returning nothing?
	}

	// TODO: Keepalive instead.
	Disconnect();

	if (!supportedResponse) {
		ERROR_LOG(LOADER, "HTTP server did not respond with the range we wanted.");
		return 0;
	}

	size_t readBytes = output.size();
	output.Take(readBytes, (char *)data);
	filepos_ = absolutePos + readBytes;
	return readBytes;
}
