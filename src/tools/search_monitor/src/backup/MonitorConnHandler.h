////////////////////////////////////////////////////////
//
// connections between cluster using to send vote request
// and response
//    create by qiuyu on Nov 26, 2018
//
///////////////////////////////////////////////////////

#ifndef __MONITOR_VOTE_HANDLER_H__
#define __MONITOR_VOTE_HANDLER_H__

class MonitorConnHandler : public CPollerObject
{
private:
  CPollThread* mThreadPoll,

public:
  MonitorConnHandler(CPollThread* poll);

  virtual ~MonitorConnHandler();

  int AttachThread();
  virtual void InputNotify (void);
}

#endif // __MONITOR_VOTE_HANDLER_H__
