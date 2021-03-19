/*
 * =====================================================================================
 *
 *       Filename:  proxy_client.h
 *
 *    Description:  agent client class.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __AGENT_CLIENT_H__
#define __AGENT_CLIENT_H__

#include <pthread.h>

#include "poller.h"
#include "timer_list.h"
#include "lqueue.h"
#include "value.h"
#include "proxy_receiver.h"

class Packet;
class AgentResultQueue
{
public:
	LinkQueue<Packet *> packet;

	AgentResultQueue() {}
	~AgentResultQueue();

	inline void Push(Packet *pkt) { packet.Push(pkt); }
	inline Packet *Pop() { return packet.Pop(); }
	inline Packet *Front() { return packet.Front(); }
	inline bool queue_empty() { return packet.queue_empty(); }
};

class PollThread;
class AgentClientUnit;
class AgentReceiver;
class AgentSender;
class AgentMultiRequest;
class AgentMultiRequest;
class TaskRequest;
class ClientAgent : public PollerObject, public TimerObject
{
public:
	ClientAgent(PollThread *o, AgentClientUnit *u, int fd);
	virtual ~ClientAgent();

	int attach_thread();
	inline void add_packet(Packet *p) { resQueue.Push(p); }
	void remember_request(AgentMultiRequest *agentrequest);
	int send_result();
	void record_request_process_time(TaskRequest *task);

	virtual void input_notify();
	virtual void output_notify();

private:
	PollThread *ownerThread;
	AgentClientUnit *owner;
	TimerList *tlist;

	AgentReceiver *receiver;
	AgentSender *sender;
	AgentResultQueue resQueue;
	ListObject<AgentMultiRequest> rememberReqHeader;

	TaskRequest *prepare_request(char *recvbuff, int recvlen, int pktcnt);
	int recv_request();
	void remember_request(TaskRequest *request);
};

#endif
