//////////////////////////////////////////////////
//
// invoke API for cluster
//   created by qiuyu on Nov 27,2018
//
//////////////////////////////////////////////////

#include "InvokeMgr.h"
#include "InvokeHandler.h"

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

int InvokeMgr::smClusterNodeSize = -1;
// global sequence id for one process, if it's cluster global will be more better
uint64_t InvokeMgr::sgSequenceId = 0;

InvokeMgr::InvokeMgr()
:
mInvokePoll(new CPollThread("dtcInvokePoll")),
mLock(new CMutex())
{
  // unit must be ms
  mInvokeTimeout = DtcMonitorConfigMgr::getInstance()->getInvokeTimeout();
  monitor_log_error("invokeTimeout:%d", mInvokeTimeout);
  
  mNeedStop = false;
  mHasInit = false;
}

InvokeMgr::~InvokeMgr()
{
  if (mInvokePoll) delete mInvokePoll;
  if (mLock) delete mLock;

  for (size_t idx = 0; idx < mInvokeHandlers.size(); idx++)
  {
    if (mInvokeHandlers[idx]) delete mInvokeHandlers[idx];
  }
  
  CallBackDataItr_t itr = mRemaingRequests.begin();
  while (itr != mRemaingRequests.end())
  {
    if (itr->second) delete itr->second;
    itr++;
  }
  mRemaingRequests.clear();
}

bool InvokeMgr::startInvokeMgr()
{
  if (mHasInit) return true;

  // the highest one byte for physical id
  int physicalId = DtcMonitorConfigMgr::getInstance()->getPhysicalId();
  sgSequenceId = ((sgSequenceId + physicalId) << GLOBAL_PHYSICAL_ID_SHIFT) + 1;
  monitor_log_info("start invokeMgr....");
  
  // start epoll thread
  int ret = mInvokePoll->InitializeThread();
  if (ret < 0)
  {
    monitor_log_error("initialize thread poll failed.");
    return false;
  }
  
  bool rslt = initInvokeHandlers();
  if (!rslt) return false; 
  mInvokePoll->RunningThread();
  
  monitor_log_info("start invokeMgr successful.... begin sequenceId:%" PRIu64, sgSequenceId);
  mHasInit = true;
  return true;
}

void InvokeMgr::stopInvokeMgr()
{
  mNeedStop = true;
}

// in current, this function call can not support reentry
bool InvokeMgr::invokeVoteSync(
      const DetectHandlerBase::DetectType type,
      const std::string& detectedAddr,
      const std::string& invokeData,
      const int timeout,
      bool& needSwitch)
{
// return true;
  bool rslt;
  SempData_t* sema = new SempData_t();
  uint64_t sequenceId;
  for (size_t idx = 0; idx < mInvokeHandlers.size(); idx++)
  {
    sequenceId = getSequenceId();

    // use semaphore to keep synchronize for a temp, in the future
    // should change it with event_fd
    int waitTime = timeout;
    mLock->lock();
    rslt = mInvokeHandlers[idx]->invokeVote(type, detectedAddr, sequenceId, invokeData, waitTime);
    if (!rslt) 
    {
      mLock->unlock();
      continue;
    }
    
    // vote for timeout with sempahore, delete it in the callback if needed
    sema->sTotalVotes++; 
    mRemaingRequests[sequenceId] = sema; 
    mLock->unlock();
    
    monitor_log_info("wait for response. sequenceId:%" PRIu64, sequenceId);
    // invoke timeout must be greater than detect timeout in order to wait the objective
    // response from peer instead of the subjective timeout, its more correctly
    waitTime = (mInvokeTimeout > waitTime + 10000/*offset*/) ? mInvokeTimeout : (waitTime + 10000);
    sema->semTimeWait(waitTime); 
    
    mLock->lock();
    CallBackDataItr_t itr = mRemaingRequests.find(sequenceId);
    if (itr != mRemaingRequests.end())
    {
      if (itr->second->sTotalVotes > ((smClusterNodeSize + 1) >> 1))
      {
        // gather majority of peer votes
        needSwitch = true;
        monitor_log_info("erase request, sequenceId:%" PRIu64, sequenceId);
        mRemaingRequests.erase(sequenceId);
        assert(mRemaingRequests.find(sequenceId) == mRemaingRequests.end());
        mLock->unlock();
        
        delete sema;
        return true;
      }

      // remove this invoke request from the remaining map
      monitor_log_info("erase request, sequenceId:%" PRIu64, sequenceId);
      mRemaingRequests.erase(sequenceId);
      assert(mRemaingRequests.find(sequenceId) == mRemaingRequests.end());
    }
    else
    {
      monitor_log_error("should never come here, sequenceId:%" PRIu64, sequenceId);
    }

    mLock->unlock();
  }
  
  needSwitch = false;
  delete sema;

  return true;
}

bool InvokeMgr::callBack(
    const uint64_t sequenceId,
    const bool isVote)
{
  SempData_t* sem = NULL;
  
  mLock->lock();
  
  // no need to deal with the sequenceId 0 because that means network issue and lock
  // the meaningless thing will cost extra time

  // if (0 == sequenceId)
  // {
  //   monitor_log_error("has network issue, notice!!!!!!");
  //   mLock->unlock();

  //   return true;
  // }

  CallBackDataItr_t itr = mRemaingRequests.find(sequenceId);
  if (itr != mRemaingRequests.end())
  {
    monitor_log_error("async call call back, isVote:%d, sequenceId:%" PRIu64, isVote, sequenceId);
    
    sem = itr->second;
    if (!isVote)
    {
      // remove the vote which was set in invoke in order to handle request timeout
      sem->sTotalVotes--;
    }
  }
  else
  {
    mLock->unlock();
    monitor_log_error("this request must be timeout, vote it.sequenceId:%" PRIu64, sequenceId);

    return true;
  }
  
  if (sem) sem->wakeUp();
  
  mLock->unlock();

  return true;
}

uint64_t InvokeMgr::getSequenceId()
{
  // the last bit in sequence id indicate the vote result, 0 means refuse
  // int temp = sgSequenceId;
  return __sync_fetch_and_add(&sgSequenceId, 1);
}

// create poll event and add it to the poll
bool InvokeMgr::initInvokeHandlers()
{
  if (mInvokeHandlers.size() != 0)
  {
    // this means user create this object more than one time,
    // this can not be permitted
    monitor_log_error("can not create singleton class for more than one time.");
    return false;
  }
  monitor_log_info("begin to init invoke handlers.");

  // get cluster info
  const DtcMonitorConfigMgr::PairVector_t& clusterInfo = DtcMonitorConfigMgr::getInstance()->getClusterInfo();
  if (clusterInfo.size() < 2 || clusterInfo.size() > 0x7FFFFFFF) // the other node is self
  {
    monitor_log_error("cluster node can not less than 3 or larger than '0x7FFFFFFF'. size:%u", (int)clusterInfo.size());
    return false;
  }
  smClusterNodeSize = clusterInfo.size();

  bool rslt;
  InvokeHandler* handler;
  for (size_t idx = 0; idx < clusterInfo.size(); idx++)
  {
    handler = NULL;
    handler = new InvokeHandler(mInvokePoll, clusterInfo[idx].first, clusterInfo[idx].second);
    if (!handler)
    {
      monitor_log_error("create invokeHandler failed.");
      return false;
    }
    
    rslt = handler->initHandler(true);
    if (!rslt)
    {
      monitor_log_error("create invoke handler failed.");
      return false;
    }
    mInvokeHandlers.push_back(handler);
  }
  monitor_log_info("create invoke handler successful.");

  return true;
}
