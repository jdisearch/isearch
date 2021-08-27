#include "sa_msg.h"
#include "log.h"
#include "sa_conn.h"
#include "http_parser.h"
#include "http_response_msg.h"
#include "sa_core.h"
#include "protocol.h"
#include "protocal.h"
#include "sa_error.h"
#include "sa_request.h"
#include "sa_response.h"

#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>
#include <limits.h>




void CSearchAgentMessage::PushBufferQueue(MessageBuffer *pBuffer)
{
    m_oBufferQueue.push(pBuffer);
}

MessageBuffer *CSearchAgentMessage::GetRecvBuffer()
{
    if (m_oBufferQueue.size() == 0)
    {
        return NULL;
    }

    MessageBuffer *pBuffer = m_oBufferQueue.back();
    return pBuffer;
}


bool CSearchAgentMessage::IsEmpty()
{
    if (m_oBufferQueue.empty())
    {
        return true;
    }
    MessageBuffer *pBuffer = m_oBufferQueue.back();

    return pBuffer->isEmpty();
}

int CSearchAgentMessage::MessageParsed(CSearchAgentCore *pCoreCtx)
{
    log_debug("CSearchAgentMessage::%s", __FUNCTION__);
    MessageBuffer *pBuffer = m_oBufferQueue.back();
    if (pBuffer->UnReadSize() == 0)
    {
        m_pOwnerConnection->RecvDone(pCoreCtx, this, NULL);
        log_debug("no more data to parse, recv done");
        return 0;
    }

    MessageBuffer *pNextBuffer = pBuffer->Split();
    if (pNextBuffer == NULL)
    {
        return -1;
    }

    CSearchAgentMessage *pNextAgentMessage = pCoreCtx->GetMessagePool()->GetMessage(m_pOwnerConnection, pCoreCtx, m_isRequest);
    if (pNextAgentMessage == NULL)
    {
        log_error("msg split error, because of get a new msg error");
        // m_pOwnerConnection->m_isError = true;
        return -1;
    }

    pNextAgentMessage->PushBufferQueue(pNextBuffer);
    m_pOwnerConnection->RecvDone(pCoreCtx, this, pNextAgentMessage);
    return 0;
}

EMessageType CSearchAgentMessage::GetMessageType()
{
    return m_eMessageType;
}

CSearchAgentMessage::~CSearchAgentMessage()
{
    log_debug("CSearchAgentMessage::%s %u", __FUNCTION__, m_iMessageId);
    while (!m_oBufferQueue.empty())
    {
        MessageBuffer *pBuffer = m_oBufferQueue.front();
        if (pBuffer != NULL)
        {
            delete pBuffer;
        }
        m_oBufferQueue.pop();
    }
    for (size_t i = 0; i < m_arrFragmentResponseMessages.size(); i++)
    {
        if (m_arrFragmentResponseMessages[i] != NULL)
        {
            delete m_arrFragmentResponseMessages[i];
        }
    }
    m_arrFragmentResponseMessages.clear();
}

void CSearchAgentMessage::PushBackFragmentResponse(CSearchAgentMessage *pResponseMessage)
{
    m_arrFragmentResponseMessages.push_back(pResponseMessage);
    m_iRecievedFragment++;
}

void CSearchAgentMessage::SetPeerConnection(ConnectionInterface *pPeerConnection)
{

    m_pPeerConnectionIds.push_back(pPeerConnection->GetConnectionId());
}

const vector<uint32_t> &CSearchAgentMessage::GetPeerConnection()
{
    return m_pPeerConnectionIds;
}

void CSearchAgentMessage::SetSending(bool isSending)
{
    m_isSending = isSending;
}

char *CSearchAgentMessage::GetTransferBuffer()
{
    return m_pTransferBuffer;
}

int CSearchAgentMessage::GetTransferLength()
{
    return m_iTransferLength;
}

bool CSearchAgentMessage::GetIsSending()
{
    return m_isSending;
}

ConnectionInterface *CSearchAgentMessage::GetOwnerConnection()
{
    return m_pOwnerConnection;
}

void CSearchAgentMessage::IncrRecievedFragment()
{
    m_iRecievedFragment++;
}

bool CSearchAgentMessage::IsRequestDone()
{
    return m_iRecievedFragment == m_iFragmentMessageCount;
}

void CSearchAgentMessage::MessageFragment(unsigned int iFragmentMessageCount)
{
    m_iFragmentMessageCount = iFragmentMessageCount;
}

uint32_t CSearchAgentMessage::GetMessageId()
{
    return m_iMessageId;
}

uint32_t CSearchAgentMessage::GetPeerMessageId()
{
    return m_iPeerMessageId;
}

const string &CSearchAgentMessage::GetResponseBody()
{
    return m_strResponseBody;
}




void CSearchAgentMessage::SetOwnerConnection(ConnectionInterface *pOwnerConnection)
{
    m_pOwnerConnection = pOwnerConnection;
}

void CSearchAgentMessage::MakeErrorMessage(SEARCH_AGENT_ERR_CODE iCode)
{
    log_debug("CSearchAgentMessagePool::%s, iCode %d", __FUNCTION__, iCode);
    CHttpResponseMessage httpResponseMessgge;
    vector<string> vecCookie;
    std::string errMessage = GetErrorMessage(iCode);
    log_debug("CSearchAgentMessagePool::%s, errmsg %s", __FUNCTION__, errMessage.c_str());

    this->m_strResponseBody = httpResponseMessgge.Encode(2,
                                                         false,
                                                         "utf-8",
                                                         "application/json",
                                                         errMessage,
                                                         200,
                                                         vecCookie,
                                                         "",
                                                         "");
    m_pTransferBuffer = const_cast<char *>(m_strResponseBody.c_str());
    m_iTransferLength = m_strResponseBody.length();
}


