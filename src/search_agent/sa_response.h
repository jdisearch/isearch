#ifndef __SA_RESPONSE_H__
#define __SA_RESPONSE_H__

#include "sa_msg.h"
#include "sa_conn.h"

class CSearchAgentResponse : public CSearchAgentMessage
{
public:
    CSearchAgentResponse(uint64_t iMessageId, ConnectionInterface *pConnection);
    virtual int ParseMessage(ConnectionInterface *pConnection,
                             CSearchAgentCore *pCoreCtx);

    virtual ~CSearchAgentResponse();

private:
    void ParseResponse();
};

#endif
