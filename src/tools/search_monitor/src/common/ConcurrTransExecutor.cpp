/////////////////////////////////////////////////////////////
//
// Task Pool, thread unsafe.
// Author:qiuyu
// Date:Jul 12th,2019
//
////////////////////////////////////////////////////////////

#include "ConcurrTransExecutor.h"

#define MAX_POOL_CAPACITY 1000000

/////////////////////////////////////////////////////////////
//
//              BasicRequest relevant
//
/////////////////////////////////////////////////////////////
BasicRequest::BasicRequest()
:
mTransactionId(0),
mExpiredWhen(-1), // never timeout
mSem(NULL),
mHeadRequest(NULL),
mNextRequest(NULL)
{
}

BasicRequest::~BasicRequest()
{
  mSem = NULL;
  mHeadRequest = NULL;
  mNextRequest =NULL;
}

void BasicRequest::setErrorCode(BasicRequest::RequestError errCode)
{
  if (!mHeadRequest)
  {
    log_error("head pointer can not be empty!");
    return;
  }

  int errValue = 0;
  switch (errCode)
  {
    default:
    case eProcFailed:
      errValue = ((errValue + eProcFailed) << 16);
      break;
    case eTimeout:
      errValue = ((errValue + eTimeout) << 16);
      break;
  }
  log_info("set err code:%d", errValue);
  mHeadRequest->mFinishedReqNum.add(errValue);
  mHeadRequest = NULL;

  return;
}

int BasicRequest::procRequestPrev()
{
  // call the real implementation function
  int ret = procRequest();
  if (ret < 0)
  {
    mHeadRequest = NULL;
    return ret;
  }

  if (!mHeadRequest)
  {
    log_error("Head request can not be Null!");
    return -1;
  }

  // if the request is sync, just return and the main thread will do the procResponse
  if (mSem)
  {
    log_info("sync trans, transId:%" PRIu64, mTransactionId);
    mHeadRequest = NULL;
    return 0;
  }

  // async trans, need to collect the response by ourselves
  Atomic rValue = mHeadRequest->mFinishedReqNum.inc(); 
  if (((int)rValue >> 16) != 0)
  {
    log_error("Trans has failed, transId:%" PRIu64, mTransactionId);
    mHeadRequest = NULL;
    return -1;
  }

  if ((int)rValue != (int)TASK_NUM(mTransactionId))
  {
    mHeadRequest = NULL;
    return 1;
  }

  // trans finished, collect result
  std::vector<BasicRequestPtr> results;
  BasicRequestPtr curr = mHeadRequest;
  // log_error("request ref:%d", curr.mRawPointer->getRef());
  while (curr)
  {
    results.push_back(curr);
    // log_error("request ref:%d", curr.mRawPointer->getRef());
    curr = curr->mNextRequest;
  }
  mHeadRequest = NULL;

  return procResponsePrev(results);
}

int BasicRequest::procResponsePrev(std::vector<BasicRequestPtr> &results)
{
  // check all request state first
  if (results.size() <= 0)
  {
    log_error("invalid response value!");
    return -1;
  }
  
  // header request must be the first one
  if (((int)results[0]->mFinishedReqNum >> 16) != 0)
  {
    log_error("Trans has failed, transId:%" PRIu64, mTransactionId);
    return -1;
  }
  
  return procResponse(results);
}

/////////////////////////////////////////////////////////////
//
//              ConcurrTransExecutor relevant
//
/////////////////////////////////////////////////////////////
ConcurrTransExecutor::ConcurrTransExecutor()
:
mBaseTransacId(1)
{
  log_info("construct ConcurrTransExecutor!");
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

ConcurrTransExecutor::~ConcurrTransExecutor()
{
  terminatePoolThread();
  // wait for all thread finished(the normal way is to call join)
  sleep(3); 
  // log_info("deconstruct ConcurrTransExecutor!");
}

bool
ConcurrTransExecutor::executeTransAsync(std::vector<BasicRequestPtr> trans)
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
  
  for (size_t idx = 0; idx < transNum; idx++)
  {
    trans[idx]->setSem(NULL);
    trans[idx]->setTransId(transId);
    trans[idx]->bindLinkList(trans[0], (idx + 1) == transNum ? (BasicRequest*)NULL : trans[idx + 1]);
    mTransactionPool.push_back(trans[idx]);   
  }

  return true;
}

bool ConcurrTransExecutor::executeTransSync(
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

  SemPtr sem = new Sem();
  for (size_t idx = 0; idx < transNum; idx++)
  {
    trans[idx]->setSem(sem);
    trans[idx]->setTimeout(msTimeout);
    trans[idx]->setTransId(transId);
    trans[idx]->bindLinkList(trans[0], (idx + 1) == transNum ? (BasicRequest*)NULL : trans[idx + 1]);
    // log_error("request ref:%d", trans[0].mRawPointer->getRef());
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

  // proc response here, use header request to collect the result
  if (!trans[0])
  {
    log_error("internal error! header request can not be NULL.");
    return false;
  }
  ret = trans[0]->procResponsePrev(trans);

  return ret < 0 ? false : true;
}

BasicRequestPtr
ConcurrTransExecutor::getTask()
{
  BasicRequestPtr task;
  bool rslt = mTransactionPool.pop_front(task, -1);
  if (!rslt)
  {
    // when stop the thread, thread shell will wake up form the queue with false
    // log_error("get task from queue failed.");
    return NULL;
  }
//  log_error("request ref:%d", task.mRawPointer->getRef());

  return task;
}

void ConcurrTransExecutor::terminatePoolThread()
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

int ConcurrTransExecutor::isPoolOverload(size_t currTransNum)
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
//      ThreadShell relevant
// 
////////////////////////////////////////////////////////////
void
ConcurrTransExecutor::ThreadShell::run()
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
//    log_error("request ref:%d", oneTask.mRawPointer->getRef());
    uint64_t currTransId = oneTask->getTransId();
    log_info("proc transaction, transId:%" PRIu64, currTransId);
    // check timeout
    bool rslt = oneTask->isTimeout();
    if (rslt)
    {
      // mark this transaction as timeout
      oneTask->setErrorCode(BasicRequest::eTimeout);
      oneTask->wakeUp();
      log_error("proc task timeout! transId:%" PRIu64, currTransId);
      continue;
    }

    // proc task
    int ret = oneTask->procRequestPrev();
    if (ret < 0)
    {
      oneTask->setErrorCode(BasicRequest::eProcFailed);
      oneTask->wakeUp();
      log_error("proc request failed, transId:%" PRIu64, currTransId);
      continue;
    }
    
    oneTask->wakeUp();
  }
  
  return;
}
