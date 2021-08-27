#include "sa_msg_pool.h"
#include "sa_conn_pool.h"
#include "log.h"
#include "sa_request.h"
#include "sa_response.h"
#include <stdint.h>

void CSearchAgentMessagePool::InitMessage()
{
    log_debug("CSearchAgentMessage::%s", __FUNCTION__);
    m_iMessageId = 0;
    m_iFragId = 0;
    m_oMessageSet.clear();
    m_mapMessageIdMapper.clear();
}

CSearchAgentMessage *CSearchAgentMessagePool::GetMessage(ConnectionInterface *pConnection, CSearchAgentCore *pCore, bool isRequest)
{
    CSearchAgentMessage *pMessage = NULL;
    if (isRequest)
    {
        pMessage = new CSearchAgentRequest(m_iMessageId++, pConnection);
    }
    else
    {
        pMessage = new CSearchAgentResponse(m_iMessageId++, pConnection);
    }

    if (pMessage == NULL)
    {
        log_error("lack of memory for search AgentMessage");
    }
    if (m_iMessageId >= UINT32_MAX)
    {
        m_iMessageId = 0;
    }
    return pMessage;
}

void CSearchAgentMessagePool::EnMessageTreeSet(const MsgNode &msgNode)
{
    if (m_mapMessageIdMapper.find(msgNode.msg_id) != m_mapMessageIdMapper.end())
    {
        return;
    }
    m_mapMessageIdMapper.insert(make_pair(msgNode.msg_id, msgNode));
    m_oMessageSet.insert(msgNode);
}

void CSearchAgentMessagePool::ReclaimTimeoutMessage(uint64_t &iNewTimeout,
                                                    CSearchAgentCore *pCore)
{
    log_debug("CSearchAgentMessagePool::%s", __FUNCTION__);
    uint64_t iNowTimeStamp = get_now_us();
    set<MsgNode>::iterator iterMessageNode = m_oMessageSet.begin();
    while (iterMessageNode != m_oMessageSet.end())
    {
        log_debug("CSearchAgentMessagePool::%s, m_oMessageSet size %zu", __FUNCTION__, m_oMessageSet.size());

        CSearchAgentRequest *pTempMessage = static_cast<CSearchAgentRequest *>(iterMessageNode->data);
        MsgNode tempMsgNode = *iterMessageNode;
        iterMessageNode++;
        if (pTempMessage == NULL)
        {
            m_mapMessageIdMapper.erase(tempMsgNode.msg_id);
            m_oMessageSet.erase(tempMsgNode);
            continue;
        }
        log_debug("CSearchAgentMessagePool::%s,reclaim message id %d ", __FUNCTION__, tempMsgNode.msg_id);

        if (m_mapMessageIdMapper.find(tempMsgNode.msg_id) == m_mapMessageIdMapper.end() || pTempMessage->IsRequestDone())
        {
            m_mapMessageIdMapper.erase(pTempMessage->GetMessageId());
            m_oMessageSet.erase(tempMsgNode);
            ReclaimMessage(pTempMessage, pCore);
            if (pTempMessage != NULL)
            {
                delete pTempMessage;
            }
            if (m_oMessageSet.size() == 0)
            {
                return;
            }
            continue;
        }
        if (iNowTimeStamp < tempMsgNode.key)
        {
            uint64_t iDelta = (iNowTimeStamp - iterMessageNode->key) / 1000;
            if (iDelta < iNewTimeout)
            {
                iNewTimeout = iDelta;
            }
            return;
        }
        log_error("req timeout iNowTimeStamp %llu, exceed time %llu , msg_id %llu", (long long unsigned int)iNowTimeStamp, 
        (long long unsigned int)tempMsgNode.key, (long long unsigned int)pTempMessage->GetMessageId());

        ReclaimMessage(pTempMessage, pCore);
        m_mapMessageIdMapper.erase(pTempMessage->GetMessageId());
        m_oMessageSet.erase(tempMsgNode);
        delete pTempMessage;
        if (m_oMessageSet.size() == 0)
        {
            return;
        }
    }
}


void CSearchAgentMessagePool::ReclaimMessage(CSearchAgentMessage *pMessage,
                                             CSearchAgentCore *pCore)
{
    log_debug("CSearchAgentMessagePool::%s", __FUNCTION__);
    ConnectionInterface *pClientConnection = pMessage->GetOwnerConnection();
    const vector<uint32_t> &arrServerConnectionIds = pMessage->GetPeerConnection();
    if (NULL != pClientConnection)
    {
        pClientConnection->DeMessageInQueue(pMessage, pCore);
        pClientConnection->DeMessageOutQueue(pMessage, pCore);
    }
    for (size_t i = 0; i < arrServerConnectionIds.size(); i++)
    {
        ConnectionInterface* pServerConnection = CSearchAgentConnectionPool::Instance()->GetConnectionById(arrServerConnectionIds[i]);     
        if (pServerConnection != NULL)
        {
            pServerConnection->DeMessageTree(pMessage, pCore);
        }
    }
}

void CSearchAgentMessagePool::DeMessageTreeSet(uint32_t msgId)
{
    m_mapMessageIdMapper.erase(msgId);
}
