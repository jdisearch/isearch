/////////////////////////////////////////////////////////////
//
// Task Pool, thread unsafe.
// Author:qiuyu
// Date:Jul 12th,2019
//
////////////////////////////////////////////////////////////

#ifndef __CONCURRENT_TRANSACTION_EXECUTOR_H_
#define __CONCURRENT_TRANSACTION_EXECUTOR_H_

#include "Sem.h"
#include "ThreadQueue.h"
#include "SingletonBase.h"
#include "Thread.h"
#include "timestamp.h"

#include <stdint.h>
#include <map>
#include <vector>
#include <string>
#include <utility>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

// can never larger than 16
#define TASK_NUM_SHIFT 8
#define MAX_TRANS_ID ((1ULL << (64 - TASK_NUM_SHIFT)) - 1)
#define SIZE_VALID(size)                             \
(                                                    \
  {                                                  \
    bool rslt = true;                                \
    if (size <= 0 || size > (1ULL << TASK_NUM_SHIFT)) \
      rslt = false;                                  \
    rslt;                                            \
  }                                                  \
)

#define TASK_NUM(transId) (transId & ((1ULL << TASK_NUM_SHIFT) - 1))

class BasicRequest;
typedef AutoPtr<BasicRequest> BasicRequestPtr;

// user need to override this class
class BasicRequest : public HandleBase
{
private:
  uint64_t mTransactionId;
  uint64_t mExpiredWhen;
  SemPtr  mSem; // Sem is NULL means async trans
  
  // the high 16 bits for Error code and the others for task counter
  Atomic mFinishedReqNum;
  BasicRequestPtr mHeadRequest;
  BasicRequestPtr mNextRequest;

public:
  enum RequestError
  {
    // Notice, err code can not be manual set, must be increment automaticly
    eTimeout = 1,
    eProcFailed
  };

public:
  BasicRequest();
  virtual ~BasicRequest();
  
private:
  // user override function
  virtual int procRequest() = 0;
  // @Return value: success 0, error return -1
  virtual int procResponse(std::vector<BasicRequestPtr>& requests) = 0;

public:
  void setTransId(uint64_t transId) { mTransactionId = transId; }
  uint64_t getTransId() { return mTransactionId; }
  
  void setSem(const SemPtr sem) { mSem = sem; }
  int timeWait(const int64_t msTimeout) { return mSem->semTimeWait(msTimeout); }
  void wakeUp() { if (mSem) mSem->semPost(); }

  void setTimeout(int64_t timeout)
  {
    mExpiredWhen = timeout < 0 ? (uint64_t)-1 
      : (timeout + GET_TIMESTAMP() < timeout ? (uint64_t)-1 : timeout + GET_TIMESTAMP()); 
  }
  bool isTimeout() { return (uint64_t)GET_TIMESTAMP() >= mExpiredWhen; }

  void setErrorCode(BasicRequest::RequestError errCode);

private:
  void bindLinkList(
      BasicRequestPtr header,
      BasicRequestPtr next)
  {
    mHeadRequest = header;
    mNextRequest = next;
  }

  int procRequestPrev();
  int procResponsePrev(std::vector<BasicRequestPtr> &results);

  friend class ConcurrTransExecutor;
};

class ConcurrTransExecutor : public SingletonBase<ConcurrTransExecutor>
{
private:
  class ThreadShell : public Thread , public HandleBase
  {
  private:
    ConcurrTransExecutor *mOwner;

  public:
    ThreadShell(ConcurrTransExecutor *owner)
    :
    mOwner(owner)
    {
      // log_info("Construct ThreadShell");
    }

    virtual ~ThreadShell()
    { 
      // log_info("deconstruct ThreadShell"); 
    }
    virtual void run();

    void terminate() { mThreadState = Thread::eExited; }
  };
  typedef AutoPtr<ThreadShell> ThreadShellPtr;

private:
  uint64_t mBaseTransacId; // start with 1
  std::vector<ThreadShellPtr> mThreadPool;
  
  size_t mTransPoolMaxCapacity;
  ThreadQueue<BasicRequestPtr> mTransactionPool;

public:
  ConcurrTransExecutor();
  virtual ~ConcurrTransExecutor();

  bool executeTransAsync(std::vector<BasicRequestPtr> trans);

  // timeout -1 for wait forever
  bool executeTransSync(
      std::vector<BasicRequestPtr>& trans,
      int64_t msTimeout);

private:
  inline uint64_t createTransId(size_t taskNum)
  {
    bool rslt = SIZE_VALID(taskNum);
    if (!rslt) return 0;
    
    uint64_t transId = mBaseTransacId << TASK_NUM_SHIFT;
    transId += taskNum;
    mBaseTransacId = (mBaseTransacId >= MAX_TRANS_ID ? 1 : ++mBaseTransacId);

    return transId;
  }

  BasicRequestPtr getTask();

  void terminatePoolThread();
  int isPoolOverload(size_t currTransNum);
};

#endif // __CONCURRENT_TRANSACTION_EXECUTOR_H_
