////////////////////////////////////////////////////////////
//
// invoke client for invoking the peer monitor node
//      created by qiuyu on Nov 27, 2018
//
///////////////////////////////////////////////////////////

#ifndef __INVOKE_HANDLER_H__
#define __INVOKE_HANDLER_H__

#include "poller.h"
#include "DetectHandlerBase.h"

#include <string>

class CPollThread;

class InvokeHandler : public CPollerObject
{
private:
  std::string mIp;
  int mPort;

public:
  InvokeHandler(
      CPollThread* poll,
      const std::string& ip,
      const int port);

  virtual ~InvokeHandler();

  virtual void InputNotify(void);
  
  bool initHandler(const bool isInit);

  bool invokeVote(
      const DetectHandlerBase::DetectType& type,
      const std::string& detectedAddr,
      const uint64_t sequenceId,
      const std::string& data,
      int& timeout);

private:
  bool attachThread();
  bool connectToServer();

  void callBack(
      const uint64_t sequenceId,
      const bool isVote);
};

#endif // __INVOKE_HANDLER_H__
