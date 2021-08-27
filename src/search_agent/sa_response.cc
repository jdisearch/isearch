#include "sa_response.h"
#include "log.h"
#include "protocal.h"
#include "protocol.h"

CSearchAgentResponse::CSearchAgentResponse(uint64_t iMessageId, ConnectionInterface *pConnection):
CSearchAgentMessage(iMessageId, pConnection, false)
{
}

int CSearchAgentResponse::ParseMessage(ConnectionInterface *pConnection,
                                       CSearchAgentCore *pCoreCtx)
{
    int status;
    if (IsEmpty())
    {
        log_debug("empty msg, no data to parse");
        return 0;
    }

    ParseResponse();

    switch (m_eParseResult)
    {
    case MSG_PARSE_OK:

        status = MessageParsed(pCoreCtx);
        break;
    case MSG_PARSE_REPAIR:
        status = 0;
        break;
    case MSG_PARSE_AGAIN:
        status = 0;
        break;
    }
    return status;
}

void CSearchAgentResponse::ParseResponse()
{
    log_debug("CSearchAgentResponse::%s", __FUNCTION__);
     MessageBuffer *pBuffer = m_oBufferQueue.back();
    if (pBuffer->UnReadSize() < sizeof(CPacketHeader))
    {
        m_eParseResult = MSG_PARSE_AGAIN;
        return;
    }
    CPacketHeader *pTempHeader = (CPacketHeader *)(pBuffer->ReadPos());
    pTempHeader->magic = ntohs(pTempHeader->magic);
   // if(pTempHeader->magic != PROTOCAL_MAGIC )
    //{
    //	log_debug("CSearchAgentResponse::%s, magic is error", __FUNCTION__  );
//	m_eParseResult = MSG_PARSE_REPAIR;
//	return;
  //  }
    pTempHeader->cmd = ntohs(pTempHeader->cmd);
    pTempHeader->seq_num = ntohl(pTempHeader->seq_num);
    pTempHeader->len = ntohl(pTempHeader->len);
    if (pBuffer->UnReadSize() < (sizeof(CPacketHeader) + pTempHeader->len))
    {
    	log_debug("CSearchAgentResponse::%s, need %lu , pratical is %d", __FUNCTION__, (sizeof(CPacketHeader) + pTempHeader->len), pBuffer->UnReadSize() );
        m_eParseResult = MSG_PARSE_AGAIN;
        return;
    }
    m_iPeerMessageId = pTempHeader->seq_num;
    m_strResponseBody = string(pBuffer->ReadPos() + sizeof(CPacketHeader), pTempHeader->len);
    log_debug("recieve peer msg id %d, %s", m_iPeerMessageId, m_strResponseBody.c_str());
    pBuffer->IncrReadPosition(sizeof(CPacketHeader) + pTempHeader->len);
    m_eParseResult = MSG_PARSE_OK;
}


CSearchAgentResponse::~CSearchAgentResponse()
  {

  }
