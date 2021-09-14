/////////////////////////////////////////////////////////////
//
// Implementation of the LIRS cache.
// Author:qiuyu
// Date:Apr 22th,2019
//
////////////////////////////////////////////////////////////

#include "lirs_cache.h"
#include "log.h"

#include <assert.h>

////////////////////////////////////////////////////////////
//           stack relevant
////////////////////////////////////////////////////////////
LirsCache::LirsStack::LirsStack(
    const int maxLirNum,
    const int maxStackSize)
:
mMaxLirEntryNum(maxLirNum),
mMaxStackSize(maxStackSize),
mCurrLirEntryNum(0),
mCurrStackSize(0),
mStackBottom(NULL),
mStackTop(NULL)
{
}

LirsCache::LirsStack::~LirsStack()
{
  LirsEntry_t *prev, *curr = mStackBottom;
  while (curr)
  {
    prev = curr;
    curr = curr->sStackNext;

    if (prev->sEntryState & HIR_BLOCK_SHARED) 
      prev->sEntryState &= ~HIR_BLOCK_SHARED;
    else 
      delete prev;
  }
}

void
LirsCache::LirsStack::removeEntry(
    LirsEntry_t *entry,
    std::map<std::string, LirsEntry_t*> &entryMap,
    const bool releaseEntry)
{
  if (!entry || !(entry->sEntryState & HIR_BLOCK_ONSTACK))
  {
    log_error("internal error, entryEmpty:%d.", !entry);
    return;
  }

  if (!entry->sStackPrev)
  {
    assert(entry == mStackBottom);

    mStackBottom = entry->sStackNext;
    if (!mStackBottom) mStackTop = NULL;
    else mStackBottom->sStackPrev = NULL;
  }
  else if (!entry->sStackNext)
  {
    assert(entry == mStackTop);

    mStackTop = entry->sStackPrev;
    if (!mStackTop) mStackBottom = NULL;
    else mStackTop->sStackNext = NULL;
  }
  else
  {
    assert(entry != mStackBottom && entry != mStackTop);

    entry->sStackPrev->sStackNext = entry->sStackNext;
    entry->sStackNext->sStackPrev = entry->sStackPrev;
  }
  
  char &state = entry->sEntryState;
  bool canRelease = (releaseEntry && !(state & HIR_BLOCK_SHARED));
  if (state & LIR_BLOCK) mCurrLirEntryNum--;
  state &= (~HIR_BLOCK_ONSTACK & ~HIR_BLOCK_SHARED & ~LIR_BLOCK); 
  mCurrStackSize--;
  
  if (canRelease)
  {
    log_info("remove entry, key:%s", entry->sKey.c_str());
    entryMap.erase(entry->sKey);
    delete entry;
    entry = NULL;
  }
  else
  {
    entry->sStackPrev = entry->sStackNext = NULL;
  }
  
  return;
}

// 1.when call this function, must be has enough space for the appending entry
// 2.the entry must be a non-exist entry in the stack
void
LirsCache::LirsStack::appendEntry(LirsEntry_t *entry)
{
  if (!entry)
  {
    log_error("append empty entry.");
    assert(false);
    return;
  }
   
  char &state = entry->sEntryState;
  if (state < 0 || (mCurrLirEntryNum >= mMaxLirEntryNum && (state & LIR_BLOCK)))
  {
    log_error("no enough space for the Lir entry");
    assert(false);
    return;
  }

  if (mCurrStackSize >= mMaxStackSize)
  {
    log_error("no enough space for the Hir entry");
    assert(false);
    return;
  }
  
  // has enough space, append it
  if (!mStackTop)
  {
    // the first one
    mStackTop = mStackBottom = entry;
    entry->sStackPrev = NULL;
    entry->sStackNext = NULL;
  }
  else
  {
    // append to the behind of the top entry
    mStackTop->sStackNext = entry;
    entry->sStackPrev = mStackTop;
    mStackTop = entry;
    mStackTop->sStackNext = NULL;
  }
  
  if (state & LIR_BLOCK) mCurrLirEntryNum++;
  mCurrStackSize++;
  
  state |= HIR_BLOCK_ONSTACK;
  if (state & (HIR_BLOCK_ONQUEUE | HIR_RESIDENT_BLOCK)) state |= HIR_BLOCK_SHARED;

  return;
}

// evicted all HIR blocks those located in the bottom of stack
void
LirsCache::LirsStack::stackPrune(std::map<std::string, LirsEntry_t*> &entryMap)
{
  if (!mStackBottom) return;

  while (mStackBottom)
  {
    if (mStackBottom->sEntryState & LIR_BLOCK) break;
    removeEntry(mStackBottom, entryMap);
  }
  
  return;
}

// release one hir block from the bottom of the stack
void
LirsCache::LirsStack::releaseOneHirEntry(std::map<std::string, LirsEntry_t*> &entryMap)
{
  if (!mStackBottom) return;
  
  LirsEntry_t *curr = mStackBottom->sStackNext;
  while (curr)
  {
    if (curr->sEntryState & LIR_BLOCK) curr = curr->sStackNext;

    // remove the entry
    removeEntry(curr, entryMap, true);
    break;
  }

  return;
}

///////////////////////////////////////////////////////////
//       Lirs queue relevant
///////////////////////////////////////////////////////////
LirsCache::LirsQueue::LirsQueue(const int maxQueueSize)
:
mMaxQueueSize(maxQueueSize),
mCurrQueueSize(0),
mQueueHead(NULL),
mQueueTail(NULL)
{
}

LirsCache::LirsQueue::~LirsQueue()
{
  LirsEntry_t *prev, *curr = mQueueHead;
  while (curr)
  {
    prev = curr;
    curr = curr->sQueueNext;

    if (prev->sEntryState & HIR_BLOCK_SHARED)
      prev->sEntryState &= ~HIR_BLOCK_SHARED;
    else
      delete prev;
  }
}

// evicted the resident HIR block from the queue
// use flag 'release' to forbidden the caller to release the entry
// if someone holding it current now
void
LirsCache::LirsQueue::removeEntry(
    LirsEntry_t *entry,
    std::map<std::string, LirsEntry_t*> &entryMap,
    const bool release)
{
  if (!entry)
  {
    log_error("can not remove an empty entry.");
    return;
  }

  char &state = entry->sEntryState;
  if (!(state & HIR_RESIDENT_BLOCK))
  {
    assert(false);
    log_error("incorrect entry state.");
    return;
  }
  
  if (!entry->sQueuePrev)
  {
    mQueueHead = entry->sQueueNext;
    if (!mQueueHead) mQueueTail = NULL;
    else mQueueHead->sQueuePrev = NULL;
  }
  else if (!entry->sQueueNext)
  {
    mQueueTail = entry->sQueuePrev;
    if (!mQueueTail) mQueueHead = NULL;
    else mQueueTail->sQueueNext = NULL;
  }
  else
  {
    entry->sQueuePrev->sQueueNext = entry->sQueueNext;
    entry->sQueueNext->sQueuePrev = entry->sQueuePrev;
  }
  
  // double check
  if (release && !(state & HIR_BLOCK_ONSTACK) && !(state & HIR_BLOCK_SHARED))
  {
    log_info("remove entry, key:%s", entry->sKey.c_str());
    entryMap.erase(entry->sKey);
    delete entry;
    entry = NULL;
  }
  else
  {
    // clear flag
    entry->sQueuePrev = entry->sQueueNext = NULL;
    state &= (~HIR_BLOCK_ONQUEUE & ~HIR_BLOCK_SHARED & ~HIR_RESIDENT_BLOCK); 
  }
  mCurrQueueSize--;

  return;
}

// when call this function, queue should has enough remaining space for appending
void
LirsCache::LirsQueue::appendEntry(LirsEntry_t *entry)
{
  if (!entry || (entry->sEntryState & LIR_BLOCK))
  {
    log_error("empty entry:%d.", entry == NULL);
    assert(false);
    return;
  }
  
  char &state = entry->sEntryState;
  if (state < 0 || mCurrQueueSize >= mMaxQueueSize)
  {
    log_error("incorrect queue data.");
    return;
  }

  // just append to the tail directly
  if (!mQueueTail)
  {
    mQueueHead = mQueueTail = entry;
    mQueueHead->sQueuePrev = NULL;
    mQueueTail->sQueueNext = NULL;
  }
  else
  {
    mQueueTail->sQueueNext = entry;
    entry->sQueuePrev = mQueueTail;
    mQueueTail = entry;
    mQueueTail->sQueueNext = NULL;
  }
  mCurrQueueSize++;

  state |= (HIR_BLOCK_ONQUEUE | HIR_RESIDENT_BLOCK);
  state &= ~LIR_BLOCK;
  if (state & HIR_BLOCK_ONSTACK) state |= HIR_BLOCK_SHARED;

  return;
}

///////////////////////////////////////////////////////////
//       LIRS cache relevant
///////////////////////////////////////////////////////////
LirsCache::LirsCache(const int cacheSize)
:
mCacheEntrySize(cacheSize)
{
  if (mCacheEntrySize < eMinCacheEntrySize || mCacheEntrySize > eMaxCacheEntrySize)
    mCacheEntrySize = mCacheEntrySize < eMinCacheEntrySize ? eMinCacheEntrySize : mCacheEntrySize;

  int queueSize = mCacheEntrySize * eQueueSizeRate / 100;
  int maxLirEntryNum = mCacheEntrySize - queueSize;
  int maxStackSize = mCacheEntrySize + queueSize; // the extra queue size for holding non-resident HIR blocks

  mBlockStack = new LirsStack(maxLirEntryNum, maxStackSize);
  mBlockQueue = new LirsQueue(queueSize);
}

LirsCache::~LirsCache()
{
  if (mBlockStack) delete mBlockStack;
  if (mBlockQueue) delete mBlockQueue;
}

// find the key and adjust the lirs cache
LirsEntry_t*
LirsCache::findEntry(const std::string &key)
{
  MapItr_t itr = mEntryMap.find(key);
  if (itr == mEntryMap.end()) return NULL;

  LirsEntry_t *entry = itr->second;
  assert(entry != NULL);
  if (!entry || !(entry->sEntryState & HIR_RESIDENT_BLOCK)) return NULL;

if (key == "182")
{
bool debug = false;
while (debug)
{
  log_info("xxxxxxx77xx11");
}
}
  // hit Lir or Resident Hir block, adjust the cache
  adjustLirsCache(entry);
  syntaxCheck();
  return entry;
}

// 1.if exist, update the value
// 2.append a new entry
bool
LirsCache::appendEntry(
    const std::string &key,
    const std::string &value)
{
  // find in the stack first
  LirsEntry_t *entry = NULL; 
  MapItr_t itr = mEntryMap.find(key);
  if (itr != mEntryMap.end())
  {
    entry = itr->second;
#if (__cplusplus >= 201103L)
    // c++0x, use rvalue reference, value can not be used any more
    entry->sValue = std::move(value);
#else
    entry->sValue = value;
#endif // __cplusplus >= 201103L
    adjustLirsCache(entry);
    syntaxCheck();

    log_info("update entry, key:%s, value:%s, state:%d", key.c_str(),\
        value.c_str(), entry->sEntryState);
    return true;
  }
  
  // append a new entry
  entry = new LirsEntry_t();
  if (!entry)
  {
    log_error("allocate memory failed.");
    return false;
  }
  entry->initEntry(0, NULL, NULL, NULL, NULL, key, value);
  char &state = entry->sEntryState;
  
  // add into the map
  mEntryMap[key] = entry;

  // make sure have enough space for appending
  bool isLirFull = mBlockStack->isLirEntryFull();
  bool isStackFull = mBlockStack->isStackFull();
  if (!isLirFull)
  {
    if (isStackFull) mBlockStack->releaseOneHirEntry(mEntryMap);

    // add as a lir entry
    state |= LIR_BLOCK;
    mBlockStack->appendEntry(entry);
    syntaxCheck();
    
    log_info("append entry, key:%s, value:%s, state:%d",\
        key.c_str(), value.c_str(), entry->sEntryState);
    return true;
  }

  // add as a resident HIR block
  bool isQueueFull = mBlockQueue->isHirEntryFull();
  if (isQueueFull || isStackFull)
  {
    if (isQueueFull)
    {
      // remove resident HIR block from queue
      LirsEntry_t *head = mBlockQueue->getHeadOfQueue();
      mBlockQueue->removeEntry(head, mEntryMap);
    }

    // check whether the stack is full or not
    if (isStackFull)
    {
      // remove the lir block in the bottom
      LirsEntry_t *bottom = mBlockStack->getBottomOfStack();
      mBlockStack->removeEntry(bottom, mEntryMap, false);
      mBlockQueue->appendEntry(bottom);
      mBlockStack->stackPrune(mEntryMap);

      // append entry as a lir block
      state |= LIR_BLOCK;
      mBlockStack->appendEntry(entry);
      syntaxCheck();
      
      log_info("append entry, key:%s, value:%s, state:%d",\
          key.c_str(), value.c_str(), entry->sEntryState);
      return true;
    }
  }
  
  // append to both the stack and the queue as an resident block
  // state |= (HIR_RESIDENT_BLOCK | HIR_BLOCK_SHARED);
  mBlockStack->appendEntry(entry);
  mBlockQueue->appendEntry(entry);
  assert(state == 30);
  syntaxCheck();
  
  log_info("append entry, key:%s, value:%s, state:%d",\
      key.c_str(), value.c_str(), entry->sEntryState);
  return true;
}

bool
LirsCache::removeEntry(const std::string &key)
{
  MapItr_t itr = mEntryMap.find(key);
  if (itr == mEntryMap.end()) return true;

  LirsEntry_t *entry = itr->second;
  char state = entry->sEntryState;
  
  // remove from the stack
  if (state & HIR_BLOCK_ONSTACK)
  {
    mBlockStack->removeEntry(entry, mEntryMap);

    // try to conduct a pruning
    mBlockStack->stackPrune(mEntryMap);
  }

  // remove from the queue
  if (state & HIR_BLOCK_ONQUEUE)
  {
    mBlockQueue->removeEntry(entry, mEntryMap);
  }

  return true;
}

// entry must be exist in the cache, even if it's a non-resident block 
void
LirsCache::adjustLirsCache(LirsEntry_t *entry)
{
  char &state = entry->sEntryState;
  if (state & LIR_BLOCK)
  {
    // lir block
    // bool inStackBottom = (entry->sStackPrev == NULL);
    mBlockStack->removeEntry(entry, mEntryMap, false);

    // maybe the removed entry is bottom, try to conduct a stack pruning
    mBlockStack->stackPrune(mEntryMap);
    
    state |= LIR_BLOCK;
  }
  else
  {
    // hir block
    if (state & HIR_RESIDENT_BLOCK)
    {
      // resident hir block
      if (state & HIR_BLOCK_ONSTACK)
      {
        // evicted from queue
        mBlockQueue->removeEntry(entry, mEntryMap, false);
        
        // move the bottom entry in the stack to the end of the queue
        LirsEntry_t *bottom = mBlockStack->getBottomOfStack();
        mBlockStack->removeEntry(bottom, mEntryMap, false);
        mBlockQueue->appendEntry(bottom);
        
        // evicted myself from stack
        mBlockStack->removeEntry(entry, mEntryMap, false);
        mBlockStack->stackPrune(mEntryMap);

        state |= LIR_BLOCK;
      }
      else
      {
        // 1.leave its status in HIR and move this block to the end of the queue
        mBlockQueue->removeEntry(entry, mEntryMap, false);
        mBlockQueue->appendEntry(entry);
        
        // 2.append to the stack
        bool isStackFull = mBlockStack->isStackFull();
        if (isStackFull)
        {
          // remove the first HIR entry from stack
          mBlockStack->releaseOneHirEntry(mEntryMap);
        }
      }
    }
    else
    {
      // non-resident hir block, block must be in the stack, if not in the stack,
      // it must be a new entry that we should call appendEntry function to add it
      if (!(state & HIR_BLOCK_ONSTACK) || (state & HIR_BLOCK_ONQUEUE))
      {
        log_error("internal error.");
        assert(false);
        return;
      }

      // remove the resident HIR block from the head of queue first
      LirsEntry_t *head = mBlockQueue->getHeadOfQueue();
      mBlockQueue->removeEntry(head, mEntryMap, true);

      // move the entry in the bottom of the stack into the tail of the queue
      LirsEntry_t *bottom = mBlockStack->getBottomOfStack();
      mBlockStack->removeEntry(bottom, mEntryMap, false);
      mBlockQueue->appendEntry(bottom);

      // remove the entry from the stack first, then conduct stack prune
      mBlockStack->removeEntry(entry, mEntryMap, false);

      mBlockStack->stackPrune(mEntryMap);

      state |= LIR_BLOCK;
    }
  }
  
  // append this entry to the top of the stack
  mBlockStack->appendEntry(entry);

  return;
}

// check LIRS cache
bool LirsCache::syntaxCheck()
{
  int stackBlockNum = 0;
  int stackLirBlockNum = 0;
  int stackRHirBlockNum = 0;
  int stackNRHirBlockNum = 0;
  int queueSharedBlockNum = 0;
  
  // check stack
  if (mBlockStack)
  {
    LirsEntry_t *bottom = mBlockStack->getBottomOfStack();
    LirsEntry_t *top = mBlockStack->getTopOfStack();

    char state;
    LirsEntry_t *prev = NULL, *curr = bottom;
    while (curr)
    {
      state = curr->sEntryState;
      if (state <= 0 ||
          state > (HIR_BLOCK_SHARED + HIR_BLOCK_ONQUEUE + HIR_BLOCK_ONSTACK + HIR_RESIDENT_BLOCK))
      {
        log_error("incorrect entry state.");
        assert(false);
        return false;
      }

      if (!(state & HIR_BLOCK_ONSTACK))
      {
          log_error("incorrect LIR block state. state:%d", state);
          assert(false);
          return false;
      }

      stackBlockNum++;
      
      if (state & LIR_BLOCK)
      {
        if ((state & HIR_RESIDENT_BLOCK)
            || (state & HIR_BLOCK_ONQUEUE)
            || (state & HIR_BLOCK_SHARED))
        {
          log_error("incorrect LIR block. state:%d", state);
          assert(false);
          return false;
        }
        stackLirBlockNum++;
      }
      else if (state & HIR_RESIDENT_BLOCK)
      {
        if (!(state & HIR_BLOCK_ONQUEUE)
            || !(state & HIR_BLOCK_SHARED))
        {
          log_error("incorrect LIR block. state:%d", state);
          assert(false);
          return false;
        }
        stackRHirBlockNum++;
      }
      else
      {
        if ((state & HIR_BLOCK_ONQUEUE)
            || (state & HIR_BLOCK_SHARED))
        {
          log_error("incorrect LIR block. state:%d", state);
          assert(false);
          return false;
        }
        stackNRHirBlockNum++;
      }
      
      prev = curr;
      curr = curr->sStackNext;
      if (curr && prev != curr->sStackPrev)
      {
        log_error("incorrect double link.");
        assert(false);
        return false;
      }
    }

    assert(prev == top);
  }

  // check cache size
  if (stackRHirBlockNum > mBlockQueue->getCurrQueueSize())
  {
    log_error("check RHir block failed.");
    assert(false);
    return false;
  }

  // check queue
  if (mBlockQueue)
  {
    LirsEntry_t *head = mBlockQueue->getHeadOfQueue();
    LirsEntry_t *tail = mBlockQueue->getTailOfQueue();
    
    char state;
    LirsEntry_t *prev = NULL, *curr = head;
    while (curr)
    {
      state = curr->sEntryState;
      if (state <= 0 ||
          state > (HIR_BLOCK_SHARED + HIR_BLOCK_ONQUEUE + HIR_BLOCK_ONSTACK + HIR_RESIDENT_BLOCK))
      {
        log_error("incorrect entry state.");
        assert(false);
        return false;
      }

      if (!(state & HIR_BLOCK_ONQUEUE) || !(state & HIR_RESIDENT_BLOCK))
      {
          log_error("incorrect Resident HIR block state. state:%d", state);
          assert(false);
          return false;
      }

      if (state & LIR_BLOCK)
      {
        log_error("incorrect Resident HIR block state. state:%d", state);
        assert(false);
        return false;
      }
      
      if (state & HIR_BLOCK_ONSTACK)
      {
        if (!(state & HIR_BLOCK_SHARED))
        {
          log_error("incorrect Resident HIR block state. state:%d", state);
          assert(false);
          return false;
        }
        
        queueSharedBlockNum++;
      }
      
      prev = curr;
      curr = curr->sQueueNext;
      if (curr && prev != curr->sQueuePrev)
      {
        log_error("incorrect double link.");
        assert(false);
        return false;
      }
    }

    assert(prev == tail);
  }
  
  if (stackRHirBlockNum != queueSharedBlockNum)
  {
    log_error("shared pointer occur error.");
    assert(false);
    return false;
  }

  return true;
}
