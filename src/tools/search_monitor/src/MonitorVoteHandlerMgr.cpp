//////////////////////////////////////////////////
//
// epoll driver that handle client vote request event,
//   created by qiuyu on Nov 27,2018
//
//////////////////////////////////////////////////

#include "MonitorVoteHandlerMgr.h"
#include "MonitorVoteHandler.h"
#include "poll_thread.h"
#include "lock.h"

MonitorVoteHandlerMgr::MonitorVoteHandlerMgr()
:
mVoteEventPoll(new CPollThread("handleVoteThreadPoll")),
mMutexLock(new CMutex())
{
}

MonitorVoteHandlerMgr::~MonitorVoteHandlerMgr()
{
  if (mVoteEventPoll) delete mVoteEventPoll;
  if (mMutexLock) delete mMutexLock;

  for (size_t idx = 0; idx < mVoteHandlers.size(); idx++)
  {
    if (mVoteHandlers[idx]) delete mVoteHandlers[idx];
  }
}

bool MonitorVoteHandlerMgr::startVoteHandlerMgr()
{
  int ret = mVoteEventPoll->InitializeThread();
  if (ret < 0)
  {
    monitor_log_error("initialize thread poll failed.");
    return false;
  }
  
  mVoteEventPoll->RunningThread();
  monitor_log_info("start handleVoteThreadPoll successful.");
  return true;
}

// all vote handler must be added into epoll through this function for
// ensure the poll thread safety
void MonitorVoteHandlerMgr::addHandler(MonitorVoteHandler* handler)
{
  mMutexLock->lock();
  // poll should be protected by lock 
  handler->addVoteEventToPoll(); 
  mVoteHandlers.push_back(handler);
  mMutexLock->unlock();

  return;
}

// because the number of handlers should be only a little, 
// so directly to remove from the vector will not to be a big problem
void MonitorVoteHandlerMgr::removeHandler(MonitorVoteHandler* handler)
{
  mMutexLock->lock();
  for (size_t idx = 0; idx < mVoteHandlers.size(); idx++)
  {
    if (mVoteHandlers[idx] == handler)
    {
      // need to death from the poll
      handler->removeFromEventPoll();

      delete mVoteHandlers[idx];
      mVoteHandlers.erase(mVoteHandlers.begin() + idx);
      mMutexLock->unlock();
      return;
    }
  }
  mMutexLock->unlock();

  monitor_log_error("handler must be in the container, serious issue.");
  return;
}
