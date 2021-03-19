/*
 * =====================================================================================
 *
 *       Filename:  helper_cient.h
 *
 *    Description:  
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
#ifndef __HELPER_CLIENT_H__
#define __HELPER_CLIENT_H__

#include "poller.h"
#include "packet.h"
#include "timer_list.h"
#include "task_request.h"
#include "stop_watch.h"

enum HelperState
{
	HelperDisconnected = 0,
	HelperConnecting,
	HelperIdleState,
	HelperRecvRepState, //wait for recv response, client side
	HelperSendReqState, //wait for send request, client side
	HelperSendVerifyState,
	HelperRecvVerifyState,
	HelperSendNotifyReloadConfigState,
	HelperRecvNotifyReloadConfigState,
};

class HelperGroup;

/*
  State Machine: HelperClient object is static, beyond reconnect
 	Disconnected
		wait retryTimeout --> trying reconnect
 	Reconnect
		If connected --> IdleState
        	If inprogress --> ConnectingState
	ConnectingState
		If hangup_notify --> Disconnected
		If output_notify --> SendVerifyState
		If timer_notify:connTimeout --> Disconnected
	SendVerifyState
		If hangup_notify --> Disconnected
		If output_notify --> Trying Sending
		If timer_notify:connTimeout --> Disconnected
	RecvVerifyStat
		If hangup_notify --> complete_task(error) -->Reconnect
		If input_notify --> DecodeDone --> IdleState
	IdleState
		If hangup_notify --> Reconnect
		If attach_task --> Trying Sending
	Trying Sending
		If Sent --> RecvRepState
		If MoreData --> SendRepState
		If SentError --> PushBackTask --> Reconnect
	SendRepState
		If hangup_notify --> PushBackTask -->Reconnect
		If output_notify --> Trying Sending
	RecvRepState
		If hangup_notify --> complete_task(error) -->Reconnect
		If input_notify --> Decode Reply
	DecodeReply
		If DecodeDone --> IdleState
		If MoreData --> RecvRepState
		If FatalError --> complete_task(error) --> Reconnect
		If DataError --> complete_task(error) --> Reconnect
	
 */
class HelperClient : public PollerObject,
					 private TimerObject
{
public:
	friend class HelperGroup;

	HelperClient(PollerUnit *, HelperGroup *hg, int id);
	virtual ~HelperClient();

	int attach_task(TaskRequest *, Packet *);

	int support_batch_key(void) const { return supportBatchKey; }

private:
	int Reset();
	int Reconnect();

	int send_verify();
	int recv_verify();

	int client_notify_helper_reload_config();
	int send_notify_helper_reload_config();
	int recv_notify_helper_reload_config();

	int Ready();
	int connect_error();

	void complete_task(void);
	void queue_back_task(void);
	void set_error(int err, const char *msg, const char *msg1)
	{
		task->set_error(err, msg, msg1);
	}
	void set_error_dup(int err, const char *msg, const char *msg1)
	{
		task->set_error_dup(err, msg, msg1);
	}

public:
	const char *state_string(void)
	{
		return this == NULL ? "NULL" : ((const char *[]){"DISC", "CONN", "IDLE", "RECV", "SEND", "SND_VER", "RECV_VER", "BAD"})[stage];
	}

private:
	virtual void input_notify(void);
	virtual void output_notify(void);
	virtual void hangup_notify(void);
	virtual void timer_notify(void);

private:
	int recv_response();
	int send_request();
	int connect_server(const char *path);

	SimpleReceiver receiver;
	TaskRequest *task;
	Packet *packet;

	TaskRequest *verify_task;
	Packet *verify_packet;

	HelperGroup *helperGroup;
	int helperIdx;

	HelperState stage;

	int supportBatchKey;
	static const unsigned int maxTryConnect = 10;
	uint64_t connectErrorCnt;
	int ready;
	stopwatch_usec_t stopWatch;
};
#endif
