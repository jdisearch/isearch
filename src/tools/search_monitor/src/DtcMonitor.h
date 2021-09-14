/////////////////////////////////////////////////////////
//
// This class detect the DTC instance for a internal time
//    created by qiuyu on Nov 26, 2018
////////////////////////////////////////////////////////

#ifndef __DTC_MONITOR_H__
#define __DTC_MONITOR_H__

#include "singleton.h"

class DetectHandlerBase;
class CPollThread;
class MonitorVoteListener;

class DtcMonitor
{
private:
  // static DtcMonitor*    mSelf;
  // static CMutex*        mMutexLock;
  CPollThread*          mDetectPoll;
  MonitorVoteListener*  mListener;
  DetectHandlerBase*    mDtcDetector;

public:
  DtcMonitor();
  virtual ~DtcMonitor();

  static DtcMonitor* getInstance()
  {
    return CSingleton<DtcMonitor>::Instance();
  }
// private:
  // DtcMonitor();
  // DtcMonitor(const DtcMonitor&);
  // DtcMonitor& operator=(const DtcMonitor&);

public:
  bool startMonitor();
  void stopMonitor();

private:
  bool initMonitor();
  bool addListenerToPoll();
  bool addTimerEventToPoll();
};

#endif // __DTC_MONITOR_H__
