////////////////////////////////////////////////////////
//
// Handle detector cluster vote request
//    create by qiuyu on Nov 26, 2018
//
///////////////////////////////////////////////////////

#include "MonitorVoteListener.h"
#include "MonitorVoteHandlerMgr.h"
#include "MonitorVoteHandler.h"
#include "DtcMonitorConfigMgr.h"

#include <errno.h>
#include <sstream>

MonitorVoteListener::MonitorVoteListener(CPollThread* poll)
:
CPollerObject(poll, 0)
{
  init(); 
}

MonitorVoteListener::~MonitorVoteListener()
{
}

int MonitorVoteListener::Bind(int blog)
{
  if ((netfd = SockBind(&mListenAddr, blog, 0, 0, 1/*reuse*/, 1/*nodelay*/, 1/*defer_accept*/)) == -1)
  {
    monitor_log_error("bind address failed.");
    return -1;
  }

  return 0;
}

int MonitorVoteListener::attachThread()
{
  EnableInput();
  int ret = CPollerObject::AttachPoller();
  if (ret < 0)
  {
    monitor_log_error("add event to poll failed.");
    return -1;
  }
  monitor_log_info("add listen event to poll successful, fd:%d", netfd);

  return 0;
}

// handle client connection
void MonitorVoteListener::InputNotify()
{
  int newFd;
  struct sockaddr peer;
  socklen_t peerSize = sizeof(peer);
  
  // extracts all the connected connections in the pending queue until return
  // EAGAIN 
  while (true)
  {
    newFd = accept(netfd, (struct sockaddr *)&peer, &peerSize);
    if (-1 == newFd)
    {
      if (errno == EINTR)
      {
        // system call "accept" was interrupted by signal before a valid connection 
        // arrived, go on accept
        continue;
      }

      if(errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // no remaining connection on the pending queue, break out
        // log_notice("accept new client error: %m, %d", errno);
        return;
      }

      // accept error
      monitor_log_error("accept new client failed, netfd:%d, errno:%d", netfd, errno);
      return;
    }

    monitor_log_error("accept new client, newFd:%d", newFd);

    // add the handler vote event to another poll driver
    CPollThread* poll = MonitorVoteHandlerMgr::getInstance()->getThreadPoll();
    MonitorVoteHandler * handler = new MonitorVoteHandler(poll, newFd);
    MonitorVoteHandlerMgr::getInstance()->addHandler(handler);
    monitor_log_info("create new vote handler successful, fd:%d", newFd);
  }

  return;
}

void MonitorVoteListener::init()
{
  std::stringstream bindAddr;
  const std::pair<std::string, int>& addr = DtcMonitorConfigMgr::getInstance()->getListenAddr();
  bindAddr << addr.first << ":" << addr.second << "/tcp";
  mListenAddr.SetAddress(bindAddr.str().c_str(), (const char*)NULL);

  return;
}
