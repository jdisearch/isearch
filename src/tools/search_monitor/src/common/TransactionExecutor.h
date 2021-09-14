/////////////////////////////////////////////////////////////
//
// Task Pool, thread unsafe.
// Author:qiuyu
// Date:Jul 12th,2019
//
////////////////////////////////////////////////////////////

#ifndef __TRANSACTION_EXECUTOR_H_
#define __TRANSACTION_EXECUTOR_H_

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
class BasicResponse;
typedef AutoPtr<BasicRequest> BasicRequestPtr;
typedef AutoPtr<BasicResponse> BasicResponsePtr;

// user need to override this class
class BasicRequest : public HandleBase
{
private:
  uint64_t mTransactionId;
  uint64_t mExpiredWhen;
  SemPtr  mSem;

public:
  BasicRequest()
  :
  mTransactionId(0),
  mExpiredWhen(-1),
  mSem(NULL)
  {
  }

  virtual int procRequest(BasicResponsePtr& response) = 0;

  void setTransId(uint64_t transId) { mTransactionId = transId; }
  uint64_t getTransId() { return mTransactionId; }
  
  void setSem(const SemPtr sem) { mSem = sem; }
  void wakeUp()
  {
    if (mSem) mSem->semPost();
  }

  void setTimeout(int64_t timeout)
  {
    mExpiredWhen = timeout < 0 ? (uint64_t)-1 
      : (timeout + GET_TIMESTAMP() < timeout ? (uint64_t)-1 : timeout + GET_TIMESTAMP()); 
  }
  bool isTimeout() { return (uint64_t)GET_TIMESTAMP() >= mExpiredWhen; }
  
  int timeWait(const int64_t msTimeout) { return mSem->semTimeWait(msTimeout); }
};

class BasicResponse : public HandleBase
{
public:
  // for coalescing result
  virtual int procResponse(std::deque<BasicResponsePtr>& responses) = 0;
};

class TransactionExecutor : public SingletonBase<TransactionExecutor>
{
private:
  class ThreadShell : public Thread , public HandleBase
  {
  private:
    TransactionExecutor *mOwner;

  public:
    ThreadShell(TransactionExecutor *owner)
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
  ThreadMutex mRespLock;
  std::vector<ThreadShellPtr> mThreadPool;
  
  size_t mTransPoolMaxCapacity;
  ThreadQueue<BasicRequestPtr> mTransactionPool;
  std::map<uint64_t, std::deque<BasicResponsePtr> > mResponsePool;

public:
  TransactionExecutor();
  virtual ~TransactionExecutor();

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

  void createEmptyResponse(uint64_t transId);
  BasicRequestPtr getTask();
  void recvResponse(uint64_t transId, const BasicResponsePtr response);
  void removeResponse(uint64_t transId);

  bool transFinished(
      uint64_t transId, 
      const BasicResponsePtr response,
      std::deque<BasicResponsePtr>& resps);

  void terminatePoolThread();
  int isPoolOverload(size_t currTransNum);
};

#endif // __TRANSACTION_EXECUTOR_H_
