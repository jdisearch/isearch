////////////////////////////////////////////////////////
//
// Handle detector cluster vote request
//    create by qiuyu on Nov 26, 2018
//
///////////////////////////////////////////////////////

#include "MonitorVoteHandler.h"
#include "DetectUtil.h"
#include "MonitorVoteHandlerMgr.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

MonitorVoteHandler::MonitorVoteHandler(
    CPollThread* poll,
    const int fd)
:
CPollerObject(poll, fd)
{
}

MonitorVoteHandler::~MonitorVoteHandler()
{
}

int MonitorVoteHandler::addVoteEventToPoll()
{
  EnableInput();
  int ret = CPollerObject::AttachPoller();
  if (ret < 0)
  {
    monitor_log_error("add event to poll failed.");
    return -1;
  }
  
  monitor_log_error("add handler to event poll successful, fd:%d", netfd);

  return 0;
}

int MonitorVoteHandler::removeFromEventPoll()
{
  CPollerObject::DetachPoller();
  
  monitor_log_error("delete vote handler event from poll successful, fd:%d", netfd);
  // close will be done on super class's destructor
  // close(netfd);
  // netfd = -1;

  return 0;
}

// handle vote request and send response
// because not thinking about the split or delay of data package, so if
// recieving unexpected message, just close the socket. the client will
// reconnect during the next invoking
void MonitorVoteHandler::InputNotify()
{
  uint16_t magic = 0;
  int len = DetectUtil::recieveMessage(netfd, (char*)&magic, sizeof(uint16_t));
  magic = ntohs(magic);
  if (len != sizeof(uint16_t) || magic != sgMagicNum)
  {
    monitor_log_error("recieve message failed. fd:%d, len:%d, magic:%d", netfd, len, magic);
    MonitorVoteHandlerMgr::getInstance()->removeHandler(this);
    return;
  }
   
  // sequeceId
  uint64_t sequenceId;
  len = DetectUtil::recieveMessage(netfd, (char*)&sequenceId, sizeof(uint64_t));
  if (len != sizeof(uint64_t))
  {
    monitor_log_error("recieve message failed. fd:%d, len:%d, sequenceId:%" PRIu64, netfd, len, sequenceId);
    MonitorVoteHandlerMgr::getInstance()->removeHandler(this);
    return;
  }
  DetectUtil::translateByteOrder(sequenceId);
  
  int timeout = 0;
  len = DetectUtil::recieveMessage(netfd, (char*)&timeout, sizeof(int));
  if (len != sizeof(int))
  {
    monitor_log_error("recieve message failed. fd:%d, len:%d, sequenceId:%" PRIu64, netfd, len, sequenceId);
    MonitorVoteHandlerMgr::getInstance()->removeHandler(this);
    return;
  }
  timeout = ntohl(timeout);

  DetectHandlerBase::DetectType detectType;
  len = DetectUtil::recieveMessage(netfd, (char*)&detectType, sizeof(detectType));
  if (len != sizeof(detectType))
  {
    monitor_log_error("recieve message failed. fd:%d, len:%d, sequenceId:%" PRIu64, netfd, len, sequenceId);
    MonitorVoteHandlerMgr::getInstance()->removeHandler(this);
    return;
  }
  detectType = (DetectHandlerBase::DetectType)(ntohl(detectType));
  
  uint16_t dataLen;
  len = DetectUtil::recieveMessage(netfd, (char*)&dataLen, sizeof(uint16_t));
  if (len != sizeof(uint16_t))
  {
    monitor_log_error("recieve message failed. fd:%d, len:%d, sequenceId:%" PRIu64, netfd, len, sequenceId);
    MonitorVoteHandlerMgr::getInstance()->removeHandler(this);
    return;
  }
  dataLen = ntohs(dataLen);

  char dataBuff[dataLen + 1];
  len = DetectUtil::recieveMessage(netfd, (char*)&dataBuff, dataLen);
  if (len != dataLen)
  {
    monitor_log_error("recieve message failed. fd:%d, len:%d, sequenceId:%" PRIu64, netfd, len, sequenceId);
    MonitorVoteHandlerMgr::getInstance()->removeHandler(this);
    return;
  }
  dataBuff[dataLen] = 0;

  monitor_log_error("recieve vote request, timeout:%d, sequenceId:%" PRIu64, timeout, sequenceId);

  bool isAlive = true;
  procVoteRequest(detectType, dataBuff, timeout, isAlive); 
  
  if (isAlive)
  {
    monitor_log_error("deny the request, sequenceId:%" PRIu64, sequenceId);
  }
  else
  {
    monitor_log_error("vote the request, sequenceId:%" PRIu64, sequenceId);
  }
  sendResponse(sequenceId, !isAlive);

  return;
}

bool MonitorVoteHandler::procVoteRequest(
    const DetectHandlerBase::DetectType requestType,
    const char* dataBuff,
    const int timeout,
    bool& isAlive)
{
  int errCode;

  switch (requestType)
  {
  case DetectHandlerBase::eAgentDetect:
  {
    std::string accessKey, ipWithPort;
    int accessKeyLen = *(uint8_t*)dataBuff;
    accessKey.assign(dataBuff + sizeof(uint8_t), accessKeyLen);
    int ipWithPortLen = *(uint8_t*)(dataBuff + sizeof(uint8_t) + accessKeyLen);
    ipWithPort.assign(dataBuff + sizeof(uint8_t)*2 + accessKeyLen, ipWithPortLen);
    int tout = timeout > 0 ? timeout : DtcMonitorConfigMgr::eAgentDefaultTimeout;

    monitor_log_error("start to detect agent. accessKey:%s, ipWithPort:%s", accessKey.c_str(), ipWithPort.c_str());

    return DetectUtil::detectAgentInstance(accessKey, ipWithPort, tout, isAlive, errCode);
  }
  case DetectHandlerBase::eDtcDetect:
  {
    std::string ipWithPort(dataBuff);
    int tout = timeout > 0 ? timeout : DtcMonitorConfigMgr::eDtcDefaultTimeout;
    monitor_log_error("start to detect dtc. ipWithPort:%s", ipWithPort.c_str());
    
    return DetectUtil::detectDtcInstance(ipWithPort, tout, isAlive, errCode);
  }
  default:
    // if proc failed, not vote
    isAlive = true;
    monitor_log_error("Error: serious internal error.......");
  }
  
  return true;
}

void MonitorVoteHandler::sendResponse(
    const uint64_t sequenceId,
    const bool isVote)
{
  VoteResponse_t response;
  response.sMagicNum = htons(sgMagicNum);
  response.sSequenceId = sequenceId;
  DetectUtil::translateByteOrder(response.sSequenceId);
  response.sIsVote = isVote;
  int ret = DetectUtil::sendMessage(netfd, (char*)&response, sizeof(VoteResponse_t));
  if (ret != sizeof(VoteResponse_t))
  {
    monitor_log_error("send response failed. netfd:%d, sequenceId:%" PRIu64, netfd, sequenceId);
  }
  
  monitor_log_error("send response successful. netfd:%d, sequenceId:%" PRIu64, netfd, sequenceId);

  if (ret < 0)
  {
    // close the connection
    monitor_log_error("close the connection, netfd:%d", netfd);
    MonitorVoteHandlerMgr::getInstance()->removeHandler(this);
  }

  return;
}
