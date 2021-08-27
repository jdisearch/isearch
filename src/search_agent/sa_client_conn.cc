#include "sa_client_conn.h"
#include "sa_msg.h"
#include "sa_server.h"
#include "sa_core.h"
#include "sa_error.h"
#include "log.h"
#include "http_parser.h"
#include "sa_request.h"
#include "sa_epoll.h"

int CClientConn::Recieve(CSearchAgentCore *pCoreCtx)
{
    return MessageRecieve(pCoreCtx);
}

CSearchAgentMessage *CClientConn::RecieveNext(CSearchAgentCore *pCoreCtx, bool isAlloc)
{
    CSearchAgentMessage *pMessage;
    if (m_isEof)
    {
        pMessage = m_pRecievingMessage;
        if (pMessage != NULL)
        {
            m_pRecievingMessage = NULL;
            delete pMessage;
        }
        return NULL;
    }
    pMessage = m_pRecievingMessage;
    if (pMessage != NULL)
    {
        return pMessage;
    }
    if (!isAlloc)
    {
        return NULL;
    }
    CSearchAgentMessagePool *pMessagePool = pCoreCtx->GetMessagePool();
    pMessage = pMessagePool->GetMessage(this, pCoreCtx, true);
    if (pMessage != NULL)
    {
        m_pRecievingMessage = pMessage;
    }
    return pMessage;
}

void CClientConn::RecvDone(CSearchAgentCore *pCoreCtx,
                           CSearchAgentMessage *pCurrentMessage,
                           CSearchAgentMessage *pNextMessage)
{
    log_debug("CClientConn::%s", __FUNCTION__);
    m_pRecievingMessage = pNextMessage;

    EMessageType eType = pCurrentMessage->GetMessageType();
    log_debug("%s, MessageType %d", __FUNCTION__, eType);

    if (pCurrentMessage->GetMessageType() == INDEX_READ_MSG)
    {
        log_debug("search request , forward to all shardings");
        CServerPool *pServerPool = pCoreCtx->GetServerPool();
        vector<ConnectionInterface *> oSearchShardingConnections = pServerPool->GetAllShardingConnections();
        pCurrentMessage->MessageFragment(oSearchShardingConnections.size());
        for (size_t i = 0; i < oSearchShardingConnections.size(); i++)
        {
            pCurrentMessage->SetPeerConnection(oSearchShardingConnections[i]);
            RequestForward(pCoreCtx, pCurrentMessage, oSearchShardingConnections[i]);
        }
    }
    else if (pCurrentMessage->GetMessageType() == INDEX_WRITE_MSG)
    {
        log_debug("search request , forward to  consistent hash specified sharding");
        CServerPool *pServerPool = pCoreCtx->GetServerPool();
        const std::string &strTempDocumentId = (static_cast<CSearchAgentRequest *>(pCurrentMessage))->GetDocumentId();
        ConnectionInterface *pServerConnection = pServerPool->GetWriteConnection(strTempDocumentId);
        if (pServerConnection == NULL)
        {
            MakeError(pCurrentMessage, ERR_NO_VALID_SERVER);
            EnMessageOutQueue(pCurrentMessage, pCoreCtx);
            log_error("no valid server connection, swalow it");
            return;
        }
        pCurrentMessage->SetPeerConnection(pServerConnection);
        pCurrentMessage->MessageFragment(1);
        RequestForward(pCoreCtx, pCurrentMessage, pServerConnection);
    }
    else
    {
        log_error("error message Type %d", eType);
        MakeError(pCurrentMessage, ERR_URI_PATH);
        EnMessageOutQueue(pCurrentMessage, pCoreCtx);
        return;
    }
    EnMessageInQueue(pCurrentMessage, pCoreCtx);
}

void CClientConn::EnMessageInQueue(CSearchAgentMessage *pMessage,
                                   CSearchAgentCore *pCoreCtx)
{
    log_debug("CClientConn::%s", __FUNCTION__);
    m_oInMessageQueue.insert(make_pair(pMessage->GetMessageId(), pMessage));
}

void CClientConn::RequestForward(CSearchAgentCore *pCoreContxt,
                                 CSearchAgentMessage *pMessage,
                                 ConnectionInterface *pServerConnection)
{
    log_debug("CClientConn::%s", __FUNCTION__);
    pCoreContxt->CacheSendEvent(pServerConnection);
    pServerConnection->EnMessageInQueue(pMessage, pCoreContxt);
}

int CClientConn::Send(CSearchAgentCore *pCoreCtx)
{
    log_debug("CClientConn::%s", __FUNCTION__);
    return MessageSend(pCoreCtx);
}

void CClientConn::Reference(void *owner)
{
}

CSearchAgentMessage *CClientConn::SendNext(CSearchAgentCore *pCoreCtx)
{
    log_debug("CClientConn::%s", __FUNCTION__);
    if (m_mapOutMessageMapper.empty())
    {
        int status = CEventBase::Instance()->EventDelOut(this);
        if (status != 0)
        {
            m_isError = true;
            return NULL;
        }
    }

    CSearchAgentMessage *pMessage = m_pSendMessage;

    if (pMessage != NULL)
    {
        //pMessage = m_oOutMessageQueue.front();
        while (!m_oOutMessageQueue.empty())
        {
            m_oOutMessageQueue.pop();
            uint32_t m_pSendMessageId = m_oOutMessageQueue.front();
            if (m_mapOutMessageMapper.find(m_pSendMessageId) != m_mapOutMessageMapper.end())
            {
                break;
            }
        }
    }
    if (m_oOutMessageQueue.empty())
    {
        m_pSendMessage = NULL;
        return NULL;
    }

    uint32_t iSendMessageId = m_oOutMessageQueue.front();
    std::map<uint32_t, CSearchAgentMessage *>::iterator iter = m_mapOutMessageMapper.find(iSendMessageId);
    m_pSendMessage = iter->second;
    m_oOutMessageQueue.pop();
    m_mapOutMessageMapper.erase(iSendMessageId);
    return m_pSendMessage;
}

void CClientConn::DeMessageInQueue(CSearchAgentMessage *pMessage,
                                   CSearchAgentCore *pCoreCtx)
{
    log_debug("CClientConn::%s, msg_id %d", __FUNCTION__, pMessage->GetMessageId());
    m_oInMessageQueue.erase(pMessage->GetMessageId());
}

void CClientConn::EnMessageOutQueue(CSearchAgentMessage *pMessage,
                                    CSearchAgentCore *pCoreCtx)
{
    log_debug("CClientConn::%s, msg_id %d", __FUNCTION__, pMessage->GetMessageId());
    m_oOutMessageQueue.push(pMessage->GetMessageId());
    m_mapOutMessageMapper.insert(make_pair(pMessage->GetMessageId(), pMessage));
    pCoreCtx->CacheSendEvent(this);
}

void CClientConn::DeMessageOutQueue(CSearchAgentMessage *pMessage,
                                    CSearchAgentCore *pCoreCtx)
{
    m_mapOutMessageMapper.erase(pMessage->GetMessageId());
}

void CClientConn::Close(CSearchAgentCore *pCoreCtx)
{
    log_debug("CClientConn::%s, close client connection fd %d", __FUNCTION__, m_iFd);
    if (m_iFd < 0)
    {
        return;
    }
    std::map<uint32_t, CSearchAgentMessage *>::iterator iter = m_oInMessageQueue.begin();
    while (iter != m_oInMessageQueue.end())
    {
        CSearchAgentMessage *pTempMessage = iter->second;
        log_debug("CClientConn::%s,close connection, delete message %u", __FUNCTION__, pTempMessage->GetMessageId());
        iter++;
        pCoreCtx->GetMessagePool()->DeMessageTreeSet(pTempMessage->GetMessageId());
        DeMessageInQueue(pTempMessage, pCoreCtx);
        pTempMessage->SetOwnerConnection(NULL);
    }
    std::map<uint32_t, CSearchAgentMessage *>::iterator iterOutQueue = m_mapOutMessageMapper.begin();
    while (iterOutQueue != m_mapOutMessageMapper.end())
    {
        CSearchAgentMessage *pTempMessage = iterOutQueue->second;
        iterOutQueue++;
        pCoreCtx->GetMessagePool()->DeMessageTreeSet(pTempMessage->GetMessageId());
        DeMessageOutQueue(pTempMessage, pCoreCtx);
        //delete pTempMessage;
    }

    if (this->m_pRecievingMessage != NULL)
    {
        delete this->m_pRecievingMessage;
        this->m_pRecievingMessage = NULL;
    }
    CEventBase::Instance()->EventDelConnection(this);
    pCoreCtx->RemoveCacheSendEvent(this);
    CSearchAgentConnectionPool::Instance()->ReclaimConnection(m_iConnId);
    close(m_iFd);
    m_iFd = -1;
    delete this;
}

void CClientConn::MakeError(CSearchAgentMessage *pMessage,
                            SEARCH_AGENT_ERR_CODE iErrorCode)
{
    pMessage->MakeErrorMessage(iErrorCode);
}

CClientConn::~CClientConn()
{
}

void CClientConn::SendDone(CSearchAgentCore *pCoreCtx, CSearchAgentMessage *pMessage)
{
    (static_cast<CSearchAgentRequest *>(pMessage))->SetRequestDone(true);
    pCoreCtx->GetMessagePool()->DeMessageTreeSet(pMessage->GetMessageId());
}
