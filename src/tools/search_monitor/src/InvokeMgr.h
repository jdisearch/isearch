//////////////////////////////////////////////////
//
// invoke API for cluster
//   created by qiuyu on Nov 27,2018
//
//////////////////////////////////////////////////

#ifndef __INVOKE_MGR_H__
#define __INVOKE_MGR_H__

#include "singleton.h"
#include "DetectHandlerBase.h"
#include "Sem.h"

#define GLOBAL_PHYSICAL_ID_SHIFT (7 * 8)

class InvokeHandler;
class CPollThread;
class CMutex;

class InvokeMgr
{
  typedef struct SemaphoreData
  {
    Sem* sSem;
    int sTotalVotes;
    
    SemaphoreData()
    :
    sSem(new Sem),
    sTotalVotes(1) // vote for itself
    {
    }

    ~SemaphoreData()
    {
      if (sSem) delete sSem;
    }

    void semWait()
    {
      sSem->semWait();
    }

    void semTimeWait(const int miSecExpriedTime)
    {
      sSem->semTimeWait(miSecExpriedTime);
    }

    void wakeUp()
    {
      sSem->semPost();
    }
  }SempData_t;

  friend struct SemaphoreData;

private:
  static int smClusterNodeSize; // numbers of cluster peer node
  static uint64_t sgSequenceId;
  
  typedef std::map<uint64_t, SempData_t*> CallBackData_t;
  typedef std::map<uint64_t, SempData_t*>::iterator CallBackDataItr_t;

private:
  CPollThread* mInvokePoll;
  CMutex*      mLock;
  std::vector<InvokeHandler*> mInvokeHandlers;
  // std::map<uint64_t, SempData_t*>  mRemaingRequests;
  CallBackData_t  mRemaingRequests;
  int mInvokeTimeout;
  bool mNeedStop;
  bool mHasInit;

public:
  InvokeMgr();
  ~InvokeMgr();

public:
  static InvokeMgr* getInstance()
  {
    return CSingleton<InvokeMgr>::Instance();
  }

  bool startInvokeMgr();
  void stopInvokeMgr();

  bool invokeVoteSync(
      const DetectHandlerBase::DetectType type,
      const std::string& detectedAddr,
      const std::string& invokeData,
      const int timeout,
      bool& needSwitch);

  bool callBack(
      const uint64_t sequenceId,
      const bool isVote);

private:
  uint64_t getSequenceId();
  bool initInvokeHandlers();
};

#endif // __INVOKE_MGR_H__
