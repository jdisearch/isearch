
/*
 * =====================================================================================
 *
 *       Filename: rocksdb_direct_worker.h 
 *
 *    Description:  access rocksdb directly
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

#ifndef __ROCKSDB_DIRECT_WORKER_H__
#define __ROCKSDB_DIRECT_WORKER_H__

#include "rocksdb_direct_context.h"
#include "common/poller.h"

class HelperProcessBase;
class PollThread;

class RocksdbDirectWorker : public PollerObject
{
private:
  HelperProcessBase *mDBProcessRocks;
  DirectRequestContext mRequestContext;
  RangeQueryRows_t range_query_row_;
  DirectResponseContext mResponseContext;

public:
  RocksdbDirectWorker(
      HelperProcessBase *processor,
      PollThread *poll,
      int fd);

  virtual ~RocksdbDirectWorker();

  int add_event_to_poll();
  virtual void input_notify(void);

private:
  int recieve_request();
  void proc_direct_request();
  void send_response();
  int remove_from_event_poll();
  void procDirectRangeQuery();
  void procReplicationRegistry();
  void procReplicationStateQuery();

  int recieve_message(
      char *data,
      int dataLen);

  int send_message(
      const char *data,
      int dataLen);

  bool condtion_priority(
      const CondOpr lc,
      const CondOpr rc);
};

#endif // __ROCKSDB_DIRECT_WORKER_H__
