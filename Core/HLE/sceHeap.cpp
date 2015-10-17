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

#include "Common/ChunkFile.h"
#include "Core/MemMap.h"
#include "Core/Reporting.h"
#include "Core/HLE/HLE.h"
#include "Core/HLE/FunctionWrappers.h"
#include "Core/HLE/sceKernelMemory.h"
#include "Core/HLE/sceHeap.h"
#include "Core/Util/BlockAllocator.h"
#include <map>

struct Heap {
	Heap():alloc(4) {}

	u32 size;
	u32 address;
	bool fromtop;
	BlockAllocator alloc;

	void DoState (PointerWrap &p) {
		p.Do(size);
		p.Do(address);
		p.Do(fromtop);
		p.Do(alloc);
	}
};

std::map<u32,Heap*> heapList;

static Heap *getHeap(u32 addr) {
	auto found = heapList.find(addr);
	if (found == heapList.end()) {
		return NULL;
	}
	return found->second;
}

void __HeapDoState(PointerWrap &p) {
	auto s = p.Section("sceHeap", 1, 2);
	if (!s)
		return;

	if (s >= 2) {
		p.Do(heapList);
	}
}

enum SceHeapAttr {
	PSP_HEAP_ATTR_HIGHMEM = 0x4000,
	PSP_HEAP_ATTR_EXT     = 0x8000,
};

void __HeapInit() {
	heapList.clear();
}

static int sceHeapReallocHeapMemory(u32 heapAddr, u32 memPtr, int memSize) {
	return 0;
}

static int sceHeapReallocHeapMemoryWithOption(u32 heapPtr, u32 memPtr, int memSize, u32 paramsPtr) {
	return 0;
}

static int sceHeapFreeHeapMemory(u32 heapAddr, u32 memAddr) {
	Heap *heap = getHeap(heapAddr);
	if (!heap)
		return SCE_KERNEL_ERROR_INVALID_ID;

	DEBUG_LOG(HLE, "sceHeapFreeHeapMemory(%08x, %08x)", heapAddr, memAddr);
	// An invalid address will crash the PSP, but 0 is always returns success.
	if (memAddr == 0) {
		return 0;
	}

	if (!heap->alloc.FreeExact(memAddr)) {
		return SCE_KERNEL_ERROR_INVALID_POINTER;
	}
	return 0;
}

static int sceHeapGetMallinfo(u32 heapAddr, u32 infoPtr) {
	return 0;
}

static u32 sceHeapAllocHeapMemoryWithOption(u32 heapAddr, u32 memSize, u32 paramsPtr) {
	Heap *heap = getHeap(heapAddr);
	u32 grain = 4;
	if (!heap)
		return 0;

	// 0 is ignored.
	if (paramsPtr != 0) {
		u32 size = Memory::Read_U32(paramsPtr);
		if (size < 8)
			return 0;
		grain = Memory::Read_U32(paramsPtr + 4);
	}

	DEBUG_LOG(HLE,"sceHeapAllocHeapMemoryWithOption(%08x, %08x, %08x)", heapAddr, memSize, paramsPtr);
	// There's 8 bytes at the end of every block, reserved.
	memSize += 8;
	u32 addr = heap->alloc.AllocAligned(memSize, grain, grain, true);
	return addr;
}

static int sceHeapGetTotalFreeSize(u32 heapAddr) {
	Heap *heap = getHeap(heapAddr);
	if (!heap)
		return SCE_KERNEL_ERROR_INVALID_ID;

	DEBUG_LOG(HLE, "sceHeapGetTotalFreeSize(%08x)", heapAddr);
	u32 free = heap->alloc.GetTotalFreeBytes();
	if (free >= 8) {
		// Every allocation requires an extra 8 bytes.
		free -= 8;
	}
	return free;
}

static int sceHeapIsAllocatedHeapMemory(u32 heapPtr, u32 memPtr) {
	if (!Memory::IsValidAddress(memPtr))
		return SCE_KERNEL_ERROR_INVALID_POINTER;

	DEBUG_LOG(HLE, "sceHeapIsAllocatedHeapMemory(%08x, %08x)", heapPtr, memPtr);
	Heap *heap = getHeap(heapPtr);
	// An invalid heap is fine, it's not a member of this heap one way or another.
	// Only an exact address matches.  Off by one crashes, and off by 4 says no.
	if (heap && heap->alloc.GetBlockStartFromAddress(memPtr) == memPtr) {
		return 1;
	}
	return 0;
}

static int sceHeapDeleteHeap(u32 heapAddr) {
	Heap *heap = getHeap(heapAddr);
	if (!heap)
		return SCE_KERNEL_ERROR_INVALID_ID;

	DEBUG_LOG(HLE, "sceHeapDeleteHeap(%08x)", heapAddr);
	heapList.erase(heapAddr);
	delete heap;
	return 0;
}

static int sceHeapCreateHeap(const char* name, u32 heapSize, int attr, u32 paramsPtr) {
	if (name == NULL)
		return 0;
	int allocSize = (heapSize + 3) & ~3;

	Heap *heap = new Heap;
	heap->size = allocSize;
	heap->fromtop = (attr & PSP_HEAP_ATTR_HIGHMEM) != 0;
	u32 addr = userMemory.Alloc(heap->size, heap->fromtop, "Heap");
	if (addr == (u32)-1) {
		delete heap;
		return 0;
	}
	heap->address = addr;

	// Some of the heap is reserved by the implementation (the first 128 bytes, and 8 after each block.)
	heap->alloc.Init(heap->address + 128, heap->size - 128);
	heapList[heap->address] = heap;
	DEBUG_LOG(HLE, "%08x=sceHeapCreateHeap(%s, %08x, %08x, %08x)", heap->address, name, heapSize, attr, paramsPtr);
	return heap->address;
}

static u32 sceHeapAllocHeapMemory(u32 heapAddr, u32 memSize) {
	Heap *heap = getHeap(heapAddr);
	if (!heap) {
		// Yes, not 0 (returns a pointer), but an error code.  Strange.
		return SCE_KERNEL_ERROR_INVALID_ID;
	}

	DEBUG_LOG(HLE, "sceHeapAllocHeapMemory(%08x, %08x)", heapAddr, memSize);
	// There's 8 bytes at the end of every block, reserved.
	memSize += 8;
	// Always goes down, regardless of whether the heap is high or low.
	u32 addr = heap->alloc.Alloc(memSize, true);
	return addr;
}


static const HLEFunction sceHeap[] = 
{
	{0X0E875980, &WrapI_UUI<sceHeapReallocHeapMemory>,            "sceHeapReallocHeapMemory",           'i', "xxi" },
	{0X1C84B58D, &WrapI_UUIU<sceHeapReallocHeapMemoryWithOption>, "sceHeapReallocHeapMemoryWithOption", 'i', "xxix"},
	{0X2ABADC63, &WrapI_UU<sceHeapFreeHeapMemory>,                "sceHeapFreeHeapMemory",              'i', "xx"  },
	{0X2A0C2009, &WrapI_UU<sceHeapGetMallinfo>,                   "sceHeapGetMallinfo",                 'i', "xx"  },
	{0X2B7299D8, &WrapU_UUU<sceHeapAllocHeapMemoryWithOption>,    "sceHeapAllocHeapMemoryWithOption",   'x', "xxx" },
	{0X4929B40D, &WrapI_U<sceHeapGetTotalFreeSize>,               "sceHeapGetTotalFreeSize",            'i', "x"   },
	{0X7012BBDD, &WrapI_UU<sceHeapIsAllocatedHeapMemory>,         "sceHeapIsAllocatedHeapMemory",       'i', "xx"  },
	{0X70210B73, &WrapI_U<sceHeapDeleteHeap>,                     "sceHeapDeleteHeap",                  'i', "x"   },
	{0X7DE281C2, &WrapI_CUIU<sceHeapCreateHeap>,                  "sceHeapCreateHeap",                  'i', "sxix"},
	{0XA8E102A0, &WrapU_UU<sceHeapAllocHeapMemory>,               "sceHeapAllocHeapMemory",             'x', "xx"  },
};

void Register_sceHeap()
{
	RegisterModule("sceHeap", ARRAY_SIZE(sceHeap), sceHeap);
}
