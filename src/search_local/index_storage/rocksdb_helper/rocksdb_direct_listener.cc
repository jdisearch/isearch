/*
 * =====================================================================================
 *
 *       Filename: rocksdb_direct_listener.cc 
 *
 *    Description:  Handle detector cluster vote request
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhuyao, zhuyao28@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "rocksdb_direct_listener.h"
#include "rocksdb_direct_worker.h"
#include "poll_thread.h"
#include "log.h"

#include <errno.h>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

RocksdbDirectListener::RocksdbDirectListener(
    const std::string &name,
    HelperProcessBase *processor,
    PollThread *poll)
    : PollerObject(poll, 0),
      mDomainSocketPath(name),
      mRocksdbProcess(processor),
      mPollerThread(poll)
{
  // init();
}

RocksdbDirectListener::~RocksdbDirectListener()
{
  unlink(mDomainSocketPath.c_str());
}

int RocksdbDirectListener::Bind()
{
  struct sockaddr_un localAddr;
  if (mDomainSocketPath.length() >= (int)sizeof(localAddr.sun_path))
  {
    log_error("unix socket path is too long! path:%s", mDomainSocketPath.c_str());
    return -1;
  }

  memset((void *)&localAddr, 0, sizeof(localAddr));
  localAddr.sun_family = AF_UNIX;
  strncpy(localAddr.sun_path, mDomainSocketPath.c_str(), mDomainSocketPath.length());

  netfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (netfd <= 0)
  {
    log_error("create unix sockst failed.");
    return -1;
  }

  int optval = 1;
  setsockopt(netfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  setsockopt(netfd, SOL_TCP, TCP_NODELAY, &optval, sizeof(optval));

  optval = 60;
  setsockopt(netfd, SOL_TCP, TCP_DEFER_ACCEPT, &optval, sizeof(optval));

  // bind addr, remove the maybe existen unique addr first
  unlink(mDomainSocketPath.c_str());
  int ret = bind(netfd, (sockaddr *)&localAddr, sizeof(localAddr));
  if (ret < 0)
  {
    log_error("bind addr failed!, addr:%s, errno:%d", mDomainSocketPath.c_str(), errno);
    close(netfd);
    return -1;
  }

  // listen for the connection
  ret = listen(netfd, 256);
  if (ret < 0)
  {
    log_error("listen to the socket failed!, addr:%s, errno:%d", mDomainSocketPath.c_str(), errno);
    close(netfd);
    return -1;
  }

  return 0;
}

int RocksdbDirectListener::attach_thread()
{
  enable_input();
  int ret = PollerObject::attach_poller();
  if (ret < 0)
  {
    log_error("add rocksdb direct listener to poll failed.");
    return -1;
  }

  log_info("add rocksdb direct listener to poll successful, fd:%d", netfd);

  return 0;
}

// handle client connection
void RocksdbDirectListener::input_notify()
{
  int newFd, ret;
  struct sockaddr_un peer;
  socklen_t peerSize = sizeof(peer);

  // extracts all the connected connections in the pending queue until return EAGAIN
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

      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // no remaining connection on the pending queue, break out
        // log_notice("accept new client error: %m, %d", errno);
        return;
      }

      // accept error
      log_error("accept new client failed, netfd:%d, errno:%d", netfd, errno);
      return;
    }

    log_error("accept new client to rocksdb direct process, newFd:%d", newFd);

    // add the handler vote event to another poll driver
    RocksdbDirectWorker *worker = new RocksdbDirectWorker(mRocksdbProcess, mPollerThread, newFd);
    if (!worker)
    {
      log_error("create rocsdb direct workder failed!");
      continue;
    }

    worker->add_event_to_poll();
  }

  return;
}

/*
void RocksdbDirectListener::init()
{
  std::stringstream bindAddr;
  const std::pair<std::string, int>& addr = DtcMonitorConfigMgr::getInstance()->getListenAddr();
  bindAddr << addr.first << ":" << addr.second << "/tcp";
  
  SocketAddress mListenAddr;
  mListenAddr.set_address(bindAddr.str().c_str(), (const char*)NULL);

  return;
}*/
