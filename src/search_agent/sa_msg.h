#ifndef __SA_MSG_H__
#define __SA_MSG_H__

#include <stdint.h>
#include <set>
#include <queue>
#include <map>
#include <malloc.h>
#include <string.h>
#include "sa_util.h"
#include "sa_buffer.h"
#include "sa_error.h"
using namespace std;

class ConnectionInterface;
class CSearchAgentCore;
class CHttpPacketMessage;

typedef enum
{
    MSG_PARSE_OK,
    MSG_PARSE_REPAIR,
    MSG_PARSE_AGAIN,
} MsgParseResult;

typedef enum
{
    INDEX_READ_MSG,
    INDEX_WRITE_MSG,
    UNKNOWN_INDEX_MSG

} EMessageType;

struct MsgNode
{
    uint64_t key;
    uint32_t msg_id;
    void *data;
    MsgNode()
    {
    }
    MsgNode(const MsgNode& node)
   {
      this->key = node.key;
      this->msg_id =  node.msg_id ;
      this->data = node.data;
   }
    bool operator<(const MsgNode &node) const
    {
	if(this->key != node.key){
        	return this->key < node.key ;
	}
	return msg_id < node.msg_id;
    }
};

class CSearchAgentMessage
{
public:
    CSearchAgentMessage(uint64_t iMessageId, ConnectionInterface *pConnection, bool isRequest) : m_iMessageId(iMessageId),
                                                                                                 m_isRequest(isRequest),
                                                                                                 m_pOwnerConnection(pConnection)
    {
        m_iStartTimeStamp = get_now_us();
        m_isSending = false;
        m_iRecievedFragment = 0;
        m_pPeerConnectionIds.clear();
    }

    MessageBuffer *GetRecvBuffer();

    void PushBufferQueue(MessageBuffer *pBuffer);

    virtual int ParseMessage(ConnectionInterface *pConnection,
                             CSearchAgentCore *pCoreCtx) = 0;

    const string &GetResponseBody();
    void MessageFragment(unsigned int iFragmentMessageCount);

    void SetOwnerConnection(ConnectionInterface *pPeerConnection);

    uint32_t GetMessageId();
    uint32_t GetPeerMessageId();

    void SetSending(bool isSending);
    bool GetIsSending();

    char *GetTransferBuffer();
    int GetTransferLength();
    EMessageType GetMessageType();

    ConnectionInterface *GetOwnerConnection();

    void SetPeerConnection(ConnectionInterface *pPeerConnection);
    const vector<uint32_t> &GetPeerConnection();

    void IncrRecievedFragment();
    bool IsRequestDone();
    void PushBackFragmentResponse(CSearchAgentMessage *pResponseMessage);
    virtual int Coalesce() {return 0;};
    bool IsRequest() {return m_isRequest;}

    void MakeErrorMessage(SEARCH_AGENT_ERR_CODE iCode);

    virtual ~CSearchAgentMessage();

protected:
    uint32_t m_iMessageId;
    uint32_t m_iPeerMessageId;

    EMessageType m_eMessageType;

    bool m_isRequest;
    uint64_t m_iStartTimeStamp;
    queue<MessageBuffer *> m_oBufferQueue;
    MsgParseResult m_eParseResult;

    unsigned int m_iFragmentMessageCount;
    unsigned int m_iRecievedFragment;

    ConnectionInterface *m_pOwnerConnection;
    vector<uint32_t> m_pPeerConnectionIds;

    bool m_isSending;

    char *m_pTransferBuffer;
    int m_iTransferLength;

    string m_strResponseBody;
    vector<CSearchAgentMessage *> m_arrFragmentResponseMessages;

protected:
    bool IsEmpty();
    int MessageParsed(CSearchAgentCore *pCoreCtx);
    
};



#endif
