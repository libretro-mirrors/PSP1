#define MIXBUFFER_QUEUE (512 * 16)
static s32 *mixBuffer;
static s16 mixBufferQueue[MIXBUFFER_QUEUE];

static int mixBufferHead = 0;
static int mixBufferTail = 0;
static int mixBufferCount = 0; // sacrifice 4 bytes for a simpler implementation. may optimize away in the future.

#define queue_room() (MIXBUFFER_QUEUE - mixBufferCount)

static void queue_clear(void)
{
   mixBufferHead = 0;
   mixBufferTail = 0;
   mixBufferCount = 0;
}

// Gets pointers to write to directly.

static void queue_pushPointers(size_t size, s16 **dest1, size_t *sz1, s16 **dest2, size_t *sz2)
{
   *dest1 = (s16*)&mixBufferQueue[mixBufferTail];
   if (mixBufferTail + (int)size < MIXBUFFER_QUEUE)
   {
      *sz1 = size;
      mixBufferTail += (int)size;
      if (mixBufferTail == MIXBUFFER_QUEUE)
         mixBufferTail = 0;
      *dest2 = 0;
      *sz2 = 0;
   }
   else
   {
      *sz1 = MIXBUFFER_QUEUE - mixBufferTail;
      mixBufferTail = (int)(size - *sz1);
      *dest2 = (s16*)&mixBufferQueue[0];
      *sz2 = mixBufferTail;
   }
   mixBufferCount += (int)size;
}

static void queue_popPointers(size_t size, const s16 **src1, size_t *sz1, const s16 **src2, size_t *sz2)
{
   if ((int)size > mixBufferCount)
      size = mixBufferCount;
   *src1 = (s16*)&mixBufferQueue[mixBufferHead];
   if (mixBufferHead + size < MIXBUFFER_QUEUE)
   {
      *sz1 = size;
      mixBufferHead += (int)size;
      if (mixBufferHead == MIXBUFFER_QUEUE)
         mixBufferHead = 0;
      *src2 = 0;
      *sz2 = 0;
   }
   else
   {
      *sz1 = MIXBUFFER_QUEUE - mixBufferHead;
      mixBufferHead = (int)(size - *sz1);
      *src2 = (s16*)&mixBufferQueue[0];
      *sz2 = mixBufferHead;
   }
   mixBufferCount -= (int)size;
}

static void queue_DoState(PointerWrap &p)
{
   int size = MIXBUFFER_QUEUE;
   p.Do(size);
   if (size != MIXBUFFER_QUEUE)
   {
      ERROR_LOG(COMMON, "Savestate failure: Incompatible queue size.");
      return;
   }
   p.DoArray<s16>(mixBufferQueue, MIXBUFFER_QUEUE);
   p.Do(mixBufferHead);
   p.Do(mixBufferTail);
   p.Do(mixBufferCount);
   p.DoMarker("FixedSizeQueueLR");
}
