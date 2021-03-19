/*
 * =====================================================================================
 *
 *       Filename:  agent_client.h
 *
 *    Description:  agent client class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __AGENT_CLIENT_H__
#define __AGENT_CLIENT_H__

#include <pthread.h>

#include "poller.h"
#include "timerlist.h"
#include "lqueue.h"
#include "value.h"
#include "agent_receiver.h"

class CPacket;
class CAgentResultQueue
{
    public:
	CLinkQueue<CPacket *> packet;

    public:
	CAgentResultQueue() {}
	~CAgentResultQueue();

	inline void Push(CPacket * pkt) { packet.Push(pkt); }
	inline CPacket * Pop() { return packet.Pop();}
	inline CPacket * Front() { return packet.Front();} 
	inline bool QueueEmpty() { return packet.QueueEmpty(); }
};

class CPollThread;
class CAgentClientUnit;
class CAgentReceiver;
class CAgentSender;
class CAgentMultiRequest;
class CAgentMultiRequest;
class CTaskRequest;
class CClientAgent : public CPollerObject, public CTimerObject
{
    public:
	CClientAgent(CPollThread * o, CAgentClientUnit * u, int fd);
	virtual ~CClientAgent();

	int AttachThread();
	inline void AddPacket(CPacket * p) { resQueue.Push(p); }
	void RememberRequest(CAgentMultiRequest * agentrequest);
	int SendResult();
	void RecordRequestProcessTime(CTaskRequest * task);

    public:
	virtual void InputNotify();
	virtual void OutputNotify();

    private:
	CPollThread * ownerThread;
	CAgentClientUnit * owner;
	CTimerList * tlist;

	CAgentReceiver * receiver;
	CAgentSender * sender;
	CAgentResultQueue resQueue;
	CListObject<CAgentMultiRequest> rememberReqHeader;

    private:
	CTaskRequest * PrepareRequest(char * recvbuff, int recvlen, int pktcnt);
	int RecvRequest();
	void RememberRequest(CTaskRequest * request);
};

#endif
