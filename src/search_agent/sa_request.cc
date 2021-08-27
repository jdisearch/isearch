#include "sa_request.h"
#include "http_response_msg.h"
#include "comm_enum.h"
#include <algorithm>

#define FLOAT_PRECISION 0.0000001

bool jsonComp(const Json::Value &d1, const Json::Value &d2)
{

    if (d1["socre"].isDouble())
    {
        double fDocument1Score = d1["score"].asDouble();
        double fDocument2Score = d2["score"].asDouble();

        if (fDocument1Score - fDocument2Score > FLOAT_PRECISION)
        {
            return false;
        }
        else if (fDocument1Score - fDocument2Score < 0.0000001)
        {
            return true;
        }
    }
    else
    {
        std::string strDocument1Score = d1["score"].asString();
        std::string strDocument2Score = d2["score"].asString();
        if (strDocument1Score != strDocument2Score)
        {
            return strDocument1Score < strDocument2Score;
        }
    }
    string strDocument1Docid = d1["doc_id"].asString();
    string strDocument2Docid = d2["doc_id"].asString();
    return strDocument1Docid < strDocument2Docid;
}

CSearchAgentRequest::CSearchAgentRequest(uint64_t iMessageId, ConnectionInterface *pConnection) : CSearchAgentMessage(iMessageId, pConnection, true)
{
    m_pHttpMessage = NULL;
    m_iDocumentCount = 0;
    m_isRequestDone=  false;
}

int CSearchAgentRequest::ParseMessage(ConnectionInterface *pConnection,
                                      CSearchAgentCore *pCoreCtx)
{
    int status;
    if (IsEmpty())
    {
        log_debug("empty msg, no data to parse");
        return 0;
    }

    ParseRequest();

    switch (m_eParseResult)
    {
    case MSG_PARSE_OK:

        status = MessageParsed(pCoreCtx);
        break;
    case MSG_PARSE_REPAIR:
        status = -1;
        break;
    case MSG_PARSE_AGAIN:
        status = 0;
        break;
    }
    return status;
}

void CSearchAgentRequest::ParseRequest()
{
    CHttpPacketMessage *httpPacketMessage = new CHttpPacketMessage(m_iMessageId);
    if (httpPacketMessage == NULL)
    {
        log_error("lack of memory for http parser");
        m_eParseResult = MSG_PARSE_AGAIN;

        // m_pOwnerConnection->m_isError =  true;
        return;
    }
    MessageBuffer *pBuffer = m_oBufferQueue.back();
    int iPacketLength;
    bool isReadCompleted = httpPacketMessage->IsReadCompleted(pBuffer->ReadPos(), pBuffer->UnReadSize(), iPacketLength);
    if (isReadCompleted)
    {
        if (0 == httpPacketMessage->Decode(pBuffer->ReadPos(), iPacketLength))
        {
            pBuffer->IncrReadPosition(iPacketLength);
            m_pHttpMessage = httpPacketMessage;
            this->m_pTransferBuffer = httpPacketMessage->GetTransferPacket();
            this->m_iTransferLength = httpPacketMessage->GetTransferLength();
            if (httpPacketMessage->GetPath() == INDEX_READ_URI)
            {
                m_eMessageType = INDEX_READ_MSG;
            }
            else if (httpPacketMessage->GetPath() == INDEX_WRITE_URI)
            {
                m_eMessageType = INDEX_WRITE_MSG;
            }
            else
            {
                m_eMessageType = UNKNOWN_INDEX_MSG;
            }
            m_eParseResult = MSG_PARSE_OK;
        }
        else
        {
            delete httpPacketMessage;
            m_eParseResult = MSG_PARSE_REPAIR;
        }
    }
    else
    {
        delete httpPacketMessage;
        m_eParseResult = MSG_PARSE_AGAIN;
    }
}

bool CSearchAgentRequest::IsRequestDone()
{
    return m_isRequestDone;
}

void CSearchAgentRequest::SetRequestDone(bool isDone)
{
    m_isRequestDone = isDone;
}

const std::string &CSearchAgentRequest::GetDocumentId()
{
    return m_pHttpMessage->GetDocumentId();
}

int CSearchAgentRequest::Coalesce()
{
    log_debug("CSearchAgentMessage::%s", __FUNCTION__);
    if (m_arrFragmentResponseMessages.size() == 0)
    {
        log_error("no valid search agent message");
        return -1;
    }
    if (m_eMessageType == INDEX_WRITE_MSG)
    {
        CoalesceWriteResponse();
    }
    else
    {
        CoalesceReadResponse();
    }
    return 0;
}

void CSearchAgentRequest::CoalesceWriteResponse()
{
    log_debug("CSearchAgentRequest::%s", __FUNCTION__);

    CHttpResponseMessage httpResponseMessgge;
    vector<string> vecCookie;
    this->m_strResponseBody = httpResponseMessgge.Encode(2,
                                                         false,
                                                         "utf-8",
                                                         "application/json",
                                                         m_arrFragmentResponseMessages[0]->GetResponseBody(),
                                                         200,
                                                         vecCookie,
                                                         "",
                                                         "");
    log_debug("CSearchAgentRequest::%s, coalesce result\n  %s", __FUNCTION__, m_strResponseBody.c_str());
    m_pTransferBuffer = const_cast<char *>(m_strResponseBody.c_str());
    m_iTransferLength = m_strResponseBody.length();
    delete m_arrFragmentResponseMessages[0];
    m_arrFragmentResponseMessages.clear();
}

void CSearchAgentRequest::CoalesceReadResponse()
{
    log_debug("CSearchAgentRequest::%s", __FUNCTION__);
    std::vector<std::string> vecCookie;

    Json::Value jsonResult;
    Json::StyledWriter jsonResponseWriter;

    bool result = false;
    for (size_t i = 0; i < m_arrFragmentResponseMessages.size(); i++)
    {
        if (m_arrFragmentResponseMessages[i] == NULL)
        {
            continue;
        }
        result = true; 
        ParseOneResponse(m_arrFragmentResponseMessages[i]);
        delete m_arrFragmentResponseMessages[i];
        m_arrFragmentResponseMessages[i] = NULL;
    }
    if(!result)
    {
      m_arrFragmentResponseMessages.clear();
      MakeErrorMessage(ERR_SERVER_ERROR);
      return;
     
    }
    jsonResult["doc_cnt"] = m_iDocumentCount;

    sort(m_arrResultDocs.begin(), m_arrResultDocs.end(), jsonComp);
    uint32_t iPageIndex, iPageSize, iSortType;
    m_pHttpMessage->GetIndexReadParameter(iPageIndex, iPageSize, iSortType);
    log_debug("result doc size %zu", m_arrResultDocs.size());
    if (iSortType == SORT_FIELD_ASC || iSortType == SORT_GEO_DISTANCE)
    {
        for (size_t i = 0; i < m_arrResultDocs.size() && i < iPageIndex * iPageSize; i++)
        {
            if (i >= (iPageIndex - 1) * iPageSize && i < (iPageIndex)*iPageSize)
            {
                jsonResult["docs"].append(m_arrResultDocs[i]);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < m_arrResultDocs.size() && i < iPageIndex * iPageSize; i++)
        {
            if (i >= (iPageIndex - 1) * iPageSize && i < (iPageIndex)*iPageSize)
            {
                jsonResult["docs"].append(m_arrResultDocs[m_arrResultDocs.size() - 1 - i]);
            }
        }
    }
    std::string strRes = jsonResponseWriter.write(jsonResult);
    CHttpResponseMessage httpResponseMessgge;
    m_arrResultDocs.clear();
    this->m_strResponseBody = httpResponseMessgge.Encode(2,
                                                         false,
                                                         "utf-8",
                                                         "application/json",
                                                         strRes,
                                                         200,
                                                         vecCookie,
                                                         "",
                                                         "");
    log_debug("CSearchAgentRequest::%s, coalesce result\n  %s", __FUNCTION__, m_strResponseBody.c_str());
    m_pTransferBuffer = const_cast<char *>(m_strResponseBody.c_str());
    m_iTransferLength = m_strResponseBody.length();

    m_arrFragmentResponseMessages.clear();
}

void CSearchAgentRequest::ParseOneResponse(CSearchAgentMessage *pOnceResponse)
{
    log_debug("CSearchAgentRequest::%s", __FUNCTION__);
    Json::Reader jsonResponseReader(Json::Features::strictMode());
    Json::Value jsonResponsePacket;
    int ret = jsonResponseReader.parse(pOnceResponse->GetResponseBody(), jsonResponsePacket);
    if (ret == 0)
    {
        log_error("response from server is not valid json, %s", pOnceResponse->GetResponseBody().c_str());
	return ;
    }
    if (jsonResponsePacket.isMember("code") && jsonResponsePacket["code"].isInt())
    {
        if (jsonResponsePacket["code"].asInt() != 0)
        {
	  return ;
        }
    }
    int iTempCount;
    if (jsonResponsePacket.isMember("count") && jsonResponsePacket["count"].isInt())
    {
        iTempCount = jsonResponsePacket["count"].asInt();
        m_iDocumentCount += iTempCount;
    }
    else
    {
	  return ;
    }

    if (iTempCount <= 0)
    {
	  return ;
    }
    if (jsonResponsePacket.isMember("result") && jsonResponsePacket["result"].isArray())
    {
        Json::Value jsonResult = jsonResponsePacket["result"];
        for (uint32_t index = 0; index < jsonResult.size(); ++index)
        {
            Json::Value jsonDocInfo = jsonResult[index];
            if (jsonDocInfo.isMember("score") && jsonDocInfo.isMember("doc_id"))
            {
		if(m_setDocIds.find(jsonDocInfo["doc_id"].asString()) == m_setDocIds.end()){
                	m_arrResultDocs.push_back(jsonDocInfo);
                        m_setDocIds.insert(jsonDocInfo["doc_id"].asString());
		}
            }
        }
    }
}

CSearchAgentRequest::~CSearchAgentRequest()
{
   log_debug("CSearchAgentRequest");
   /*
    for (size_t i = 0; i < m_arrFragmentResponseMessages.size(); i++)
    {
        if (m_arrFragmentResponseMessages[i] != NULL)
        {
            delete m_arrFragmentResponseMessages[i];
        }
    }
    m_arrFragmentResponseMessages.clear();
    */
    if (m_pHttpMessage != NULL)
    {
        delete m_pHttpMessage;
        m_pHttpMessage = NULL;
    }
}
