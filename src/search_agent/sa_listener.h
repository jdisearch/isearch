#ifndef __SA_LISTENER_H__
#define __SA_LISTENER_H__
#include "sa_conn.h"


class CSearchListener
{
public:
    int ListenerInit();
};

class CSearchListenerConnection : public ConnectionInterface
{
public:
    CSearchListenerConnection(uint32_t iConnectionId);
    virtual int Recieve(CSearchAgentCore *pCoreCtx);
    virtual void Reference(void *owner);
    int Initialize();
private:
    int ListenerRecieve(CSearchAgentCore *pCoreCtx);
    int ListenerAccept(CSearchAgentCore *pCoreCtx);

};
#endif
