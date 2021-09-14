////////////////////////////////////////////////////////
//
// Handle detector cluster vote request
//    create by qiuyu on Nov 26, 2018
//
///////////////////////////////////////////////////////

#ifndef __MONITOR_VOTE_LISTENER_H__
#define __MONITOR_VOTE_LISTENER_H__

#include "sockaddr.h"
#include "poller.h"

class CPollThread;

class MonitorVoteListener : public CPollerObject
{
private:
  CSocketAddress mListenAddr;

public:
  MonitorVoteListener(CPollThread* poll);

  virtual ~MonitorVoteListener();

  int Bind(const int blog = 256);
  int attachThread();
  virtual void InputNotify (void);

private:
  void init();
};

#endif // __MONITOR_VOTE_LISTENER_H__
