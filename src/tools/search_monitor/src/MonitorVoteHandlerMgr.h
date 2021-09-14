//////////////////////////////////////////////////
//
// epoll driver that handle client vote request event
//   created by qiuyu on Nov 27,2018
//
//////////////////////////////////////////////////

#ifndef __MONITOR_VOTE_HANDLER_MGR_H__ 
#define __MONITOR_VOTE_HANDLER_MGR_H__ 

#include "singleton.h"

#include <vector>

class CPollThread;
class CMutex;
class MonitorVoteHandler;

class MonitorVoteHandlerMgr 
{
private:
  CPollThread* mVoteEventPoll;
  CMutex*      mMutexLock;
  std::vector<MonitorVoteHandler*> mVoteHandlers;

public:
  MonitorVoteHandlerMgr();
  ~MonitorVoteHandlerMgr();

  static MonitorVoteHandlerMgr* getInstance()
  {
    return CSingleton<MonitorVoteHandlerMgr>::Instance();
  }
 
  bool startVoteHandlerMgr();

  CPollThread* getThreadPoll() { return mVoteEventPoll; }
  void addHandler(MonitorVoteHandler* handler);
  void removeHandler(MonitorVoteHandler* handler);

private:
};

#endif // __MONITOR_VOTE_HANDLER_MGR_H__ 
