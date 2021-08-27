#include "sa_conn.h"
#include "sa_server.h"
#include "sa_listener.h"
#include "sa_msg.h"
#include "sa_request.h"
#include "log.h"
#include "sa_client_conn.h"
#include "sa_server_conn.h"
#include "sa_listener.h"
#include "http_parser.h"

#define IOV_MAX 128
#define SSIZE_MAX 0x0FFFFFF


void ConnectionInterface::SetFd(int iFd)
{
    m_iFd = iFd;
}

int ConnectionInterface::GetFd()
{
    return m_iFd;
}

int ConnectionInterface::MessageRecvChain(CSearchAgentCore *pCoreCtx, CSearchAgentMessage *pMessage)
{
    log_debug("ConnectionInterface::MessageRecvChain");
    CSearchAgentMessage *pNextMessage;
    MessageBuffer *pBuffer = pMessage->GetRecvBuffer();
    if (pBuffer == NULL)
    {
        pBuffer = new MessageBuffer();
        if (pBuffer == NULL || !pBuffer->Init())
        {
            if (pBuffer != NULL)
            {
                delete pBuffer;
            }
            log_error("get mbuf for conn error, lack of memory");
            return -1;
        }
        pMessage->PushBufferQueue(pBuffer);
        log_debug("MessageRecvChain, new buffer success");
    }
    else if (pBuffer->isFull())
    {
        log_debug("MessageRecvChain, buffer is full, enlarge");
        if (!pBuffer->Enlarge())
        {
            delete pBuffer;
            log_error("enlarge buffer erro, lack of memory");
            return -1;
        }
    }
    uint32_t iBufferSize = pBuffer->BufferSize();
    int n = ConnRecieve(pBuffer->WritePos(), iBufferSize);
    if (n <= 0)
    {
        if (n == 0)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
    pBuffer->IncrWritePosition(n);
    for (;;)
    {
        int status = pMessage->ParseMessage(this, pCoreCtx);
        if (status != 0)
        {
            pMessage->MakeErrorMessage(ERR_PARSE_MSG);
            EnMessageOutQueue(pMessage,pCoreCtx);
            return 0;
        }

        pNextMessage = RecieveNext(pCoreCtx, false);
        if (pNextMessage == NULL || pNextMessage == pMessage)
        {
            break;
        }
        pMessage = pNextMessage;
    }
    return 0;
}

int ConnectionInterface::ConnRecieve(void *pBuffer, size_t iSize)
{
    ssize_t n;

    log_debug("%s, Recieve Msg for fd %d", __FUNCTION__, m_iFd);

    for (;;)
    {
        n = read(m_iFd, pBuffer, iSize);
        log_debug("recv on fd %d required %zu of %lld", m_iFd, iSize, (long long int)n);
        if (n > 0)
        {
            if (n < (ssize_t)iSize)
            {
                m_eConnectionStatus &= ~RECV_READY;
            }
            return n;
        }
        if (n == 0)
        {
            m_eConnectionStatus &= ~RECV_READY;
            m_isEof = true;
            log_debug("recv on fd %d eof", m_iFd);
            return n;
        }
        if (errno == EINTR)
        {
            log_debug("recv on %d not ready", m_iFd);
            continue;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            m_eConnectionStatus &= ~RECV_READY;
            log_debug("recv on  %d not ready -eagin", m_iFd);
            return -2;
        }
        else
        {
            m_eConnectionStatus &= ~RECV_READY;
            return -1;
        }
    }
    return -1;
}

bool ConnectionInterface::IsConnected()
{
    return m_iFd > 0;
}

ConnectionInterface::~ConnectionInterface()
{
    if (m_iFd > 0)
    {
        close(m_iFd);
        m_iFd = -1;
    }
}

int ConnectionInterface::MessageRecieve(CSearchAgentCore *pCoreCtx)
{
    log_debug("CClientConn::%s", __FUNCTION__);
    CSearchAgentMessage *pMessage = NULL;
    int status;
    m_eConnectionStatus |= RECV_READY;
    do
    {
        pMessage = RecieveNext(pCoreCtx,true);
        if (pMessage == NULL)
        {
            log_debug("%s, get next msg error NULL ", __FUNCTION__);
            return 0;
        }
        status = MessageRecvChain(pCoreCtx, pMessage);
        if (status != 0)
        {
            return status;
        }
    } while (m_eConnectionStatus & RECV_READY);
    return 0;
}

int ConnectionInterface::MessageSend(CSearchAgentCore *pCoreCtx)
{
    int status;
    CSearchAgentMessage *pMessage;
    this->m_eConnectionStatus |= SEND_READY;
    do
    {
        pMessage = SendNext(pCoreCtx);
        if (pMessage == NULL)
        {
            return 0;
        }
        status = MessageSendChain(pCoreCtx, pMessage);
        if (status != 0)
        {
            return status;
        }

    } while (m_eConnectionStatus & SEND_READY);
    return status;
}

int ConnectionInterface::MessageSendChain(CSearchAgentCore *pCoreCtx, CSearchAgentMessage *pMessage)
{
    log_debug("CClientConn::%s", __FUNCTION__);
    vector<CSearchAgentMessage *> sendMessageQueue;
    struct iovec iovec_array[IOV_MAX];
    vector<iovec *> vecSendV;
    int iCount = 0;
    size_t iSend = 0;
    for (;;)
    {

        //sendMessageQueue.push(pMessage);
        pMessage->SetSending(true);
        struct iovec *pTempIovec = &iovec_array[iCount++];
        pTempIovec->iov_base = pMessage->GetTransferBuffer();
        pTempIovec->iov_len = pMessage->GetTransferLength();
        iSend += pTempIovec->iov_len;
       // vecSendV.push_back(pTempIovec);
        sendMessageQueue.push_back(pMessage);

        if (iCount >= IOV_MAX || iSend > SSIZE_MAX)
        {
            break;
        }
        pMessage = SendNext(pCoreCtx);
        if (pMessage == NULL)
        {
            break;
        }
    }

    log_debug("msg_data need send  %lu", iSend);
    int nSentCount, nStatus;
    if (iSend != 0 && iCount != 0)
    {
        nStatus = ConnectionSend(iovec_array, iCount, iSend);
    }
    else
    {
        nStatus = 0;
    }
    nSentCount =  nStatus > 0 ? nStatus : 0;

    for(size_t i = 0 ; i < sendMessageQueue.size() ; i++)
    {
        if(nSentCount == 0)
        {
            if(sendMessageQueue[i]->GetTransferLength() == 0)
            {
                sendMessageQueue[i]->SetSending(false);
                SendDone(pCoreCtx, sendMessageQueue[i]);
            }
            continue;
        }
        nSentCount -= sendMessageQueue[i]->GetTransferLength();
        if(nSentCount >= 0 ){
            SendDone(pCoreCtx, sendMessageQueue[i]);
        }
        
    }
    
    if(nStatus >= 0 || nStatus == -2)
    {
        return 0;
    }
    return -1;
}

int ConnectionInterface::ConnectionSend(struct iovec *iovec_array, int iElements, size_t iSend)
{
    ssize_t n;
    log_debug("enter connection send");
    for (;;)
    {
        n = writev(m_iFd, iovec_array, iElements);
        if (n > 0)
        {
            log_debug("send on sd %d returned n %d bytes of %zu", m_iFd, (int)n, iSend);
            if (n < (ssize_t)iSend)
            {
                m_eConnectionStatus &= ~SEND_READY;
            }
            return n;
        }
        if (n == 0)
        {
            log_warning("send on sd %d returned zero", m_iFd);
            m_eConnectionStatus &= ~SEND_READY;
            return 0;
        }
        if (errno == EINTR)
        {
            log_debug("sendv on %d not ready -eintr", m_iFd);
            continue;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            m_eConnectionStatus &= ~SEND_READY;
            log_debug("sendv on %d not ready -eagain", m_iFd);
            return -2;
        }
        else
        {
            m_eConnectionStatus &= ~SEND_READY;
            m_isError = true;
            log_error("sendv on %d failed %s", m_iFd, strerror(errno));

            return -1;
        }
    }
    return -1;
}

struct sockaddr* ConnectionInterface::GetSockAddr()
{
	return &m_oSocketAddress;
}


void ConnectionInterface::SendDone(CSearchAgentCore *pCoreCtx,CSearchAgentMessage* pMessage )
{
    
}


 bool ConnectionInterface::IsEof()
 {
     return m_isEof;
 }
