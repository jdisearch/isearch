#include "sa_server.h"
#include "sa_msg.h"
#include "sa_core.h"
#include "sa_error.h"
#include "sa_server_conn.h"
#include "sa_epoll.h"
#include "log.h"

void CInstanceConn::Reference(void *pOwner)
{
    log_debug("CInstanceConn::%s", __FUNCTION__);
    CSearchServerInstance *ins = static_cast<CSearchServerInstance *>(pOwner);
    struct sockinfo oSockInfo;
    log_debug("resolve inet %s, %d", ins->GetIpAddress().c_str(), ins->GetPort());
    resolve_inet(ins->GetIpAddress(), ins->GetPort(), &oSockInfo);
    memcpy(&m_oSocketAddress, &(oSockInfo.addr.in), sizeof(sockaddr_in));
    m_pOwnerServer = ins;
}

void CInstanceConn::EnMessageInQueue(CSearchAgentMessage *pMessage,
                                     CSearchAgentCore *pCoreCtx)
{
    log_debug("CInstanceConn::%s", __FUNCTION__);
    m_oInMessageQueue.push(pMessage);
    m_oRecvMessages.insert(make_pair(pMessage->GetMessageId(),pMessage));
    int timeout = m_pOwnerServer->GetTimeout();
    MsgNode msgNode;
    uint64_t key = get_now_us() + timeout * 1000000;
    msgNode.key = key;
    msgNode.msg_id = pMessage->GetMessageId();
    log_debug("CInstanceConn::%s, EnMessageTreeSet msg_id %d", __FUNCTION__, msgNode.msg_id);
    msgNode.data = pMessage;
    pCoreCtx->GetMessagePool()->EnMessageTreeSet(msgNode);
}

int CInstanceConn::Send(CSearchAgentCore *pCoreCtx)
{
    log_debug("CInstanceConn::%s", __FUNCTION__);
    return MessageSend(pCoreCtx);
}

CSearchAgentMessage *CInstanceConn::SendNext(CSearchAgentCore *pCoreCtx)
{
    log_debug("CInstanceConn::%s", __FUNCTION__);
    int status;
    if (m_oInMessageQueue.size() == 0)
    {
        status = CEventBase::Instance()->EventDelOut(this);
        if (status != 0)
        {
            m_isError = true;
        }
        return NULL;
    }
    CSearchAgentMessage *pMessage = NULL;
    pMessage = m_pSendMessage;
    if (pMessage != NULL)
    {
       m_oInMessageQueue.pop();
    }
    if (m_oInMessageQueue.empty())
    {
        m_pSendMessage = NULL;
        return NULL;
    }
    m_pSendMessage = m_oInMessageQueue.front();

    return m_pSendMessage;
}

int CInstanceConn::InstanceConnect()
{
    log_debug("CInstanceConn::%s", __FUNCTION__);
    int status;
    if (IsConnected())
    {
        return 0;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        log_error("socket for server error %s", strerror(errno));
        return -1;
    }
    m_iFd = fd;

    status = fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (status < 0)
    {
        log_error("fcntl FD_CLOEXEC for server error %s", strerror(errno));
        return -1;
    }

    status = set_nonblocking(fd);
    if (status < 0)
    {
        log_error("set fd %d nonblocking error %s", fd, strerror(errno));
        return -1;
    }
    int nodelay = 1;
    socklen_t len = sizeof(nodelay);
    status = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, len);
    if (status < 0)
    {
        log_error("set tcp no_delay for server error %s", strerror(errno));
        return -1;
    }

    status = connect(fd, &m_oSocketAddress, sizeof(struct sockaddr));
    if (status != 0)
    {
        log_error("connecnt error %d ,errno %d, errmsg %s", m_oSocketAddress.sa_family, errno, strerror(errno));
        if (errno == EINPROGRESS)
        {
            m_isConnecting = true;
            return 0;
        }
        log_error("connect failed. %s", strerror(errno));
        return -1;
    }
    return 0;
}

int CInstanceConn::Recieve(CSearchAgentCore *pCoreCtx)
{
    log_debug("CInstanceConn::%s", __FUNCTION__);
    return MessageRecieve(pCoreCtx);
}

CSearchAgentMessage *CInstanceConn::RecieveNext(CSearchAgentCore *pCoreCtx,
                                                bool isAlloc)
{
    log_debug("CInstanceConn::%s", __FUNCTION__);
    CSearchAgentMessage *pTempMessage;
    if (m_isEof)
    {
        pTempMessage = m_pRecievingMessage;
        if (pTempMessage != NULL)
        {
        }
        return NULL;
    }
    pTempMessage = m_pRecievingMessage;
    if (pTempMessage != NULL)
    {
        return pTempMessage;
    }
    if (!isAlloc)
    {
        return NULL;
    }
    CSearchAgentMessagePool *pMessagePool = pCoreCtx->GetMessagePool();
    pTempMessage = pMessagePool->GetMessage(this, pCoreCtx, false);
    if (pTempMessage != NULL)
    {
        m_pRecievingMessage = pTempMessage;
    }

    return pTempMessage;
}

void CInstanceConn::RecvDone(CSearchAgentCore *pCoreCtx,
                             CSearchAgentMessage *pCurrentMessage,
                             CSearchAgentMessage *pNextMessage)
{
    log_debug("CInstanceConn::%s", __FUNCTION__);
    m_pRecievingMessage = pNextMessage;
    uint32_t iPeerMessageId = pCurrentMessage->GetPeerMessageId();
    std::map<uint32_t, CSearchAgentMessage *>::iterator it = m_oConnectionMessages.find(iPeerMessageId);
    if (it == m_oConnectionMessages.end())
    {
        log_debug("response msg id error");
        return;
    }
    CSearchAgentMessage *pRequest = it->second;
    ConnectionInterface *pClientConnection = pRequest->GetOwnerConnection();
    if (pClientConnection == NULL)
    {
        log_error("client connection is closed, swallow");
        return;
    }
    pRequest->PushBackFragmentResponse(pCurrentMessage);
    this->m_oConnectionMessages.erase(iPeerMessageId);
    this->m_oRecvMessages.erase(iPeerMessageId);
    ResponseForward(pCoreCtx, pCurrentMessage, pClientConnection, pRequest);
}

void CInstanceConn::SendDone(CSearchAgentCore *pCoreCtx,
                             CSearchAgentMessage *pMessage)
{
    log_debug("CInstanceConn::%s, msg_id %ud", __FUNCTION__, pMessage->GetMessageId());
    this->m_oRecvMessages.erase(pMessage->GetMessageId());
    m_oConnectionMessages.insert(make_pair(pMessage->GetMessageId(), pMessage));
}

void CInstanceConn::ResponseForward(CSearchAgentCore *pCoreContxt,
                                    CSearchAgentMessage *pResponseMessage,
                                    ConnectionInterface *pClientConnection,
                                    CSearchAgentMessage *pRequestMessage)
{
    log_debug("CInstanceConn::%s", __FUNCTION__);
    if (!pRequestMessage->IsRequestDone())
    {
        return;
    }
    int status = pRequestMessage->Coalesce();
    if (status < 0)
    {
        log_error("CInstanceConn::%s, coalesce error", __FUNCTION__);
        return;
    }
    pClientConnection->DeMessageInQueue(pRequestMessage, pCoreContxt);
    pClientConnection->EnMessageOutQueue(pRequestMessage, pCoreContxt);
}

void CInstanceConn::DeMessageTree(CSearchAgentMessage *pMessage,
                                  CSearchAgentCore *pCoreCtx)
{
    m_oConnectionMessages.erase(pMessage->GetMessageId());
}

void CInstanceConn::Close(CSearchAgentCore *pCoreCtx)
{
    log_debug("CInstanceConn::%s, pending task %d", __FUNCTION__, (int)m_oRecvMessages.size());
    if (m_pRecievingMessage != NULL)
    {
        delete m_pRecievingMessage;
        m_pRecievingMessage = NULL;
    }
    std::map<uint32_t, CSearchAgentMessage *>::iterator iter = m_oRecvMessages.begin();   
    for(; iter != m_oRecvMessages.end(); iter++){
    	log_debug("CInstanceConn::%s, pending task %d", __FUNCTION__, iter->first);
        CSearchAgentMessage *pTempRequestMessage = iter->second;
        ConnectionInterface *pClientConnection = pTempRequestMessage->GetOwnerConnection();
        if (pClientConnection == NULL)
        {
            log_error("client connection is closed, swallow");
            continue;
        }
        if (pTempRequestMessage->GetMessageType() == INDEX_READ_MSG)
        {
            pTempRequestMessage->PushBackFragmentResponse(NULL);
            this->m_oConnectionMessages.erase(pTempRequestMessage->GetMessageId());
            ResponseForward(pCoreCtx, NULL, pClientConnection, pTempRequestMessage);
        }
        else
        {
            pTempRequestMessage->MakeErrorMessage(ERR_SERVER_ERROR);
            pClientConnection->DeMessageInQueue(pTempRequestMessage, pCoreCtx);
            pClientConnection->EnMessageOutQueue(pTempRequestMessage, pCoreCtx);
            pCoreCtx->CacheSendEvent(pClientConnection);
        }
    }
    m_oRecvMessages.clear();
    m_oConnectionMessages.clear();
    m_pOwnerServer->SetOwnerConnection(NULL);
    m_pOwnerServer = NULL;
    
    CSearchAgentConnectionPool::Instance()->ReclaimConnection(m_iConnId);
    CEventBase::Instance()->EventDelConnection(this);
    pCoreCtx->RemoveCacheSendEvent(this);
    delete this;
}
