// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <algorithm>
#include "base/logging.h"
#include "util/text/utf8.h"
#include "LogManager.h"
#include "ConsoleListener.h"
#include "Timer.h"
#include "FileUtil.h"
#include "../Core/Config.h"

// Don't need to savestate this.
const char *hleCurrentThreadName = NULL;

// Unfortunately this is quite slow.
#define LOG_MSC_OUTPUTDEBUG false
// #define LOG_MSC_OUTPUTDEBUG true

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, 
		const char *file, int line, const char* fmt, ...) {
	if (!g_Config.bEnableLogging) return;

	va_list args;
	va_start(args, fmt);
	if (LogManager::GetInstance())
		LogManager::GetInstance()->Log(level, type,
			file, line, fmt, args);
	va_end(args);
}

bool GenericLogEnabled(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type) {
	if (LogManager::GetInstance())
		return g_Config.bEnableLogging && LogManager::GetInstance()->IsEnabled(level, type);
	return false;
}

LogManager *LogManager::logManager_ = NULL;

struct LogNameTableEntry {
	LogTypes::LOG_TYPE logType;
	const char *name;
	const char *longName;
};

static const LogNameTableEntry logTable[] = {
	{LogTypes::MASTER_LOG, "*",       "Master Log"},

	{LogTypes::SCEAUDIO   ,"AUDIO",   "sceAudio"},
	{LogTypes::SCECTRL    ,"CTRL",    "sceCtrl"},
	{LogTypes::SCEDISPLAY ,"DISP",    "sceDisplay"},
	{LogTypes::SCEFONT    ,"FONT",    "sceFont"},
	{LogTypes::SCEGE      ,"SCEGE",   "sceGe"},
	{LogTypes::SCEINTC    ,"INTC",    "sceKernelInterrupt"},
	{LogTypes::SCEIO      ,"IO",      "sceIo"},
	{LogTypes::SCEKERNEL  ,"KERNEL",  "sceKernel*"},
	{LogTypes::SCEMODULE  ,"MODULE",  "sceKernelModule"},
	{LogTypes::SCENET     ,"NET",     "sceNet*"},
	{LogTypes::SCERTC     ,"SCERTC",  "sceRtc"},
	{LogTypes::SCESAS     ,"SCESAS",  "sceSas"},
	{LogTypes::SCEUTILITY ,"UTIL",    "sceUtility"},

	{LogTypes::BOOT       ,"BOOT",    "Boot"},
	{LogTypes::COMMON     ,"COMMON",  "Common"},
	{LogTypes::CPU        ,"CPU",     "CPU"},
	{LogTypes::FILESYS    ,"FileSys", "File System"},
	{LogTypes::G3D        ,"G3D",     "3D Graphics"},
	{LogTypes::HLE        ,"HLE",     "HLE"},
	{LogTypes::JIT        ,"JIT",     "JIT compiler"},
	{LogTypes::LOADER     ,"LOAD",    "Loader"},
	{LogTypes::ME         ,"ME",      "Media Engine"},
	{LogTypes::MEMMAP     ,"MM",      "Memory Map"},
	{LogTypes::TIME       ,"TIME",    "CoreTiming"},
	{LogTypes::SASMIX     ,"SASMIX",  "Sound Mixer (Sas)"},
};

LogManager::LogManager() {
}

LogManager::~LogManager() {
}

void LogManager::ChangeFileLog(const char *filename) {
}

void LogManager::SaveConfig(IniFile::Section *section) {
}

void LogManager::LoadConfig(IniFile::Section *section) {
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char *file, int line, const char *format, va_list args) {
}

bool LogManager::IsEnabled(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type) {
	return true;
}

void LogManager::Init() {
	logManager_ = new LogManager();
}

void LogManager::Shutdown() {
	delete logManager_;
	logManager_ = NULL;
}

LogChannel::LogChannel(const char* shortName, const char* fullName, bool enable)
	: enable_(enable), m_hasListeners(false) {
	strncpy(m_fullName, fullName, 128);
	strncpy(m_shortName, shortName, 32);
#if defined(_DEBUG)
	level_ = LogTypes::LDEBUG;
#else
	level_ = LogTypes::LINFO;
#endif
}

// LogContainer
void LogChannel::AddListener(LogListener *listener) {
}

void LogChannel::RemoveListener(LogListener *listener) {
}

void LogChannel::Trigger(LogTypes::LOG_LEVELS level, const char *msg) {
}

FileLogListener::FileLogListener(const char *filename) {
}

void FileLogListener::Log(LogTypes::LOG_LEVELS, const char *msg) {
}

void DebuggerLogListener::Log(LogTypes::LOG_LEVELS, const char *msg) {
}

void RingbufferLogListener::Log(LogTypes::LOG_LEVELS level, const char *msg) {
}
