/////////////////////////////////////////////////////////////
//
// Implementation of the LIRS cache.
// Author:qiuyu
// Date:Apr 22th,2019
//
////////////////////////////////////////////////////////////

#ifndef LIRS_CACHE_H__
#define LIRS_CACHE_H__

#include <stdint.h>
#include <map>
#include <string>
#include <utility>

// LIRS use two data structure to hold all cache data, LIRS stack and queue.
// Data to be described as hot data and cold data, hot data names LIR
// and cold data is HIR, all LIR data are located in the LIRS stack and 
// the others located either in the stack or queue; The HIR data also be
// divided into resident and non-resident, all resident HIR data are linked
// to be a small size queue

#define LIR_BLOCK 1            // Hot data
#define HIR_RESIDENT_BLOCK 2   // HIR is cold data
#define HIR_BLOCK_ONSTACK 4
#define HIR_BLOCK_ONQUEUE 8
#define HIR_BLOCK_SHARED 16 // shared Resident HIR entry reference between Stack and Queue
// 1.Unfixed data type(include either key or value):
//   unsigned long long, float, double, string
// 2.Except 1, others is fixed, such as the following:
//   char, short, int, the unsigned series that size is small than 8, and so on
// #define HIR_BLOCK_FIXED 32

typedef struct LirsEntry
{
  char sEntryState;
  // 1.we assume that the value is big enough, so the space cost in using shared entry 
  //   mechanism(two double link pointer) will be cheaper than clone the same entry
  // 2.use shared ptr let us implement the LRU cache with only one Hashmap
  struct LirsEntry *sStackPrev;
  struct LirsEntry *sStackNext;
  struct LirsEntry *sQueuePrev;
  struct LirsEntry *sQueueNext;
  std::string sKey;
  std::string sValue;

  void initEntry(
      const char state,
      struct LirsEntry *sPrev,
      struct LirsEntry *sNext,
      struct LirsEntry *qPrev,
      struct LirsEntry *qNext,
      const std::string &key,
      const std::string &value)
  {
    sEntryState = state;
    sStackPrev = sPrev;
    sStackNext = sNext;
    sQueuePrev = qPrev;
    sQueueNext = qNext;
#if (__cplusplus >= 201103L)
    sKey = std::move(key);
    sValue = std::move(value);
#else
    sKey = key;
    sValue = value;
#endif
  }
}LirsEntry_t;


class LirsCache
{
private:
  enum CacheRelevant
  {
    eQueueSizeRate = 1,  // 1%
    eMinCacheEntrySize = 100,
    eMaxCacheEntrySize = 500000
  };

private:
  typedef std::map<std::string, LirsEntry_t*>::iterator MapItr_t;

  class LirsStack
  {
  private:
    int mMaxLirEntryNum; // Maximum LIR entry number
    int mMaxStackSize;  // maximum real stack capacity, contain LIR + resident HIR + non-resident blocks
    int mCurrLirEntryNum;
    int mCurrStackSize;
    LirsEntry_t* mStackBottom;
    LirsEntry_t* mStackTop;

  public:
    LirsStack(const int maxLir, const int maxStackSize);
    virtual ~LirsStack();
    
    inline LirsEntry_t* getBottomOfStack() { return mStackBottom; }
    inline LirsEntry_t* getTopOfStack() { return mStackTop; }
    inline bool isLirEntryFull() { return mCurrLirEntryNum >= mMaxLirEntryNum; }
    inline bool isStackFull() { return mCurrStackSize >= mMaxStackSize; }
    void stackPrune(std::map<std::string, LirsEntry_t*> &entryMap);
    void releaseOneHirEntry(std::map<std::string, LirsEntry_t*> &entryMap);

    void appendEntry(LirsEntry_t *entry);
    void removeEntry(
        LirsEntry_t *entry, 
        std::map<std::string, LirsEntry_t*> &entryMap,
        const bool releaseEntry = true);
  };

  class LirsQueue
  {
  private:
    int mMaxQueueSize; // Maximum resident HIR entry number
    int mCurrQueueSize;
    LirsEntry_t *mQueueHead;
    LirsEntry_t *mQueueTail;

  public:
    LirsQueue(const int maxQueueSize);
    virtual ~LirsQueue();

    inline LirsEntry_t* getHeadOfQueue() { return mQueueHead; }
    inline LirsEntry_t* getTailOfQueue() { return mQueueTail; }
    inline bool isHirEntryFull() { return mCurrQueueSize >= mMaxQueueSize; }
    inline int getCurrQueueSize() { return mCurrQueueSize; }

    void appendEntry(LirsEntry_t *entry);
    void removeEntry(
        LirsEntry_t *entry, 
        std::map<std::string, LirsEntry_t*> &entryMap,
        const bool releaseEntry = true);
  };

public:
  explicit LirsCache(const int cacheSize = eMaxCacheEntrySize);
  virtual ~LirsCache();

  LirsEntry_t* findEntry(const std::string &key);
  
  // user convert all basic data type to string
  bool appendEntry(const std::string &key, const std::string &value);
  bool removeEntry(const std::string &key);

private:
  void adjustLirsCache(LirsEntry_t * entry);
  bool syntaxCheck();

private:
  int mCacheEntrySize;  // LIR and resident HIR block nums
  LirsStack* mBlockStack; // store all LIR blocks and some HIR blocks
  LirsQueue* mBlockQueue; // store all resident HIR blocks
  std::map<std::string, LirsEntry_t*> mEntryMap; // store all cache data for efficient search
  
  friend class LirsStack;
  friend class LirsQueue;
};

#endif // LIRS_CACHE_H__
