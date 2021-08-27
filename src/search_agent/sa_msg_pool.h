#ifndef __SA_MESSAGE_POOL_H_
#define __SA_MESSAGE_POOL_H_
#include "sa_msg.h"


class CSearchAgentMessagePool
{
public:
    void InitMessage();
    CSearchAgentMessage *GetMessage(ConnectionInterface *pConnection,
                                    CSearchAgentCore *pCore,
                                    bool isRequest);
    void EnMessageTreeSet(const MsgNode &msgNode);
    void DeMessageTreeSet(uint32_t msgId);
    void ReclaimTimeoutMessage(uint64_t &iNewTimeout,
                               CSearchAgentCore *pCore);

private:
    uint32_t m_iMessageId;
    uint64_t m_iFragId;
    set<MsgNode> m_oMessageSet;
    map<uint32_t,  MsgNode> m_mapMessageIdMapper;

private:
    void ReclaimMessage(CSearchAgentMessage *pMessage,
                        CSearchAgentCore *pCore);
    void ReclaimSendingMessage(CSearchAgentMessage *pMessage,
                               CSearchAgentCore *pCore);
};

#endif