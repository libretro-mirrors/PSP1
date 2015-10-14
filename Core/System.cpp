// Copyright (C) 2012 PPSSPP Project

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

#if defined(_WIN32) && !defined(__MINGW32__)
#include "Common/CommonWindows.h"
#include <ShlObj.h>
#include <string>
#include <codecvt>
#endif

#include "math/math_util.h"
#include "native/thread/thread.h"
#include "native/thread/threadutil.h"
#include "native/base/mutex.h"
#include "util/text/utf8.h"

#include "Core/MemMap.h"
#include "Core/HDRemaster.h"

#include "Core/MIPS/MIPS.h"
#include "Core/MIPS/MIPSAnalyst.h"

#include "Core/Host.h"
#include "Core/System.h"
#include "Core/HLE/HLE.h"
#include "Core/HLE/ReplaceTables.h"
#include "Core/HLE/sceKernel.h"
#include "Core/HLE/sceKernelMemory.h"
#include "Core/HLE/sceAudio.h"
#include "Core/Config.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/CoreParameter.h"
#include "Core/FileSystems/MetaFileSystem.h"
#include "Core/Loaders.h"
#include "Core/PSPLoaders.h"
#include "Core/ELF/ParamSFO.h"
#include "Core/SaveState.h"
#include "Core/HLE/sceAudiocodec.h"

#include "GPU/GPUState.h"
#include "GPU/GPUInterface.h"

enum CPUThreadState {
	CPU_THREAD_NOT_RUNNING,
	CPU_THREAD_PENDING,
	CPU_THREAD_STARTING,
	CPU_THREAD_RUNNING,
	CPU_THREAD_SHUTDOWN,
	CPU_THREAD_QUIT,

	CPU_THREAD_EXECUTE,
	CPU_THREAD_RESUME,
};

MetaFileSystem pspFileSystem;
ParamSFOData g_paramSFO;
static CoreParameter coreParameter;
static FileLoader *loadedFile;
static std::thread *cpuThread = nullptr;
static std::thread::id cpuThreadID;
static recursive_mutex cpuThreadLock;
static condition_variable cpuThreadCond;
static condition_variable cpuThreadReplyCond;
static u64 cpuThreadUntil;
bool audioInitialized;

// This can be read and written from ANYWHERE.
volatile CoreState coreState = CORE_STEPPING;
// Note: intentionally not used for CORE_NEXTFRAME.
volatile bool coreStatePending = false;
static volatile CPUThreadState cpuThreadState = CPU_THREAD_NOT_RUNNING;

bool IsAudioInitialised() {
	return audioInitialized;
}

void Audio_Init() {
	if (!audioInitialized) {
		audioInitialized = true;
		host->InitSound();
	}
}

bool IsOnSeparateCPUThread() {
	if (cpuThread != nullptr)
		return cpuThreadID == std::this_thread::get_id();
   return false;
}

void CPU_SetState(CPUThreadState to) {
	lock_guard guard(cpuThreadLock);
	cpuThreadState = to;
	cpuThreadCond.notify_one();
	cpuThreadReplyCond.notify_one();
}

bool CPU_NextState(CPUThreadState from, CPUThreadState to) {
	lock_guard guard(cpuThreadLock);
	if (cpuThreadState == from)
   {
		CPU_SetState(to);
		return true;
	}
   return false;
}

bool CPU_NextStateNot(CPUThreadState from, CPUThreadState to) {
	lock_guard guard(cpuThreadLock);
	if (cpuThreadState != from)
   {
		CPU_SetState(to);
		return true;
	}
   return false;
}

bool CPU_IsReady() {
	return cpuThreadState == CPU_THREAD_RUNNING || cpuThreadState == CPU_THREAD_NOT_RUNNING;
}

bool CPU_IsShutdown() {
	return cpuThreadState == CPU_THREAD_NOT_RUNNING;
}

bool CPU_HasPendingAction() {
	return cpuThreadState != CPU_THREAD_RUNNING;
}

void CPU_WaitStatus(condition_variable &cond, bool (*pred)()) {
	lock_guard guard(cpuThreadLock);
	while (!pred())
		cond.wait(cpuThreadLock);
}

void CPU_Shutdown();

void CPU_Init() {
	coreState = CORE_POWERUP;
	currentMIPS = &mipsr4k;

	// Default memory settings
	// Seems to be the safest place currently..
	if (g_Config.iPSPModel == PSP_MODEL_FAT)
		Memory::g_MemorySize = Memory::RAM_NORMAL_SIZE; // 32 MB of ram by default
	else
		Memory::g_MemorySize = Memory::RAM_DOUBLE_SIZE;

	g_RemasterMode = false;
	g_DoubleTextureCoordinates = false;
	Memory::g_PSPModel = g_Config.iPSPModel;

	std::string filename = coreParameter.fileToStart;
	loadedFile = ConstructFileLoader(filename);
	IdentifiedFileType type = Identify_File(loadedFile);

	// TODO: Put this somewhere better?
	if (coreParameter.mountIso != "") {
		coreParameter.mountIsoLoader = ConstructFileLoader(coreParameter.mountIso);
	}

	MIPSAnalyst::Reset();
	Replacement_Init();

	switch (type) {
	case FILETYPE_PSP_ISO:
	case FILETYPE_PSP_ISO_NP:
	case FILETYPE_PSP_DISC_DIRECTORY:
		InitMemoryForGameISO(loadedFile);
		break;
	default:
		break;
	}

	Memory::Init();
	mipsr4k.Reset();

	host->AttemptLoadSymbolMap();

	if (coreParameter.enableSound) {
		Audio_Init();
	}

	CoreTiming::Init();

	// Init all the HLE modules
	HLEInit();

	// TODO: Check Game INI here for settings, patches and cheats, and modify coreParameter accordingly

	// Why did we check for CORE_POWERDOWN here?
	if (!LoadFile(&loadedFile, &coreParameter.errorString)) {
		CPU_Shutdown();
		coreParameter.fileToStart = "";
		CPU_SetState(CPU_THREAD_NOT_RUNNING);
		return;
	}


	if (coreParameter.updateRecent) {
		g_Config.AddRecent(filename);
	}

	coreState = coreParameter.startPaused ? CORE_STEPPING : CORE_RUNNING;
}

void CPU_Shutdown() {
	if (g_Config.bAutoSaveSymbolMap) {
		host->SaveSymbolMap();
	}

	Replacement_Shutdown();

	CoreTiming::Shutdown();
	__KernelShutdown();
	HLEShutdown();
	if (coreParameter.enableSound) {
		host->ShutdownSound();
		audioInitialized = false;  // deleted in ShutdownSound
	}
	pspFileSystem.Shutdown();
	mipsr4k.Shutdown();
	Memory::Shutdown();

	delete loadedFile;
	loadedFile = nullptr;

	delete coreParameter.mountIsoLoader;
	coreParameter.mountIsoLoader = nullptr;
}

// TODO: Maybe loadedFile doesn't even belong here...
void UpdateLoadedFile(FileLoader *fileLoader) {
	delete loadedFile;
	loadedFile = fileLoader;
}

void CPU_RunLoop() {
	setCurrentThreadName("CPU");

	if (CPU_NextState(CPU_THREAD_PENDING, CPU_THREAD_STARTING)) {
		CPU_Init();
		CPU_NextState(CPU_THREAD_STARTING, CPU_THREAD_RUNNING);
	} else if (!CPU_NextState(CPU_THREAD_RESUME, CPU_THREAD_RUNNING)) {
		ERROR_LOG(CPU, "CPU thread in unexpected state: %d", cpuThreadState);
		return;
	}

	while (cpuThreadState != CPU_THREAD_SHUTDOWN)
	{
		CPU_WaitStatus(cpuThreadCond, &CPU_HasPendingAction);
		switch (cpuThreadState) {
		case CPU_THREAD_EXECUTE:
			mipsr4k.RunLoopUntil(cpuThreadUntil);
			gpu->FinishEventLoop();
			CPU_NextState(CPU_THREAD_EXECUTE, CPU_THREAD_RUNNING);
			break;

		// These are fine, just keep looping.
		case CPU_THREAD_RUNNING:
		case CPU_THREAD_SHUTDOWN:
			break;

		case CPU_THREAD_QUIT:
			// Just leave the thread, CPU is switching off thread.
			CPU_SetState(CPU_THREAD_NOT_RUNNING);
			return;

		default:
			ERROR_LOG(CPU, "CPU thread in unexpected state: %d", cpuThreadState);
			// Begin shutdown, otherwise we'd just spin on this bad state.
			CPU_SetState(CPU_THREAD_SHUTDOWN);
			break;
		}
	}

	if (coreState != CORE_ERROR) {
		coreState = CORE_POWERDOWN;
	}

	// Let's make sure the gpu has already cleaned up before we start freeing memory.
	if (gpu) {
		gpu->FinishEventLoop();
		gpu->SyncThread(true);
	}

	CPU_Shutdown();
	CPU_SetState(CPU_THREAD_NOT_RUNNING);
}

void Core_UpdateState(CoreState newState) {
	if ((coreState == CORE_RUNNING || coreState == CORE_NEXTFRAME) && newState != CORE_RUNNING)
		coreStatePending = true;
	coreState = newState;
	Core_UpdateSingleStep();
}

void System_Wake() {
	// Ping the threads so they check coreState.
	CPU_NextStateNot(CPU_THREAD_NOT_RUNNING, CPU_THREAD_SHUTDOWN);
	if (gpu) {
		gpu->FinishEventLoop();
	}
}

static bool pspIsInited = false;
static bool pspIsIniting = false;
static bool pspIsQuiting = false;

bool PSP_InitStart(const CoreParameter &coreParam, std::string *error_string) {
	if (pspIsIniting || pspIsQuiting) {
		return false;
	}

#if defined(_WIN32) && defined(_M_X64)
	INFO_LOG(BOOT, "PPSSPP %s Windows 64 bit", PPSSPP_GIT_VERSION);
#elif defined(_WIN32) && !defined(_M_X64)
	INFO_LOG(BOOT, "PPSSPP %s Windows 32 bit", PPSSPP_GIT_VERSION);
#else
	INFO_LOG(BOOT, "PPSSPP %s", PPSSPP_GIT_VERSION);
#endif
	coreParameter = coreParam;
	coreParameter.errorString = "";
	pspIsIniting = true;

	if (g_Config.bSeparateCPUThread) {
		Core_ListenShutdown(System_Wake);
		CPU_SetState(CPU_THREAD_PENDING);
		cpuThread = new std::thread(&CPU_RunLoop);
		cpuThreadID = cpuThread->get_id();
		cpuThread->detach();
	} else {
		CPU_Init();
	}

	*error_string = coreParameter.errorString;
	bool success = coreParameter.fileToStart != "";
	if (!success) {
		pspIsIniting = false;
	}
	return success;
}

bool PSP_InitUpdate(std::string *error_string) {
	if (pspIsInited || !pspIsIniting) {
		return true;
	}

	if (g_Config.bSeparateCPUThread && !CPU_IsReady()) {
		return false;
	}

	bool success = coreParameter.fileToStart != "";
	*error_string = coreParameter.errorString;
	if (success) {
		success = GPU_Init();
		if (!success) {
			PSP_Shutdown();
			*error_string = "Unable to initialize rendering engine.";
		}
	}
	pspIsInited = success;
	pspIsIniting = false;
	return true;
}

bool PSP_Init(const CoreParameter &coreParam, std::string *error_string) {
	PSP_InitStart(coreParam, error_string);

	if (g_Config.bSeparateCPUThread) {
		CPU_WaitStatus(cpuThreadReplyCond, &CPU_IsReady);
	}

	PSP_InitUpdate(error_string);
	return pspIsInited;
}

bool PSP_IsIniting() {
	return pspIsIniting;
}

bool PSP_IsInited() {
	return pspIsInited && !pspIsQuiting;
}

void PSP_Shutdown() {
	// Do nothing if we never inited.
	if (!pspIsInited && !pspIsIniting && !pspIsQuiting) {
		return;
	}

#ifndef MOBILE_DEVICE
	if (g_Config.bFuncHashMap) {
		MIPSAnalyst::StoreHashMap();
	}
#endif

	// Make sure things know right away that PSP memory, etc. is going away.
	pspIsQuiting = true;
	if (coreState == CORE_RUNNING)
		Core_UpdateState(CORE_ERROR);
	Core_NotifyShutdown();
	if (cpuThread != nullptr) {
		CPU_NextStateNot(CPU_THREAD_NOT_RUNNING, CPU_THREAD_SHUTDOWN);
		CPU_WaitStatus(cpuThreadReplyCond, &CPU_IsShutdown);
		delete cpuThread;
		cpuThread = 0;
		cpuThreadID = std::thread::id();
	} else {
		CPU_Shutdown();
	}
	GPU_Shutdown();
	g_paramSFO.Clear();
	host->SetWindowTitle(0);
	currentMIPS = 0;
	pspIsInited = false;
	pspIsIniting = false;
	pspIsQuiting = false;
	g_Config.unloadGameConfig();
}

void PSP_RunLoopUntil(u64 globalticks) {
	SaveState::Process();
	if (coreState == CORE_POWERDOWN || coreState == CORE_ERROR) {
		return;
	}

	// Switch the CPU thread on or off, as the case may be.
	bool useCPUThread = g_Config.bSeparateCPUThread;
	if (useCPUThread && cpuThread == nullptr) {
		// Need to start the cpu thread.
		Core_ListenShutdown(System_Wake);
		CPU_SetState(CPU_THREAD_RESUME);
		cpuThread = new std::thread(&CPU_RunLoop);
		cpuThreadID = cpuThread->get_id();
		cpuThread->detach();
		if (gpu) {
			gpu->SetThreadEnabled(true);
		}
		CPU_WaitStatus(cpuThreadReplyCond, &CPU_IsReady);
	} else if (!useCPUThread && cpuThread != nullptr) {
		CPU_SetState(CPU_THREAD_QUIT);
		CPU_WaitStatus(cpuThreadReplyCond, &CPU_IsShutdown);
		delete cpuThread;
		cpuThread = nullptr;
		cpuThreadID = std::thread::id();
		if (gpu) {
			gpu->SetThreadEnabled(false);
		}
	}

	if (cpuThread != nullptr) {
		// Tell the gpu a new frame is about to begin, before we start the CPU.
		gpu->SyncBeginFrame();

		cpuThreadUntil = globalticks;
		if (CPU_NextState(CPU_THREAD_RUNNING, CPU_THREAD_EXECUTE)) {
			// The CPU doesn't actually respect cpuThreadUntil well, especially when skipping frames.
			// TODO: Something smarter?  Or force CPU to bail periodically?
			while (!CPU_IsReady()) {
				gpu->RunEventsUntil(CoreTiming::GetTicks() + msToCycles(1000));
				if (coreState != CORE_RUNNING) {
					CPU_WaitStatus(cpuThreadReplyCond, &CPU_IsReady);
				}
			}
		} else {
			ERROR_LOG(CPU, "Unable to execute CPU run loop, unexpected state: %d", cpuThreadState);
		}
	} else {
		mipsr4k.RunLoopUntil(globalticks);
	}

	gpu->CleanupBeforeUI();
}

void PSP_RunLoopFor(int cycles) {
	PSP_RunLoopUntil(CoreTiming::GetTicks() + cycles);
}

CoreParameter &PSP_CoreParameter() {
	return coreParameter;
}

std::string GetSysDirectory(PSPDirectories directoryType) {
	switch (directoryType) {
	case DIRECTORY_CHEATS:
		return g_Config.memStickDirectory + "PSP/Cheats/";
	case DIRECTORY_GAME:
		return g_Config.memStickDirectory + "PSP/GAME/";
	case DIRECTORY_SAVEDATA:
		return g_Config.memStickDirectory + "PSP/SAVEDATA/";
	case DIRECTORY_SCREENSHOT:
		return g_Config.memStickDirectory + "PSP/SCREENSHOT/";
	case DIRECTORY_SYSTEM:
		return g_Config.memStickDirectory + "PSP/SYSTEM/";
	case DIRECTORY_PAUTH:
		return g_Config.memStickDirectory + "PAUTH/";
	case DIRECTORY_DUMP:
		return g_Config.memStickDirectory + "PSP/SYSTEM/DUMP/";
	case DIRECTORY_SAVESTATE:
		return g_Config.memStickDirectory + "PSP/PPSSPP_STATE/";
	case DIRECTORY_CACHE:
		return g_Config.memStickDirectory + "PSP/SYSTEM/CACHE/";
	// Just return the memory stick root if we run into some sort of problem.
	default:
		ERROR_LOG(FILESYS, "Unknown directory type.");
		return g_Config.memStickDirectory;
	}
}
