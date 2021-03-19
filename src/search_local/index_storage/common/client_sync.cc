/*
 * =====================================================================================
 *
 *       Filename:  client_sync.cc
 *
 *    Description:  client sync.
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
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "client_sync.h"
#include "task_request.h"
#include "packet.h"
#include "client_resource_pool.h"
#include "client_unit.h"
#include "log.h"

#include <stat_dtc.h>

class PollThread;
static int statEnable = 0;
static StatItemU32 statAcceptCount;
static StatItemU32 statCurConnCount;

class ReplySync : public ReplyDispatcher<TaskRequest>
{
public:
	ReplySync(void) {}
	virtual ~ReplySync(void) {}
	virtual void reply_notify(TaskRequest *task);
};

void ReplySync::reply_notify(TaskRequest *task)
{
	ClientSync *cli = task->OwnerInfo<ClientSync>();
	DTCDecoderUnit *resourceOwner = (DTCDecoderUnit *)task->resourceOwner;

	/* cli not exist, clean&free resource */
	if (cli == NULL)
	{
		if (task->resourceId != 0)
		{
			resourceOwner->clean_resource(task->resourceId);
			resourceOwner->unregist_resource(task->resourceId, task->resourceSeq);
		}
	}
	/* normal case */
	else if (cli->send_result() == 0)
	{
		cli->delay_apply_events();
	}
	else /* send error, delete client, resource will be clean&free there */
	{
		// delete client apply delete task
		delete cli;
	}
}

static ReplySync syncReplyObject;

ClientSync::ClientSync(DTCDecoderUnit *o, int fd, void *peer, int ps) : PollerObject(o->owner_thread(), fd),
																		owner(o),
																		receiver(fd),
																		stage(IdleState),
																		task(NULL),
																		reply(NULL)
{

	addrLen = ps;
	addr = MALLOC(ps);
	resourceId = 0;
	resource = NULL;

	if (addr == NULL)
		throw(int) - ENOMEM;

	memcpy((char *)addr, (char *)peer, ps);

	if (!statEnable)
	{
		statAcceptCount = statmgr.get_item_u32(ACCEPT_COUNT);
		statCurConnCount = statmgr.get_item_u32(CONN_COUNT);
		statEnable = 1;
	}

	/* ClientSync deleted if allocate resource failed. clean resource allocated */
	get_resource();
	if (resource)
	{
		task = resource->task;
		reply = resource->packet;
	}
	else
	{
		throw(int) - ENOMEM;
	}
	rscStatus = RscClean;

	++statAcceptCount;
	++statCurConnCount;
}

ClientSync::~ClientSync()
{
	if (task)
	{
		if (stage == ProcReqState)
		{
			/* task in use, save resource to reply */
			task->clear_owner_info();
			task->resourceId = resourceId;
			task->resourceOwner = owner;
			task->resourceSeq = resourceSeq;
		}
		else
		{
			/* task not in use, clean resource, free resource */
			if (resource)
			{
				clean_resource();
				rscStatus = RscClean;
				free_resource();
			}
		}
	}

	//DELETE(reply);
	FREE_IF(addr);
	--statCurConnCount;
}

int ClientSync::Attach()
{
	enable_input();
	if (attach_poller() == -1)
		return -1;

	attach_timer(owner->idle_list());

	stage = IdleState;
	return 0;
}

//server peer
int ClientSync::recv_request()
{
	/* clean task from pool, basic init take place of old construction */
	if (RscClean == rscStatus)
	{
		task->set_data_table(owner->owner_table());
		task->set_hotbackup_table(owner->admin_table());
		task->set_role_as_server();
		task->BeginStage();
		receiver.erase();
		rscStatus = RscDirty;
	}

	disable_timer();

	int ret = task->Decode(receiver);
	switch (ret)
	{
	default:
	case DecodeFatalError:
		if (errno != 0)
			log_notice("decode fatal error, ret = %d msg = %m", ret);
		return -1;

	case DecodeDataError:
		task->response_timer_start();
		task->mark_as_hit();
		if (task->result_code() < 0)
			log_notice("DecodeDataError, role=%d, fd=%d, result=%d", task->Role(), netfd, task->result_code());
		return send_result();

	case DecodeIdle:
		attach_timer(owner->idle_list());
		stage = IdleState;
		rscStatus = RscClean;
		break;

	case DecodeWaitData:
		stage = RecvReqState;
		break;

	case DecodeDone:
		if ((ret = task->prepare_process()) < 0)
		{
			log_debug("build packed key error: %d", ret);
			return send_result();
		}
#if 0
        /* 处理任务时，如果client关闭连接，server也应该关闭连接, 所以依然enable_input */
        DisableInput();
#endif
		disable_output();
		task->set_owner_info(this, 0, (struct sockaddr *)addr);
		stage = ProcReqState;
		task->push_reply_dispatcher(&syncReplyObject);
		owner->task_dispatcher(task);
	}
	return 0;
}

/* keep task in resource slot */
int ClientSync::send_result(void)
{
	stage = SendRepState;

	owner->record_request_time(task);

	if (task->flag_keep_alive())
		task->versionInfo.set_keep_alive_timeout(owner->idle_time());
	reply->encode_result(task);

	return Response();
}

int ClientSync::Response(void)
{
	int ret = reply->Send(netfd);

	switch (ret)
	{
	case SendResultMoreData:
		enable_output();
		return 0;

	case SendResultDone:
		clean_resource();
		rscStatus = RscClean;
		stage = IdleState;
		disable_output();
		enable_input();
		attach_timer(owner->idle_list());
		return 0;

	default:
		log_notice("send failed, return = %d, error = %m", ret);
		return -1;
	}
	return 0;
}

void ClientSync::input_notify(void)
{
	if (stage == IdleState || stage == RecvReqState)
	{
		if (recv_request() < 0)
		{

			delete this;
			return;
		}
	}

	/* receive input events again. */
	else
	{
		/*  check whether client close connection. */
		if (check_link_status())
		{
			log_debug("client close connection, delete ClientSync obj, stage=%d", stage);
			delete this;
			return;
		}
		else
		{
			DisableInput();
		}
	}

	delay_apply_events();
}

void ClientSync::output_notify(void)
{
	if (stage == SendRepState)
	{
		if (Response() < 0)
			delete this;
	}
	else
	{
		disable_output();
		log_info("Spurious ClientSync::output_notify, stage=%d", stage);
	}
}

int ClientSync::get_resource()
{
	return owner->regist_resource(&resource, resourceId, resourceSeq);
}

void ClientSync::free_resource()
{
	task = NULL;
	reply = NULL;
	owner->unregist_resource(resourceId, resourceSeq);
}

void ClientSync::clean_resource()
{
	owner->clean_resource(resourceId);
}
