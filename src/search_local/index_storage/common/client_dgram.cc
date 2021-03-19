/*
 * =====================================================================================
 *
 *       Filename:  client_dgram.cc
 *
 *    Description:  client dgram.
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
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "client_dgram.h"
#include "task_request.h"
#include "client_unit.h"
#include "protocol.h"
#include "log.h"

class PollThread;
static int GetSocketFamily(int fd)
{
	struct sockaddr addr;
	bzero(&addr, sizeof(addr));
	socklen_t alen = sizeof(addr);
	getsockname(fd, &addr, &alen);
	return addr.sa_family;
}

class ReplyDgram : public ReplyDispatcher<TaskRequest>
{
public:
	ReplyDgram(void) {}
	virtual ~ReplyDgram(void);
	virtual void reply_notify(TaskRequest *task);
};

ReplyDgram::~ReplyDgram(void) {}

void ReplyDgram::reply_notify(TaskRequest *task)
{
	DgramInfo *info = task->OwnerInfo<DgramInfo>();
	if (info == NULL)
	{
		delete task;
	}
	else if (info->cli == NULL)
	{
		log_error("info->cli is NULL, possible memory corrupted");
		FREE(info);
		delete task;
	}
	else
	{
		info->cli->send_result(task, info->addr, info->len);
		FREE(info);
	}
}

static ReplyDgram replyDgram;

ClientDgram::ClientDgram(DTCDecoderUnit *o, int fd)
	: PollerObject(o->owner_thread(), fd),
	  owner(o),
	  hastrunc(0),
	  mru(0),
	  mtu(0),
	  alen(0),
	  abuf(NULL)
{
}

ClientDgram::~ClientDgram()
{
	FREE_IF(abuf);
}

int ClientDgram::init_socket_info(void)
{
	switch (GetSocketFamily(netfd))
	{
	default:
		mru = 65508;
		mtu = 65507;
		alen = sizeof(struct sockaddr);
		break;
	case AF_UNIX:
		mru = 16 << 20;
		mtu = 16 << 20;
		alen = sizeof(struct sockaddr_un);
		break;
	case AF_INET:
		hastrunc = 1;
		mru = 65508;
		mtu = 65507;
		alen = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		hastrunc = 1;
		mru = 65508;
		mtu = 65507;
		alen = sizeof(struct sockaddr_in6);
		break;
	}
	return 0;
}

int ClientDgram::allocate_dgram_info(void)
{
	if (abuf != NULL)
		return 0;

	abuf = (DgramInfo *)MALLOC(sizeof(DgramInfo) + alen);
	if (abuf == NULL)
		return -1;

	abuf->cli = this;
	return 0;
}

int ClientDgram::Attach()
{
	init_socket_info();

	enable_input();
	if (attach_poller() == -1)
		return -1;

	return 0;
}

// server peer
// return value: -1, fatal error or no more packets
// return value: 0, more packet may be pending
int ClientDgram::recv_request(int noempty)
{
	if (allocate_dgram_info() < 0)
	{
		log_error("%s", "create DgramInfo object failed, msg[no enough memory]");
		return -1;
	}

	int dataLen;
	if (hastrunc)
	{
		char dummy[1];
		abuf->len = alen;
		dataLen = recvfrom(netfd, dummy, 1, MSG_DONTWAIT | MSG_TRUNC | MSG_PEEK, (sockaddr *)abuf->addr, &abuf->len);
		if (dataLen < 0)
		{
			// NO ERROR, and assume NO packet pending
			return -1;
		}
	}
	else
	{
		dataLen = -1;
		ioctl(netfd, SIOCINQ, &dataLen);
		if (dataLen < 0)
		{
			log_error("%s", "next packet size unknown");
			return -1;
		}

		if (dataLen == 0)
		{
			if (noempty)
			{
				// NO ERROR, and assume NO packet pending
				return -1;
			}
			/* treat 0 as max packet, because we can't ditiguish the no-packet & zero-packet */
			dataLen = mru;
		}
	}

	char *buf = (char *)MALLOC(dataLen);
	if (buf == NULL)
	{
		log_error("allocate packet buffer[%d] failed, msg[no enough memory]", dataLen);
		return -1;
	}

	abuf->len = alen;
	dataLen = recvfrom(netfd, buf, dataLen, MSG_DONTWAIT, (sockaddr *)abuf->addr, &abuf->len);
	if (abuf->len <= 1)
	{
		log_notice("recvfrom error: no source address");
		free(buf);
		// more packet pending
		return 0;
	}

	if (dataLen <= (int)sizeof(PacketHeader))
	{
		int err = dataLen == -1 ? errno : 0;
		if (err != EAGAIN)
			log_notice("recvfrom error: size=%d errno=%d", dataLen, err);
		free(buf);
		// more packet pending
		return 0;
	}

	TaskRequest *task = new TaskRequest(owner->owner_table());
	if (NULL == task)
	{
		log_error("%s", "create TaskRequest object failed, msg[no enough memory]");
		return -1;
	}

	task->set_hotbackup_table(owner->admin_table());

	int ret = task->Decode(buf, dataLen, 1 /*eat*/);
	switch (ret)
	{
	default:
	case DecodeFatalError:
		if (errno != 0)
			log_notice("decode fatal error, ret = %d msg = %m", ret);
		free(buf); // buf not eatten
		break;

	case DecodeDataError:
		task->response_timer_start();
		if (task->result_code() < 0)
			log_notice("DecodeDataError, role=%d, fd=%d, result=%d",
					   task->Role(), netfd, task->result_code());
		send_result(task, (void *)abuf->addr, abuf->len);
		break;

	case DecodeDone:
		if (task->prepare_process() < 0)
		{
			send_result(task, (void *)abuf->addr, abuf->len);
			break;
		}

		task->set_owner_info(abuf, 0, (sockaddr *)abuf->addr);
		abuf = NULL; // eat abuf

		task->push_reply_dispatcher(&replyDgram);
		owner->task_dispatcher(task);
	}
	return 0;
}

int ClientDgram::send_result(TaskRequest *task, void *addr, int len)
{
	if (task == NULL)
		return 0;

	owner->record_request_time(task);
	Packet *reply = new Packet;
	if (reply == NULL)
	{
		delete task;
		log_error("create Packet object failed");
		return 0;
	}

	task->versionInfo.set_keep_alive_timeout(owner->idle_time());
	reply->encode_result(task, mtu);
	delete task;

	int ret = reply->send_to(netfd, (struct sockaddr *)addr, len);

	delete reply;

	if (ret != SendResultDone)
	{
		log_notice("send failed, return = %d, error = %m", ret);
		return 0;
	}
	return 0;
}

void ClientDgram::input_notify(void)
{
	const int batchsize = 64;
	for (int i = 0; i < batchsize; ++i)
	{
		if (recv_request(i) < 0)
			break;
	}
}
