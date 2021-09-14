////////////////////////////////////////////////////////
//
// connections between cluster using to send vote request
// and response
//    create by qiuyu on Nov 26, 2018
//
///////////////////////////////////////////////////////

#ifndef __MONITOR_VOTE_HANDLER_H__
#define __MONITOR_VOTE_HANDLER_H__

#include "DetectHandlerBase.h"

class MonitorVoteHandler : public CPollerObject
{
public:
  static const uint16_t sgMagicNum = 12345; // global magic number

// private:
  // CPollThread* mThreadPoll,
#pragma pack(push, 1)
  typedef struct VoteRequestTrans
  {
    uint16_t sMagicNum;
    uint64_t sSequenceId; // for sync call
    int      sTimeout;
    DetectHandlerBase::DetectType sDetectType;
    uint16_t sDataLen;
    char sDataBuff[];
  }VoteRequest_t;
  
  typedef struct VoteResponseTrans
  {
    uint16_t sMagicNum;
    uint64_t sSequenceId; // for sync call
    bool sIsVote;
  }VoteResponse_t;
#pragma pack(pop)

public:
  MonitorVoteHandler(
      CPollThread* poll,
      const int fd);

  virtual ~MonitorVoteHandler();

  int addVoteEventToPoll();
  int removeFromEventPoll();
  virtual void InputNotify(void);

private:
  bool procVoteRequest(
    const DetectHandlerBase::DetectType requestType,
    const char* dataBuff,
    const int timeout,
    bool& isAlive);
  
  void sendResponse(
      const uint64_t sequenceId,
      const bool isVote);
};

#endif // __MONITOR_VOTE_HANDLER_H__
