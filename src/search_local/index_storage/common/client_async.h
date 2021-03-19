/*
 * =====================================================================================
 *
 *       Filename:  client_async.h
 *
 *    Description:  client async operation.
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
#ifndef __CLIENT__ASYNC_H__
#define __CLIENT__ASYNC_H__

#include "poller.h"
#include "task_request.h"
#include "packet.h"
#include "timer_list.h"
#include "list.h"

class DTCDecoderUnit;
class ClientAsync;

class AsyncInfo : private ListObject<AsyncInfo>
{
public:
	AsyncInfo(ClientAsync *c, TaskRequest *r);
	~AsyncInfo();
	void list_move_tail(ListObject<AsyncInfo> *a)
	{
		ListObject<AsyncInfo>::list_move_tail(a);
	}

	ClientAsync *cli;
	TaskRequest *req;
	Packet *pkt;
};

class ClientAsync : public PollerObject
{
public:
	friend class AsyncInfo;
	DTCDecoderUnit *owner;

	ClientAsync(DTCDecoderUnit *, int fd, int depth, void *peer, int ps);
	virtual ~ClientAsync();

	virtual int Attach(void);
	int queue_result(AsyncInfo *);
	int queue_error(void);
	int flush_result(void);
	int adjust_events(void);

	virtual void input_notify(void);

private:
	virtual void output_notify(void);

	int recv_request(void);
	int Response(void);
	int response_one(void);

protected:
	SimpleReceiver receiver;
	TaskRequest *curReq; // decoding
	Packet *curRes;		 // sending

	ListObject<AsyncInfo> waitList;
	ListObject<AsyncInfo> doneList;

	void *addr;
	int addrLen;
	int maxReq;
	int numReq;
};

#endif
