#ifndef __HTTP_PARSER_H__
#define __HTTP_PARSER_H__
#include <string>
#include <map>
#include "task_request.h"
#include "protocol.h"
#include "protocal.h"

using namespace std;

const string INDEX_READ_URI = "/search";
const string INDEX_WRITE_URI = "/insert";


class CHttpPacketMessage
{
public:
    CHttpPacketMessage(uint32_t iMessageId)
    {
        m_pTransferPacket = NULL;
        m_iOwnerMessageId = iMessageId;
        m_iPageIndex = 1;
        m_iPageSize = 10;
        m_iSortType = 1;

    }
    int Decode(char *pBuffer, int iLen);
    bool IsReadCompleted(const char *pBuffer, unsigned int iLen, int &packetLen);
    const string& GetPath() {return m_strPath ;}
    const string& GetDocumentId() {return m_strDocumentId;}
    char *GetTransferPacket();
    int GetTransferLength();
    ~CHttpPacketMessage()
    {
       if(m_pTransferPacket != NULL){
            delete[] m_pTransferPacket;
            m_pTransferPacket = NULL;
        }
    }
    void GetIndexReadParameter(uint32_t& iPageInex, uint32_t& iPageSize , uint32_t& i_SortType);
private:
    bool m_isPost;
    std::string m_strPath;
    std::string m_strBody;
    int m_iHttpVersion;
    bool m_isKeepAlive;
    unsigned int m_iContentLen;
    std::string m_strCookie;
    std::string m_strXForwardedFor;
    bool m_isXForwardedFor;
    std::string m_strHost;
    int m_xffport;
    std::string m_strContentType;
    std::map<std::string, std::string> m_mapHeaders;
    string m_strDocumentId;
    char *m_pTransferPacket;
    uint32_t m_iOwnerMessageId;
    uint32_t m_iPageIndex;
    uint32_t m_iPageSize;
    uint32_t m_iSortType;
private:
    void ParseHeaders(const char *pHeaders, unsigned int iLen);
    char *FindLastString(char *pBegin, char *pEnd, const char *pSub);
    
};

#endif
