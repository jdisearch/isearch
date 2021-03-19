
/*
 * =====================================================================================
 *
 *       Filename: rocksdb_direct_listener.h 
 *
 *    Description:  accept connection from search engine 
 *
 *        Version:  1.0
 *        Created:  Aug 17, 2020
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhuyao, zhuyao28@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __ROCKSDB_DIRECT_LISTENER_H__
#define __ROCKSDB_DIRECT_LISTENER_H__

#include "common/net_addr.h"
#include "common/poller.h"

#include <string>

class HelperProcessBase;
class PollThread;

class RocksdbDirectListener : public PollerObject
{
private:
  std::string mDomainSocketPath;
  HelperProcessBase *mRocksdbProcess;
  PollThread *mPollerThread;

public:
  RocksdbDirectListener(
      const std::string &name,
      HelperProcessBase *processor,
      PollThread *poll);

  virtual ~RocksdbDirectListener();

  int Bind();
  int attach_thread();
  virtual void input_notify(void);

private:
  // void init();
};

#endif // __ROCKSDB_DIRECT_LISTENER_H__
