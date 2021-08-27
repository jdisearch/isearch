#ifndef __SA_SERVER_CONN_H__
#define __SA_SERVER_CONN_H__
#include "sa_conn.h"
#include "sa_server.h"
#include <map>
#include <list>

class CInstanceConn : public ConnectionInterface
{
public:
     CInstanceConn(uint32_t iConnectionId):ConnectionInterface(iConnectionId){};
    virtual void Reference(void *owner);
    virtual void EnMessageInQueue(CSearchAgentMessage *pMessage,
                                  CSearchAgentCore *pCoreCtx);
    virtual int Send(CSearchAgentCore *pCoreCtx);
    virtual CSearchAgentMessage *SendNext(CSearchAgentCore *pCoreCtx);
    virtual int Recieve(CSearchAgentCore *pCoreCtx);
    virtual CSearchAgentMessage *RecieveNext(CSearchAgentCore *pCoreCtx, bool isAlloc);
    virtual void RecvDone(CSearchAgentCore *pCoreCtx,
                          CSearchAgentMessage *pCurrentMessage,
                          CSearchAgentMessage *pNextMessage);
    int InstanceConnect();
    virtual void SendDone(CSearchAgentCore *pCoreCtx,
                          CSearchAgentMessage *pMessage);
    virtual void DeMessageTree(CSearchAgentMessage *pMessage,
                               CSearchAgentCore *pCoreCtx);
    virtual void Close(CSearchAgentCore *pCoreCtx);
private:
    std::queue<CSearchAgentMessage *> m_oInMessageQueue;
    std::map<uint32_t, CSearchAgentMessage *> m_oConnectionMessages;
    std::map<uint32_t, CSearchAgentMessage *> m_oRecvMessages;
    CSearchServerInstance *m_pOwnerServer;

private:
    void ResponseForward(CSearchAgentCore *pCoreContxt,
                         CSearchAgentMessage *pResponseMessage,
                         ConnectionInterface *pClientConnection,
                         CSearchAgentMessage *pRequestMessage);
};

#endif
