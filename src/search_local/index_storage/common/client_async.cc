/*
 * =====================================================================================
 *
 *       Filename:  client_async.cc
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
#include <stdio.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "client_async.h"
#include "client_unit.h"
#include "poll_thread.h"
#include "log.h"

static int statEnable = 0;
static StatItemU32 statAcceptCount;
static StatItemU32 statCurConnCount;

inline AsyncInfo::AsyncInfo(ClientAsync *c, TaskRequest *r)
	: cli(c),
	  req(r),
	  pkt(NULL)

{
	c->numReq++;
	/* move to waiting list */
	ListAddTail(&c->waitList);
}

inline AsyncInfo::~AsyncInfo()
{
	if (req)
		req->clear_owner_info();
	DELETE(pkt);
	if (cli)
		cli->numReq--;
}

class CReplyAsync : public ReplyDispatcher<TaskRequest>
{
public:
	CReplyAsync(void) {}
	virtual ~CReplyAsync(void);
	virtual void reply_notify(TaskRequest *task);
};

CReplyAsync::~CReplyAsync(void) {}

void CReplyAsync::reply_notify(TaskRequest *task)
{
	AsyncInfo *info = task->OwnerInfo<AsyncInfo>();
	if (info == NULL)
	{
		delete task;
	}
	else if (info->cli == NULL)
	{
		log_error("info->cli is NULL, possible memory corrupted");
		delete task;
	}
	else
	{
		info->cli->queue_result(info);
	}
}

static CReplyAsync replyAsync;

ClientAsync::ClientAsync(DTCDecoderUnit *o, int fd, int m, void *peer, int ps)
	: PollerObject(o->owner_thread(), fd),
	  owner(o),
	  receiver(fd),
	  curReq(NULL),
	  curRes(NULL),
	  maxReq(m),
	  numReq(0)
{
	addrLen = ps;
	addr = MALLOC(ps);
	memcpy((char *)addr, (char *)peer, ps);

	if (!statEnable)
	{
		statAcceptCount = statmgr.get_item_u32(ACCEPT_COUNT);
		statCurConnCount = statmgr.get_item_u32(CONN_COUNT);
		statEnable = 1;
	}
	++statAcceptCount;
	++statCurConnCount;
}

ClientAsync::~ClientAsync()
{
	while (!doneList.ListEmpty())
	{
		AsyncInfo *info = doneList.NextOwner();
		delete info;
	}
	while (!waitList.ListEmpty())
	{
		AsyncInfo *info = waitList.NextOwner();
		delete info;
	}
	--statCurConnCount;

	FREE_IF(addr);
}

int ClientAsync::Attach()
{
	enable_input();
	if (attach_poller() == -1)
		return -1;

	return 0;
}

//server peer
int ClientAsync::recv_request()
{
	if (curReq == NULL)
	{
		curReq = new TaskRequest(owner->owner_table());
		if (NULL == curReq)
		{
			log_error("%s", "create TaskRequest object failed, msg[no enough memory]");
			return -1;
		}
		curReq->set_hotbackup_table(owner->admin_table());
		receiver.erase();
	}

	int ret = curReq->Decode(receiver);
	switch (ret)
	{
	default:
	case DecodeFatalError:
		if (errno != 0)
			log_notice("decode fatal error, ret = %d msg = %m", ret);
		DELETE(curReq);
		return -1;

	case DecodeDataError:
		curReq->response_timer_start();
		curReq->mark_as_hit();
		if (curReq->result_code() < 0)
			log_notice("DecodeDataError, role=%d, fd=%d, result=%d",
					   curReq->Role(), netfd, curReq->result_code());
		return queue_error();

	case DecodeDone:
		if (curReq->prepare_process() < 0)
			return queue_error();
		curReq->set_owner_info(new AsyncInfo(this, curReq), 0, (struct sockaddr *)addr);
		curReq->push_reply_dispatcher(&replyAsync);
		owner->task_dispatcher(curReq);
		curReq = NULL;
	}
	return 0;
}

int ClientAsync::queue_error(void)
{
	AsyncInfo *info = new AsyncInfo(this, curReq);
	curReq = NULL;
	return queue_result(info);
}

int ClientAsync::queue_result(AsyncInfo *info)
{
	if (info->req == NULL)
	{
		delete info;
		return 0;
	}

	TaskRequest *const cur = info->req;
	owner->record_request_time(curReq);

	/* move to sending list */
	info->list_move_tail(&doneList);
	/* convert request to result */
	info->pkt = new Packet;
	if (info->pkt == NULL)
	{
		delete info->req;
		info->req = NULL;
		delete info;
		log_error("create Packet object failed");
		return 0;
	}

	cur->versionInfo.set_keep_alive_timeout(owner->idle_time());
	info->pkt->encode_result(info->req);
	DELETE(info->req);

	Response();
	adjust_events();
	return 0;
}

int ClientAsync::response_one(void)
{
	if (curRes == NULL)
		return 0;

	int ret = curRes->Send(netfd);

	switch (ret)
	{
	case SendResultMoreData:
		return 0;

	case SendResultDone:
		DELETE(curRes);
		numReq--;
		return 0;

	default:
		log_notice("send failed, return = %d, error = %m", ret);
		return -1;
	}
	return 0;
}

int ClientAsync::Response(void)
{
	do
	{
		int ret;
		if (curRes == NULL)
		{
			// All result sent
			if (doneList.ListEmpty())
				break;

			AsyncInfo *info = doneList.NextOwner();
			curRes = info->pkt;
			info->pkt = NULL;
			delete info;
			numReq++;
			if (curRes == NULL)
				continue;
		}
		ret = response_one();
		if (ret < 0)
			return ret;
	} while (curRes == NULL);
	return 0;
}

void ClientAsync::input_notify(void)
{
	if (recv_request() < 0)
		delete this;
	else
		adjust_events();
}

void ClientAsync::output_notify(void)
{
	if (Response() < 0)
		delete this;
	else
		adjust_events();
}

int ClientAsync::adjust_events(void)
{
	if (curRes == NULL && doneList.ListEmpty())
		disable_output();
	else
		enable_output();
	if (numReq >= maxReq)
		DisableInput();
	else
		enable_input();
	return apply_events();
}
