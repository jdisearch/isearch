
/*
 * =====================================================================================
 *
 *       Filename: rocksdb_direct_process.cc 
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
#include "rocksdb_direct_process.h"
#include "rocksdb_direct_listener.h"
#include "common/poll_thread.h"
#include "log.h"

#include <assert.h>

RocksdbDirectProcess::RocksdbDirectProcess(
    const std::string &name,
    HelperProcessBase *processor)
    : mDomainSocketPath(name),
      mRocksdbProcess(processor),
      mRocksDirectPoll(new PollThread("RocksdbDirectAccessPoll")),
      mListener(NULL)
{
}

int RocksdbDirectProcess::init()
{
  assert(mRocksDirectPoll);

  int ret = mRocksDirectPoll->initialize_thread();
  if (ret < 0)
  {
    log_error("initialize thread poll failed.");
    return -1;
  }

  // add listener to poll
  ret = add_listener_to_poll();
  if (ret != 0)
    return -1;

  // add worker to poll
  // ret = addRocksdbWorkToPoll();
  // if ( ret != 0 ) return -1;

  return 0;
}

int RocksdbDirectProcess::add_listener_to_poll()
{
  mListener = new RocksdbDirectListener(mDomainSocketPath, mRocksdbProcess, mRocksDirectPoll);
  if (!mListener)
  {
    log_error("create listener instance failed");
    return -1;
  }

  int ret = mListener->Bind();
  if (ret < 0)
  {
    log_error("bind address failed.");
    return -1;
  }

  ret = mListener->attach_thread();
  if (ret < 0)
  {
    log_error("add listener to poll failed.");
    return -1;
  }

  return 0;
}

/*
int RocksdbDirectProcess::addRocksdbWorkToPoll()
{
  mDirectWorker = new RocksdbDirectWorker(mRocksDirectPoll);
  if ( !mDirectWorker )
  {
    log_error("create rocksdb direct worker failed.");
    return -1;
  }
  
  int ret = mDirectWorker->attach_thread();
  if ( ret < 0 )
  {
    log_error("add rocksdb direct worker to poll failed.");
    return -1;
  }
  
  return true;
}*/

int RocksdbDirectProcess::run_process()
{
  mRocksDirectPoll->running_thread();
  log_error("start rocksdb direct process!");
  return 0;
}
