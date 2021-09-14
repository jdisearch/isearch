/////////////////////////////////////////////////////////
//
// This class detect the DTC instance for a internal time
//    created by qiuyu on Nov 26, 2018
////////////////////////////////////////////////////////

#include "DtcMonitor.h"
#include "poll_thread.h"
#include "MonitorVoteListener.h"
#include "DtcDetectHandler.h"

// DtcMonitor* DtcMonitor::mSelf = NULL;
// CMutex* DtcMonitor::mMutexLock = new CMutex();

DtcMonitor::DtcMonitor()
:
mDetectPoll(new CPollThread("DtcDetectPoll")),
mListener(NULL),
mDtcDetector(NULL)
{
}

DtcMonitor::~DtcMonitor()
{
  // if (mMutexLock)
  //   delete mMutexLock;
  if (mDetectPoll)
    delete mDetectPoll;
  if (mListener)
    delete mListener;
  if (mDtcDetector)
    delete mDtcDetector;
}

bool DtcMonitor::initMonitor()
{
  // add poller to poller thread
  if (!mDetectPoll)
  {
    monitor_log_error("create thread poll failed.");
    return false;
  }

  int ret = mDetectPoll->InitializeThread();
  if (ret < 0)
  {
    monitor_log_error("initialize thread poll failed.");
    return false;
  }

  // add listener to poll
  bool rslt = addListenerToPoll();
  if (!rslt) return false;
  
  rslt = addTimerEventToPoll();
  if (!rslt) return false;
  
  return true;
}

bool DtcMonitor::startMonitor()
{
  bool rslt = initMonitor();
  if (!rslt) return false;

  mDetectPoll->RunningThread();
  monitor_log_info("start DtcMonitor successful.");
  return true;
}

bool DtcMonitor::addListenerToPoll()
{
  mListener = new MonitorVoteListener(mDetectPoll);
  if (!mListener)
  {
    monitor_log_error("create listener instance failed");
    return false;
  }
  
  int ret = mListener->Bind();
  if (ret < 0)
  {
    monitor_log_error("bind address failed.");
    return false;
  }
  
  ret = mListener->attachThread();
  if (ret < 0)
  {
    monitor_log_error("add listener to poll failed.");
    return false;
  }
  
  return true;
}

// add detect agent, detect dtc event to the poll
bool DtcMonitor::addTimerEventToPoll()
{ 
  mDtcDetector = new DtcDetectHandler(mDetectPoll);
  if (!mDtcDetector)
  {
    monitor_log_error("create dtc detector failed.");
    return false;
  }
  mDtcDetector->addTimerEvent();

  return true;
}
