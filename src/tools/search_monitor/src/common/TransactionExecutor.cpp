/////////////////////////////////////////////////////////////
//
// Task Pool, thread unsafe.
// Author:qiuyu
// Date:Jul 12th,2019
//
////////////////////////////////////////////////////////////

#include "TransactionExecutor.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define MAX_POOL_CAPACITY 1000000

TransactionExecutor::TransactionExecutor()
:
mBaseTransacId(1)
{
  log_info("construct TransactionExecutor!");
  // create thread pool
  // user configurable with config(CPU-bound and IO-bound)
  int threadNum = 3;
  for (int idx = 0; idx < threadNum; idx++)
  {
    mThreadPool.push_back(new ThreadShell(this));
    mThreadPool[idx]->start();
  }
  
  // user configurable
  size_t cap = 10000000;
  mTransPoolMaxCapacity = cap > MAX_POOL_CAPACITY ? MAX_POOL_CAPACITY : cap;
}

TransactionExecutor::~TransactionExecutor()
{
  terminatePoolThread();
  // wait for all thread finished(the normal way is to call join)
  sleep(3); 
  // log_info("deconstruct TransactionExecutor!");
}

bool
TransactionExecutor::executeTransAsync(std::vector<BasicRequestPtr> trans)
{
  size_t transNum = trans.size();
  int ret = isPoolOverload(transNum);
  if (ret >= 0)
  {
    // not overload
  }
  else if (ret == -1)
  {
    // current pool size more than 3/4 of the capacity, pay attention to it
    log_error("request too fast, trans pool will be full soon, pay attention!");
  }
  else
  {
    // pool is full, discard this trans
    log_error("trans pool is full, need to limit the inputs, discard this request!");
    return false;
  }

  uint64_t transId = createTransId(transNum);
  if (transId == 0)
  {
    log_error("get trans id faield, size:%zu, currTransId:%" PRIu64, transNum, mBaseTransacId);
    return false;
  }
  
  // create response for error control, such as timeout or others
  createEmptyResponse(transId); 

  for (size_t idx = 0; idx < transNum; idx++)
  {
    trans[idx]->setTransId(transId);
    mTransactionPool.push_back(trans[idx]);   
  }

  return true;
}

bool TransactionExecutor::executeTransSync(
    std::vector<BasicRequestPtr>& trans,
    int64_t msTimeout)
{
  size_t transNum = trans.size();
  int ret = isPoolOverload(transNum);
  if (ret >= 0)
  {
    // not overload
  }
  else if (ret == -1)
  {
    // current pool size more than 3/4 of the capacity, pay attention to it
    log_error("request too fast, trans pool will be full soon, pay attention!");
  }
  else
  {
    // pool is full, discard this trans
    log_error("trans pool is full, need to limit the inputs, discard this request!");
    return false;
  }

  uint64_t transId = createTransId(transNum);
  if (transId == 0)
  {
    log_error("get trans id faield, size:%zu, currTransId:%" PRIu64, transNum, mBaseTransacId);
    return false;
  }

  // create response for error control
  createEmptyResponse(transId); 

  SemPtr sem = new Sem();
  for (size_t idx = 0; idx < transNum; idx++)
  {
    trans[idx]->setSem(sem);
    trans[idx]->setTimeout(msTimeout);
    trans[idx]->setTransId(transId);
    mTransactionPool.push_back(trans[idx]);
  }

  // wait for the trans been executed
  for (size_t idx = 0; idx < transNum; idx++)
  {
    ret = trans[idx]->timeWait(msTimeout);
    if (ret < 0)
    {
      log_error("proc request failed. errno:%d", errno);
      return false;
    }
  }

  return true;
}

BasicRequestPtr
TransactionExecutor::getTask()
{
  BasicRequestPtr task;
  bool rslt = mTransactionPool.pop_front(task, -1);
  if (!rslt)
  {
    // when stop the thread, thread shell will wake up form the queue with false
    // log_error("get task from queue failed.");
    return NULL;
  }

  return task;
}

void
TransactionExecutor::recvResponse(
    uint64_t transId,
    const BasicResponsePtr response)
{
  Lock<ThreadMutex> lock(mRespLock);
  // if the trans not in the map, means it has beed remove for procing failed 
  if (mResponsePool.find(transId) == mResponsePool.end())
  {
    log_error("request must be failed yet! transId:%" PRIu64, transId);
    return;
  }
  mResponsePool[transId].push_back(response);

  return;
}

void
TransactionExecutor::removeResponse(uint64_t transId)
{
  Lock<ThreadMutex> lock(mRespLock);
  mResponsePool.erase(transId);
  log_info("remove trans pool, transId:%" PRIu64, transId);

  return;
}

bool
TransactionExecutor::transFinished(
    uint64_t transId,
    const BasicResponsePtr response,
    std::deque<BasicResponsePtr>& resps)
{
  bool rslt;
  Lock<ThreadMutex> lock(mRespLock);

  if (mResponsePool.find(transId) == mResponsePool.end())
  {
    // maybe occurs
    log_error("request must be failed yet! transId:%" PRIu64, transId);
    return false;
  }
  
  // recv response first
  mResponsePool[transId].push_back(response);

  size_t respSize = mResponsePool[transId].size();
  rslt = (respSize == TASK_NUM(transId));
  if (rslt)
  {
    resps.swap(mResponsePool[transId]);
    mResponsePool.erase(transId);
    log_info("proc trans finished. transId:%" PRIu64, transId);
  }

  return rslt;
}

void
TransactionExecutor::createEmptyResponse(uint64_t transId)
{
  Lock<ThreadMutex> lock(mRespLock);
  mResponsePool[transId] = std::deque<BasicResponsePtr>();

  return;
}

void TransactionExecutor::terminatePoolThread()
{
  // stop thread
  for (size_t idx = 0; idx < mThreadPool.size(); idx++)
  {
    mThreadPool[idx]->terminate();
  }
  
  // trigger those thread sleep on the queue
  mTransactionPool.notifyT();

  return;
}

int TransactionExecutor::isPoolOverload(size_t currTransNum)
{
  size_t currPoolSize = mTransactionPool.size();
  size_t totalSize = currPoolSize + currTransNum;
  if (totalSize <= (mTransPoolMaxCapacity * 3 / 4))
  {
    return 0;
  }
  else if (totalSize > mTransPoolMaxCapacity)
  {
    return -2;
  }
  
  // need to limit the request speed
  return -1;
}

/////////////////////////////////////////////////////////////
//
//      ThreadShell
// 
////////////////////////////////////////////////////////////
void
TransactionExecutor::ThreadShell::run()
{
  while (mThreadState != Thread::eExited)
  {
    // if no task in the queue, get function will sink into block
    BasicRequestPtr oneTask = mOwner->getTask();
    if (!oneTask)
    {
      // log_info("internal error, get empty task from queue! threadState:%d", mThreadState);
      continue;
    }

    uint64_t currTransId = oneTask->getTransId();
    log_info("proc transaction, transId:%" PRIu64, currTransId);
    // check timeout
    bool rslt = oneTask->isTimeout();
    if (rslt)
    {
      mOwner->removeResponse(currTransId);
      oneTask->wakeUp();
      log_error("proc task timeout! transId:%" PRIu64, currTransId);
      continue;
    }

    // proc task
    BasicResponsePtr response;
    int ret = oneTask->procRequest(response);
    if (ret < 0)
    {
      mOwner->removeResponse(currTransId);
      oneTask->wakeUp();
      log_error("proc request failed, transId:%" PRIu64, currTransId);
      continue;
    }

    // proc response
    // mOwner->recvResponse(currTransId, response);

    std::deque<BasicResponsePtr> responses;
    rslt = mOwner->transFinished(currTransId, response, responses);
    if (!rslt)
    {
      // wake up the caller
      oneTask->wakeUp();
      continue;
    }

    // double check timeout
    rslt = oneTask->isTimeout();
    if (rslt)
    {
      // remove remaining response
      mOwner->removeResponse(currTransId);
      oneTask->wakeUp();
      log_error("proc task timeout! transId:%" PRIu64, currTransId);
      continue;
    }

    // trans has finished
    response->procResponse(responses);
    oneTask->wakeUp();
  }
  
  return;
}
