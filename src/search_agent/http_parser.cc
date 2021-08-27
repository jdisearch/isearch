#include "http_parser.h"
#include "log.h"
#include "json/json.h"

#include <string.h>
#include <stdlib.h>

const string DOC_ID_HEADER = "doc_id";

bool CHttpPacketMessage::IsReadCompleted(const char *pBuffer, unsigned int iLen, int &packetLen)
{
    const char *pEnd = strstr(pBuffer, "\r\n\r\n");
    while (pEnd != NULL)
    {
        if (0 == strncmp(pBuffer, "GET", 3))
        {
            packetLen = pEnd + 4 - pBuffer;
            return true;
        }
        else if (0 == strncmp(pBuffer, "POST", 4))
        {
            const char *pFound = strstr(pBuffer, "\r\n");
            int lenbody = -1;
            while (pFound != NULL && pFound < pEnd)
            {
                log_debug("CHttpPacketMessage::%s,-------post find content-length\n", __FUNCTION__);
                if (strncasecmp(pFound, "\r\nContent-length", 16) == 0)
                {
                    sscanf(pFound + 16, "%*[: ]%d", &lenbody);
                    log_debug("CHttpPacketMessage::%s,-------lenbody[%d]\n", __FUNCTION__, lenbody);
                    break;
                }
                pFound = strstr(pFound + 2, "\r\n");
            }
            if (lenbody != -1)
            {
                log_debug("CHpsConnection::%s,content-length[%d]\n", __FUNCTION__, lenbody);
                if (pBuffer + iLen >= pEnd + 4 + lenbody)
                {
                    packetLen = pEnd + 4 + lenbody - pBuffer;
                    log_debug("CHttpPacketMessage::%s,complement packet\n", __FUNCTION__);
                    return true;
                }
                else
                {
                    log_debug("CHttpPacketMessage::%s,incomplement packet\n", __FUNCTION__);
                    break;
                }
            }
        }
        pEnd = strstr(pBuffer, "\r\n\r\n");
    }
    return false;
}

char *CHttpPacketMessage::FindLastString(char *pBegin, char *pEnd, const char *pSub)
{
    char *pLastSpace = NULL;
    char *pSpace = strstr(pBegin, pSub);
    while (pSpace != NULL && pSpace <= pEnd)
    {
        pLastSpace = pSpace;
        pSpace = strstr(pSpace + 1, pSub);
    }
    return pLastSpace;
}

int CHttpPacketMessage::Decode(char *pBuffer, int iLen)
{
    char *pBegin = pBuffer;
    char *pEnd = strstr(pBegin, "\r\n");
    if (pEnd == NULL || pEnd - pBuffer >= iLen)
        return -1;

    char szHttpVersion[256] = {0};

    if (0 == strncmp(pBuffer, "GET", 3))
    {
        m_isPost = false;
        pBegin += 4;
        char *pSpace = FindLastString(pBegin, pEnd, " ");
        if (pSpace == NULL || pSpace >= pEnd)
        {
            log_error("CHttpPacketParser::%s, DecodeFail %s\n", __FUNCTION__, pBegin);
            return -1;
        }
        char *pSplit = strstr(pBegin, "?");
        if (pSplit != NULL && pSplit < pSpace)
        {
            m_strPath = string(pBegin, pSplit - pBegin);
            m_strBody = string(pSplit + 1, pSpace - pSplit - 1);
        }
        else
        {
            m_strPath = string(pBegin, pSpace - pBegin);
        }
        pBegin = pSpace + 1;
        memcpy(szHttpVersion, pBegin, pEnd - pBegin);
        if (strncasecmp(szHttpVersion, "http/1.1", 8) == 0)
        {
            m_iHttpVersion = 1;
            m_isKeepAlive = true;
        }
        else
        {
            m_iHttpVersion = 0;
            m_isKeepAlive = false;
        }
        pBegin = strstr(pBegin, "\r\n") + 2;
        const char *pHeadersEnd = strstr(pBegin, "\r\n\r\n");
        ParseHeaders(pBegin, pHeadersEnd - pBegin + 4);
    }
    else if (0 == strncmp(pBuffer, "POST", 4))
    {
        m_isPost = true;
        pBegin += 5;
        char *pSpace = FindLastString(pBegin, pEnd, " ");
        if (pSpace == NULL || pSpace >= pEnd)
        {
            log_error("CHttpPacketParser::%s, DecodeFail %s\n", __FUNCTION__, pBegin);
            return -1;
        }
        char *pSplit = NULL; 
        pSplit = strstr(pBegin, "?");
        if (pSplit != NULL && pSplit < pSpace)
        {
            m_strPath = string(pBegin, pSplit - pBegin);
            m_strBody = string(pSplit + 1, pSpace - pSplit - 1);
        }
        else
        {
            m_strPath = string(pBegin, pSpace - pBegin);
        }
        pBegin = pSpace + 1;
        memcpy(szHttpVersion, pBegin, pEnd - pBegin);

        if (strncasecmp(szHttpVersion, "http/1.1", 8) == 0)
        {
            m_iHttpVersion = 1;
            m_isKeepAlive = true;
        }
        else
        {
            m_iHttpVersion = 0;
            m_isKeepAlive = false;
        }
        pBegin = strstr(pBegin, "\r\n") + 2;
        const char *pHeadersEnd = strstr(pBegin, "\r\n\r\n");
        ParseHeaders(pBegin, pHeadersEnd - pBegin + 4);
        if (m_iContentLen > 0)
        {
            if (m_strBody.length() > 0)
                m_strBody = string("&") + m_strBody;
            m_strBody = string(pBuffer + iLen - m_iContentLen, m_iContentLen) + m_strBody;
            log_debug("CHttpPacketMessage::%s,m_strBody[%s] len[%ld]\n", __FUNCTION__, m_strBody.c_str(), m_strBody.length());
        }
    }
    else
    {
        return -1;
    }

    if (m_strPath == INDEX_WRITE_URI)
    {
        std::map<std::string, std::string>::const_iterator iter = m_mapHeaders.find(DOC_ID_HEADER);
        if (iter != m_mapHeaders.end())
        {
            m_strDocumentId = iter->second;
        }
        else
        {
            return -1;
        }
    }
    else if (m_strPath == INDEX_READ_URI)
    {
        Json::Reader jsonRequestBodyReader;
        Json::Value jsonRequestBodyPacket;
        int ret = jsonRequestBodyReader.parse(m_strBody, jsonRequestBodyPacket);
         if (ret == 0)
        {
            return -1;
        }
        if (jsonRequestBodyPacket.isMember("page_index") && jsonRequestBodyPacket["page_index"].isInt())
        {
            m_iPageIndex = jsonRequestBodyPacket["page_index"].asInt();
        }
        if (jsonRequestBodyPacket.isMember("page_size") && jsonRequestBodyPacket["page_size"].isInt())
        {
            m_iPageSize = jsonRequestBodyPacket["page_size"].asInt();
        }
        if (jsonRequestBodyPacket.isMember("sort_type") && jsonRequestBodyPacket["sort_type"].isInt())
        {
            m_iSortType = jsonRequestBodyPacket["sort_type"].asInt();
        }
        jsonRequestBodyPacket["page_index"] = 1;
        jsonRequestBodyPacket["page_size"] = m_iPageSize *  m_iPageIndex;
        Json::FastWriter wr;
	m_strBody = wr.write(jsonRequestBodyPacket);
    }
    int iLength = m_strBody.length();
    int iPacketLen = iLength + sizeof(CPacketHeader);
    m_pTransferPacket = new char[iPacketLen];
    if (m_pTransferPacket == NULL)
    {
        log_error("no new memory for packet");
        return -1;
    }

    CPacketHeader *pHeader = (CPacketHeader *)m_pTransferPacket;
    pHeader->magic = htons(PROTOCOL_MAGIC);

    pHeader->cmd = (m_strPath == INDEX_WRITE_URI) ? htons(SERVICE_INDEXGEN) : htons(SERVICE_SEARCH);
    pHeader->len = htonl(iLength);
    pHeader->seq_num = htonl(m_iOwnerMessageId);
    memcpy(m_pTransferPacket + sizeof(CPacketHeader), m_strBody.c_str(), iLength);
    return 0;
}

void CHttpPacketMessage::ParseHeaders(const char *pHeaders, unsigned int iLen)
{
    m_iContentLen = 0;
    char strHead[64] = {0};
    char strHeadValue[128] = {0};
    const char *pBegin = pHeaders;
    const char *pEnd = strstr(pBegin, "\r\n");
    while (pBegin != pEnd)
    {
        if (sscanf(pBegin, "%63[^:]: %127[^\r]", strHead, strHeadValue) == 2)
        {
            if (strncasecmp(pBegin, "cookie", 6) == 0)
            {
                m_strCookie = strHeadValue;
                pBegin = pEnd + 2;
                pEnd = strstr(pBegin, "\r\n");
                continue;
            }
            else if ((strncasecmp(strHead, "connection", 10) == 0))
            {
                if (strncasecmp(strHeadValue, "close", 6) == 0)
                {
                    m_isKeepAlive = false;
                }
                else if (strncasecmp(strHeadValue, "keep-alive", 10) == 0)
                {
                    m_isKeepAlive = true;
                }
            }
            else if (strncasecmp(strHead, "x-forwarded-for", 16) == 0)
            {
                char szIp[128] = {0};
                char *pLocate = strchr(strHeadValue, ',');
                if (NULL != pLocate)
                {
                    memcpy(szIp, strHeadValue, (int)(pLocate - strHeadValue));
                }
                else
                {
                    memcpy(szIp, strHeadValue, strlen(strHeadValue));
                }
                m_strXForwardedFor = szIp;
                if (m_strXForwardedFor.size() > 0)
                    m_isXForwardedFor = true;
            }
            else if (strncasecmp(strHead, "host", 4) == 0)
            {
                m_strHost = strHeadValue;
            }
            else if (strncasecmp(strHead, "x-ff-port", 9) == 0)
            {
                m_xffport = atoi(strHeadValue);
            }
            else if (strncasecmp(strHead, "content-type", 12) == 0)
            {
                m_strContentType = strHeadValue;
            }
            else if (strncasecmp(strHead, "content-length", 14) == 0)
            {
                m_iContentLen = atoi(strHeadValue);
            }

            m_mapHeaders.insert(std::make_pair(strHead, strHeadValue));
            pBegin = pEnd + 2;
            pEnd = strstr(pBegin, "\r\n");
        }
    }
}

char *CHttpPacketMessage::GetTransferPacket()
{
    return m_pTransferPacket;
}

int CHttpPacketMessage::GetTransferLength()
{
    return m_strBody.length() + sizeof(CPacketHeader);
}

void CHttpPacketMessage::GetIndexReadParameter(uint32_t &iPageInex, uint32_t &iPageSize, uint32_t &iSortType)
{
    iPageSize = m_iPageSize;
    iPageInex = m_iPageIndex;
    iSortType = m_iSortType;
}
