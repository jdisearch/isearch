////////////////////////////////////////////////////////////
//
// invoke client for invoking the peer monitor node
//      created by qiuyu on Nov 27, 2018
//
///////////////////////////////////////////////////////////

#include "InvokeHandler.h"
#include "log.h"
#include "DetectUtil.h"
#include "MonitorVoteHandler.h"
#include "InvokeMgr.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

InvokeHandler::InvokeHandler(
    CPollThread* poll,
    const std::string& ip,
    const int port)
:
CPollerObject(poll, 0),
mIp(ip),
mPort(port)
{
}

InvokeHandler::~InvokeHandler()
{
}

bool InvokeHandler::initHandler(const bool isInit)
{
  connectToServer();
  if (isInit && netfd <= 0) return true;
  if (!isInit && netfd <= 0) return false;
  

  return attachThread();
}

bool InvokeHandler::connectToServer()
{
  bool rslt = DetectUtil::connectServer(netfd, mIp, mPort);
  if (!rslt)
  {
    // return false;
    // if connect failed. may be the server not startup, reconnect it when
    // the next time send vote
    netfd = -1;
    monitor_log_error("connect to the server failed.");
  }
  else
  {
    monitor_log_error("connect to server successful, ip:%s, port:%d, netfd:%d", mIp.c_str(), mPort, netfd);
  }

  return true;
}

bool InvokeHandler::attachThread()
{
  if (netfd <= 0)
  {
    monitor_log_error("invalid socket fd.");
    return false;
  }

  EnableInput();
  int ret = CPollerObject::AttachPoller();
  if (ret < 0)
  {
    monitor_log_error("add event to poll failed.");
    return false;
  }
  monitor_log_error("add invoke handler event to the poll successful. netfd:%d", netfd);
  
  return true;
}

/* Notice:
* 1.in current now, not dealing with the sticky package!!!!!!!!
* 2.timeout need to be return with the bigger one between timeout and peer timeout
*/   
bool InvokeHandler::invokeVote(
    const DetectHandlerBase::DetectType& type,
    const std::string& detectedAddr,
    const uint64_t sequenceId,
    const std::string& data,
    int& timeout)
{
  if (data.length() <= 0)
  {
    monitor_log_error("invoke failed. %s:%d, sequenceId:%" PRIu64, mIp.c_str(), mPort, sequenceId);
    return false; 
  }

  monitor_log_error("invoke to %s:%d, timeout:%d, sequenceId:%" PRIu64, mIp.c_str(), mPort, timeout, sequenceId);

  if (netfd <= 0)
  {
    // reconnect the socket
    bool rslt = initHandler(false);
    if (!rslt)
    {
      monitor_log_error("invoke vote failed. ip:%s, port:%d", mIp.c_str(), mPort);
      return false;
    }
    // bool rslt = DetectUtil::connectServer(netfd, mIp, mPort);
    // if (!rslt || netfd <= 0)
    // {
    //   monitor_log_error("invoke vote failed. netfd:%d", netfd);
    //   return false;
    // }
  }
  
  // should set socket to block, function call maybe has some issue
  char* request = (char*)malloc(sizeof(MonitorVoteHandler::VoteRequest_t) + data.length());
  MonitorVoteHandler::VoteRequest_t* pData = (MonitorVoteHandler::VoteRequest_t*)request;
  pData->sMagicNum = htons(MonitorVoteHandler::sgMagicNum);
  pData->sSequenceId = sequenceId;
  DetectUtil::translateByteOrder(pData->sSequenceId);

  // calculate the peer timeout value and pick the bigger one
  static std::string peerAddr = mIp + DetectHandlerBase::toString<long long int>(mPort); // only c++11 support 
  int peerTimeout = DetectHandlerBase::getInstanceTimeout(type, peerAddr, detectedAddr);
  timeout = peerTimeout > timeout ? peerTimeout : timeout;
  pData->sTimeout = htonl(timeout);

  pData->sDetectType = (DetectHandlerBase::DetectType)htonl((uint32_t)type);
  pData->sDataLen = htons(data.length());
  memcpy(pData->sDataBuff, data.c_str(), data.length());
  
  int dataLen = sizeof(MonitorVoteHandler::VoteRequest_t) + data.length();
  int ret = DetectUtil::sendMessage(netfd, request, dataLen);
  delete request;
  if (ret != dataLen)
  {
    monitor_log_error("invoke vote failed. netfd:%d", netfd);
    
    CPollerObject::DetachPoller();
    close(netfd);
    netfd = -1;

    return false;
  }

  monitor_log_error("send data successful, fd:%d, sequenceId:%" PRIu64, netfd, sequenceId);

  return true;
}

// handle voted response from peer
void InvokeHandler::InputNotify()
{
#if 0
  uint16_t magic;
  int len = DetectUtil::recieveMessage(netfd, (char*)&magic, sizeof(uint16_t));
  if (0 == len)
  {
    monitor_log_error("client close the fd:%d.", netfd);
    CPollerObject::DetachPoller();
    // MonitorVoteHandlerMgr::getInstance()->removeHandler(this);
    netfd = -1;
    callBack(0, false);
    return;
  }
   
  if (magic != MonitorVoteHandler::sgMagicNum)
  {
    monitor_log_error("receive message failed.");
    callBack(0, false);
    return;
  }
  
  uint64_t sequenceId;
  len = DetectUtil::recieveMessage(netfd, (char*)&sequenceId, sizeof(uint64_t));
  if (len != sizeof(uint64_t))
  {
    monitor_log_error("revieve message failed, fieldLen:%d", len);
    callBack(0, false);
    return;
  }
  
  // result
  bool isVote = false;
  len = DetectUtil::recieveMessage(netfd, (char*)&isVote, sizeof(bool));
  if (len != sizeof(bool))
  {
    monitor_log_info("recieve message failed. sequenceId:%" PRIu64 , sequenceId);
    callBack(0, false);
    return;
  }

  monitor_log_info("call back to caller. sequenceId:%" PRIu64 , sequenceId);
  callBack(sequenceId, isVote);
#else
  MonitorVoteHandler::VoteResponse_t response;
  int len = DetectUtil::recieveMessage(netfd, (char*)&response, sizeof(response));
  if (len != sizeof(response))
  {
    monitor_log_error("client close or read socket failed, fd:%d, len:%d.", netfd, len);
    
    CPollerObject::DetachPoller();
    close(netfd);
    netfd = -1;
    
    // callBack(0, false);
    return;
  }
  
  // check magic num
  if (ntohs(response.sMagicNum) != MonitorVoteHandler::sgMagicNum)
  {
    monitor_log_error("receive message failed.");
    // callBack(0, false);
    return;
  }
  
  DetectUtil::translateByteOrder(response.sSequenceId);
  monitor_log_error("call back to caller. sequenceId:%" PRIu64 , response.sSequenceId);
  callBack(response.sSequenceId, response.sIsVote);
#endif
  return;
}

void InvokeHandler::callBack(
    const uint64_t sequenceId,
    const bool isVote)
{
  InvokeMgr::getInstance()->callBack(sequenceId, isVote);
  return;
}
