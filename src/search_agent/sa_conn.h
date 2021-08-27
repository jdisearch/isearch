#ifndef __SA_CONN_H__
#define __SA_CONN_H__

#include <netinet/in.h>
#include <queue>
#include "sa_util.h"

typedef enum
{
    FRONTWORK,
    LISTENER,
    BACKWORK
} ConnectionType;

typedef enum
{
    RECV_READY = 1,
    SEND_READY = 2,
} ConnectionStatus;

class CSearchAgentCore;
class CSearchAgentMessage;


class ConnectionInterface
{
public:
    ConnectionInterface(uint32_t iConnectionId)
    {
       m_iConnId = iConnectionId;
    }
    virtual int Recieve(CSearchAgentCore *pCoreCtx) = 0;
    virtual int Send(CSearchAgentCore *pCoreCtx) { return 0; };
    virtual void Reference(void *owner){};
    virtual CSearchAgentMessage *RecieveNext(CSearchAgentCore *pCoreCtx, bool isAlloc) { return NULL; };
    virtual CSearchAgentMessage *SendNext(CSearchAgentCore *pCoreCtx) { return NULL; };
    virtual void SendDone(CSearchAgentCore *pCoreCtx, CSearchAgentMessage *pMessage);
    void SetFd(int fd);
    int GetFd();
    bool IsConnected();
    virtual ~ConnectionInterface();
    virtual void RecvDone(CSearchAgentCore *pCoreCtx,
                          CSearchAgentMessage *pCurrentMessage,
                          CSearchAgentMessage *pNextMessage){};
    virtual void EnMessageInQueue(CSearchAgentMessage *pMessage,
                                  CSearchAgentCore *pCoreCtx){};

    virtual void DeMessageInQueue(CSearchAgentMessage *pMessage,
                                  CSearchAgentCore *pCoreCtx) {};

    virtual void EnMessageOutQueue(CSearchAgentMessage *pMessage,
                                   CSearchAgentCore *pCoreCtx) {};

    virtual void DeMessageOutQueue(CSearchAgentMessage *pMessage,
                                  CSearchAgentCore *pCoreCtx) {};

    virtual void DeMessageTree(CSearchAgentMessage *pMessage,
                               CSearchAgentCore *pCoreCtx) {};

    virtual void Close(CSearchAgentCore *pCoreCtx){};

    struct sockaddr *GetSockAddr();
    void SetIsConnecting();
    bool IsCached() { return m_isCached; };
    void SetCached(bool isCached) { m_isCached = isCached; };
    bool IsEof();
    uint32_t GetConnectionId() {return m_iConnId;}
protected:
    ConnectionType m_eConnectionType;
    uint32_t m_eConnectionStatus;
    uint32_t m_iConnId;
    int m_iFd;
    bool m_isError;
    bool m_isEof;
    bool m_isDone;
    struct sockaddr m_oSocketAddress;
    bool m_isConnecting;
    bool m_isCached;
    CSearchAgentMessage *m_pRecievingMessage;
    CSearchAgentMessage *m_pSendMessage;

protected:
    int MessageRecieve(CSearchAgentCore *pCore);
    int MessageRecvChain(CSearchAgentCore *pCoreCtx, CSearchAgentMessage *pMessage);
    int ConnRecieve(void *pBuffer, size_t iSize);
    int MessageSend(CSearchAgentCore *pCoreCtx);
    int MessageSendChain(CSearchAgentCore *pCoreCtx, CSearchAgentMessage *pMessage);
    int ConnectionSend(struct iovec *iovec_array, int iElements, size_t iSend);
};



#endif
