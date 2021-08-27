#ifndef __SA_CLIENT_CONN_H__
#define __SA_CLIENT_CONN_H__

#include "sa_conn.h"
#include "sa_error.h"
#include <queue>
#include <map>

class CClientConn : public ConnectionInterface
{
public:
    CClientConn(uint32_t iConnectionId):ConnectionInterface(iConnectionId){};
    int Recieve(CSearchAgentCore *pCoreCtx);
    virtual CSearchAgentMessage *RecieveNext(CSearchAgentCore *pCoreCtx, bool isAlloc);
    void RecvDone(CSearchAgentCore *pCoreCtx,
                  CSearchAgentMessage *pCurrentMessage,
                  CSearchAgentMessage *pNextMessage);
   
    void EnMessageInQueue(CSearchAgentMessage *pMessage,
                          CSearchAgentCore *pCoreCtx);
    void DeMessageInQueue(CSearchAgentMessage *pMessage,
                          CSearchAgentCore *pCoreCtx);
    void EnMessageOutQueue(CSearchAgentMessage *pMessage,
                          CSearchAgentCore *pCoreCtx);
    void DeMessageOutQueue(CSearchAgentMessage *pMessage,
                            CSearchAgentCore *pCoreCtx);

    virtual int Send(CSearchAgentCore *pCoreCtx);
    virtual void Reference(void *owner);
    virtual CSearchAgentMessage *SendNext(CSearchAgentCore *pCoreCtx);
    virtual void Close(CSearchAgentCore *pCoreCtx);
    virtual void SendDone(CSearchAgentCore *pCoreCtx, CSearchAgentMessage *pMessage);
    virtual ~CClientConn();
private:
    std::map<uint32_t, CSearchAgentMessage *> m_oInMessageQueue;
    std::queue<uint32_t> m_oOutMessageQueue;
    std::map<uint32_t, CSearchAgentMessage *> m_mapOutMessageMapper;
private:
       void RequestForward(CSearchAgentCore *pCoreContxt,
                           CSearchAgentMessage *pMessage,
                           ConnectionInterface *pServerConnection);
       void MakeError(CSearchAgentMessage *pMessage,
                      SEARCH_AGENT_ERR_CODE iErrorCode);
};

#endif
