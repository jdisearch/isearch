#ifndef __SA_REQUEST_H__
#define __SA_REQUEST_H__

#include "sa_msg.h"
#include "sa_conn.h"
#include "http_parser.h"
#include "json/json.h"
#include <list>
#include <set>


class CSearchAgentRequest : public CSearchAgentMessage
{

public:
    CSearchAgentRequest(uint64_t iMessageId, ConnectionInterface *pConnection);

    virtual int ParseMessage(ConnectionInterface *pConnection,
                             CSearchAgentCore *pCoreCtx);

    virtual ~CSearchAgentRequest();
    bool IsRequestDone();
    void SetRequestDone(bool isDone);
    const std::string &GetDocumentId();
    virtual int Coalesce();

private:
    CHttpPacketMessage *m_pHttpMessage;
    bool m_isRequestDone;
    uint32_t m_iDocumentCount;
    vector<Json::Value> m_arrResultDocs;
    set<string> m_setDocIds;
private:
    void ParseRequest();
    void CoalesceWriteResponse();
    void CoalesceReadResponse();
    void ParseOneResponse(CSearchAgentMessage *pOnceResponse);
};

#endif
