
/*
 * =====================================================================================
 *
 *       Filename:  rocksdb_direct_process.h 
 *
 *    Description:  
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
#ifndef __ROCKSDB_DIRECT_PROCESS_H__
#define __ROCKSDB_DIRECT_PROCESS_H__

#include <string>

class HelperProcessBase;
class RocksdbDirectListener;
class PollThread;

class RocksdbDirectProcess
{
private:
  std::string mDomainSocketPath;
  HelperProcessBase *mRocksdbProcess;
  PollThread *mRocksDirectPoll;
  RocksdbDirectListener *mListener;
  // RocksdbDirectWorker* mDirectWorker;

public:
  RocksdbDirectProcess(
      const std::string &name,
      HelperProcessBase *processor);

  RocksdbDirectProcess(const RocksdbDirectProcess &) = delete;
  void operator=(const RocksdbDirectProcess &) = delete;

  int init();
  int run_process();

private:
  int add_listener_to_poll();
  // int addRocksdbWorkToPoll();
};

#endif // __ROCKSDB_DIRECT_PROCESS_H__
