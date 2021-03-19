/*
 * =====================================================================================
 *
 *       Filename:  helper_cient.cc
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
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <alloca.h>
#include <stdlib.h>

#include "log.h"
#include "helper_client.h"
#include "helper_group.h"
#include "unix_socket.h"
#include "table_def_manager.h"

HelperClient::HelperClient(PollerUnit *o, HelperGroup *hg, int idx) : PollerObject(o)
{
	packet = NULL;
	task = NULL;

	helperGroup = hg;
	helperIdx = idx;

	supportBatchKey = 0;
	connectErrorCnt = 0;
	ready = 0;
	Ready(); // 开始默认可用
}

HelperClient::~HelperClient()
{
	if ((0 != task))
	{
		if (stage == HelperRecvVerifyState)
		{
			DELETE(task);
		}
		else if (stage != HelperRecvRepState)
		{
			queue_back_task();
		}
		else
		{
			if (task->result_code() >= 0)
				set_error(-EC_UPSTREAM_ERROR, __FUNCTION__,
						 "Server Shutdown");
			task->reply_notify();
			task = NULL;
		}
	}

	DELETE(packet);
}

int HelperClient::Ready()
{
	if (ready == 0)
	{
		helperGroup->add_ready_helper();
	}

	ready = 1;
	connectErrorCnt = 0;

	return 0;
}

int HelperClient::connect_error()
{
	connectErrorCnt++;
	if (connectErrorCnt > maxTryConnect && ready)
	{
		log_debug("helper-client[%d] try connect %lu times, switch invalid.",
				  helperIdx,
				  (unsigned long)connectErrorCnt);
		helperGroup->dec_ready_helper();
		ready = 0;
	}

	return 0;
}

int HelperClient::attach_task(TaskRequest *p, Packet *s)
{
	log_debug("HelperClient::attach_task()");

	task = p;
	packet = s;

	int ret = packet->Send(netfd);
	if (ret == SendResultDone)
	{
		DELETE(packet);
		stopWatch.start();
		task->prepare_decode_reply();
		receiver.attach(netfd);
		receiver.erase();

		stage = HelperRecvRepState;
		enable_input();
	}
	else
	{
		stage = HelperSendReqState;
		enable_output();
	}

	attach_timer(helperGroup->recvList);
	return delay_apply_events();
}

void HelperClient::complete_task(void)
{
	DELETE(packet);
	if (task != NULL)
	{
		task->reply_notify();
		task = NULL;
	}
}

void HelperClient::queue_back_task(void)
{
	DELETE(packet);
	helperGroup->queue_back_task(task);
	task = NULL;
}

int HelperClient::Reset()
{
	if (stage == HelperSendVerifyState || stage == HelperRecvVerifyState)
	{
		DELETE(packet);
		DELETE(task);
	}
	else
	{
		if (task != NULL && task->result_code() >= 0)
		{
			if (stage == HelperRecvRepState)
				set_error(-EC_UPSTREAM_ERROR,
						 "HelperGroup::Reset", "helper recv error");
			else if (stage == HelperSendReqState)
				set_error(-EC_SERVER_ERROR,
						 "HelperGroup::Reset", "helper send error");
		}
		complete_task();
	}

	if (stage == HelperIdleState)
		helperGroup->connection_reset(this);

	DisableInput();
	disable_output();
	PollerObject::detach_poller();
	if (netfd > 0)
		close(netfd);
	netfd = -1;
	stage = HelperDisconnected;
	attach_timer(helperGroup->retryList);
	return 0;
}

int HelperClient::connect_server(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return -1;

	if (is_unix_socket_path(path))
	{
		if ((netfd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1)
		{
			log_error("%s", "socket error,%m");
			return -2;
		}

		fcntl(netfd, F_SETFL, O_RDWR | O_NONBLOCK);

		struct sockaddr_un unaddr;
		socklen_t addrlen;
		addrlen = init_unix_socket_address(&unaddr, path);
		return connect(netfd, (struct sockaddr *)&unaddr, addrlen);
	}
	else
	{
		const char *addr = NULL;
		const char *port = NULL;
		const char *begin = strchr(path, ':');
		if (begin)
		{
			char *p = (char *)alloca(begin - path + 1);
			memcpy(p, path, begin - path);
			p[begin - path] = '\0';
			addr = p;
		}
		else
		{
			log_error("address error,correct address is addr:port/protocol");
			return -5;
		}

		const char *end = strchr(path, '/');
		if (begin && end)
		{
			char *p = (char *)alloca(end - begin);
			memcpy(p, begin + 1, end - begin - 1);
			p[end - begin - 1] = '\0';
			port = p;
		}
		else
		{
			log_error("protocol error,correct address is addr:port/protocol");
			return -6;
		}

		struct sockaddr_in inaddr;
		bzero(&inaddr, sizeof(struct sockaddr_in));
		inaddr.sin_family = AF_INET;
		inaddr.sin_port = htons(atoi(port));

		if (strcmp(addr, "*") != 0 &&
			inet_pton(AF_INET, addr, &inaddr.sin_addr) <= 0)
		{
			log_error("invalid address %s:%s", addr, port);
			return -3;
		}

		if (strcasestr(path, "tcp"))
			netfd = socket(AF_INET, SOCK_STREAM, 0);
		else
			netfd = socket(AF_INET, SOCK_DGRAM, 0);

		if (netfd < 0)
		{
			log_error("%s", "socket error,%m");
			return -4;
		}

		fcntl(netfd, F_SETFL, O_RDWR | O_NONBLOCK);

		return connect(netfd, (const struct sockaddr *)&inaddr, sizeof(inaddr));
	}
	return 0;
}

int HelperClient::Reconnect(void)
{
	// increase connect count
	connect_error();

	if (stage != HelperDisconnected)
		Reset();

	const char *sockpath = helperGroup->sock_path();
	if (connect_server(sockpath) == 0)
	{
		log_debug("Connected to helper[%d]: %s", helperIdx, sockpath);

		packet = new Packet;
		packet->encode_detect(TableDefinitionManager::Instance()->get_cur_table_def());

		if (attach_poller() != 0)
		{
			log_error("helper[%d] attach poller error", helperIdx);
			return -1;
		}
		DisableInput();
		enable_output();
		stage = HelperSendVerifyState;
		return send_verify();
	}

	if (errno != EINPROGRESS)
	{
		log_error("connect helper-%s error: %m", sockpath);
		close(netfd);
		netfd = -1;
		attach_timer(helperGroup->retryList);
		//check helpergroup task queue expire.
		helperGroup->check_queue_expire();
		return 0;
	}

	log_debug("Connectting to helper[%d]: %s", helperIdx, sockpath);

	DisableInput();
	enable_output();
	attach_timer(helperGroup->connList);
	stage = HelperConnecting;
	return attach_poller();
}

int HelperClient::send_verify()
{
	int ret = packet->Send(netfd);
	if (ret == SendResultDone)
	{
		DELETE(packet);

		task = new TaskRequest(TableDefinitionManager::Instance()->get_cur_table_def());
		if (task == NULL)
		{
			log_error("%s: %m", "new task & packet error");
			return -1;
		}
		task->prepare_decode_reply();
		receiver.attach(netfd);
		receiver.erase();

		stage = HelperRecvVerifyState;
		disable_output();
		enable_input();
	}
	else
	{
		stage = HelperSendVerifyState;
		enable_output();
	}

	attach_timer(helperGroup->recvList);
	return delay_apply_events();
}

int HelperClient::recv_verify()
{
	static int logwarn;
	int ret = task->Decode(receiver);

	supportBatchKey = 0;
	switch (ret)
	{
	default:
	case DecodeFatalError:
		log_error("decode fatal error retcode[%d] msg[%m] from helper", ret);
		goto ERROR_RETURN;

	case DecodeDataError:
		log_error("decode data error from helper %d", task->result_code());
		goto ERROR_RETURN;

	case DecodeWaitData:
	case DecodeIdle:
		attach_timer(helperGroup->recvList);
		return 0;

	case DecodeDone:
		switch (task->result_code())
		{
		case -EC_EXTRA_SECTION_DATA:
			supportBatchKey = 1;
			break;
		case -EC_BAD_FIELD_NAME: // old version dtc
			supportBatchKey = 0;
			break;
		default:
			log_error("detect helper-%s error: %d, %s",
					  helperGroup->sock_path(),
					  task->result_code(),
					  task->resultInfo.error_message());
			goto ERROR_RETURN;
		}
		break;
	}

	if (supportBatchKey)
	{
		log_debug("helper-%s support batch-key", helperGroup->sock_path());
	}
	else
	{
		if (logwarn++ == 0)
			log_warning("helper-%s unsupported batch-key", helperGroup->sock_path());
		else
			log_debug("helper-%s unsupported batch-key", helperGroup->sock_path());
	}

	DELETE(task);
	Ready();

	enable_input();
	disable_output();
	stage = HelperIdleState;
	helperGroup->request_completed(this);
	disable_timer();
	return delay_apply_events();

ERROR_RETURN:
	Reset();
	attach_timer(helperGroup->retryList);
	//check helpergroup task queue expire.
	helperGroup->check_queue_expire();
	return 0;
}

//client peer
int HelperClient::recv_response()
{
	int ret = task->Decode(receiver);

	switch (ret)
	{
	default:
	case DecodeFatalError:
		log_notice("decode fatal error retcode[%d] msg[%m] from helper", ret);
		task->set_error(-EC_UPSTREAM_ERROR, __FUNCTION__, "decode fatal error from helper");
		break;

	case DecodeDataError:
		log_notice("decode data error from helper %d", task->result_code());
		task->set_error(-EC_UPSTREAM_ERROR, __FUNCTION__, "decode data error from helper");
		break;

	case DecodeWaitData:
	case DecodeIdle:
		attach_timer(helperGroup->recvList);
		return 0;

	case DecodeDone:
		break;
	}

	stopWatch.stop();
	helperGroup->record_process_time(task->request_code(), stopWatch);
	complete_task();
	helperGroup->request_completed(this);

	// ??
	enable_input();
	stage = HelperIdleState;
	if (ret != DecodeDone)
		return -1;
	return 0;
}

int HelperClient::send_request()
{
	int ret = packet->Send(netfd);

	log_debug("[HelperClient][task=%d]Send Request result=%d, fd=%d", task->Role(), ret, netfd);

	switch (ret)
	{
	case SendResultMoreData:
		break;

	case SendResultDone:
		DELETE(packet);
		stopWatch.start();
		task->prepare_decode_reply();
		receiver.attach(netfd);
		receiver.erase();

		stage = HelperRecvRepState;
		disable_output();
		enable_input();
		break;

	case SendResultError:
	default:
		log_notice("send result error, ret = %d msg = %m", ret);
		task->set_error(-EC_SERVER_ERROR, "Data source send failed", NULL);
		return -1;
	}

	attach_timer(helperGroup->recvList);
	return 0;
}

void HelperClient::input_notify(void)
{
	disable_timer();

	if (stage == HelperRecvVerifyState)
	{
		if (recv_verify() < 0)
			Reconnect();
		return;
	}
	else if (stage == HelperRecvNotifyReloadConfigState)
	{
		if (recv_notify_helper_reload_config() < 0)
			Reconnect();
		return;
	}
	else if (stage == HelperRecvRepState)
	{
		if (recv_response() < 0)
			Reconnect();
		return;
	}
	else if (stage == HelperIdleState)
	{
		/* no data from peer allowed in idle state */
		Reset();
		return;
	}
	DisableInput();
}

void HelperClient::output_notify(void)
{
	disable_timer();
	if (stage == HelperSendVerifyState)
	{
		if (send_verify() < 0)
		{
			DELETE(packet);
			Reconnect();
		}
		return;
	}
	else if (stage == HelperSendNotifyReloadConfigState)
	{
		if (send_notify_helper_reload_config() < 0)
		{
			DELETE(packet);
			Reconnect();
		}
		return;
	}
	else if (stage == HelperSendReqState)
	{
		if (send_request() < 0)
		{
			queue_back_task();
			Reconnect();
		}
		return;
	}
	else if (stage == HelperConnecting)
	{
		packet = new Packet;
		packet->encode_detect(TableDefinitionManager::Instance()->get_cur_table_def());

		DisableInput();
		enable_output();
		stage = HelperSendVerifyState;
		send_verify();
		return;
	}
	disable_output();
}

void HelperClient::hangup_notify(void)
{
#if 0
	if(stage!=HelperConnecting)
		Reconnect();
	else
#endif
	Reset();
}

void HelperClient::timer_notify(void)
{
	switch (stage)
	{
	case HelperRecvRepState:
		stopWatch.stop();
		helperGroup->record_process_time(task->request_code(), stopWatch);

		log_error("helper index[%d] execute timeout.", helperIdx);
		set_error(-EC_UPSTREAM_ERROR, "HelperGroup::Timeout", "helper execute timeout");
		Reconnect();
		break;
	case HelperSendReqState:

		log_error("helper index[%d] send timeout.", helperIdx);
		set_error(-EC_SERVER_ERROR, "HelperGroup::Timeout", "helper send timeout");
		Reconnect();
		break;
	case HelperDisconnected:
		Reconnect();
		break;
	case HelperConnecting:
		Reset();
		break;
	case HelperSendVerifyState:
	case HelperRecvVerifyState:
		DELETE(packet);
		DELETE(task);
		Reconnect();
		break;
	case HelperSendNotifyReloadConfigState:
	case HelperRecvNotifyReloadConfigState:
		DELETE(packet);
		DELETE(task);
		Reconnect();
		break;
	default:
		break;
	}
}

int HelperClient::client_notify_helper_reload_config()
{
	packet = new Packet;
	packet->encode_reload_config(TableDefinitionManager::Instance()->get_cur_table_def());
	if (0 != attach_poller())
	{
		log_error("notify reload config helper [%d] attach poller failed!", helperIdx);
		DELETE(packet);
		return -1;
	}
	DisableInput();
	enable_output();
	stage = HelperSendNotifyReloadConfigState;
	return send_notify_helper_reload_config();
}

int HelperClient::send_notify_helper_reload_config()
{
	int ret = packet->Send(netfd);
	if (SendResultDone == ret)
	{
		DELETE(packet);
		task = new TaskRequest(TableDefinitionManager::Instance()->get_cur_table_def());
		if (NULL == task)
		{
			log_error("new task error, maybe not have enough memory!");
			return -1;
		}
		task->prepare_decode_reply();
		receiver.attach(netfd);
		receiver.erase();

		stage = HelperRecvNotifyReloadConfigState;
		disable_output();
		enable_input();
	}
	else
	{
		stage = HelperSendNotifyReloadConfigState;
		enable_output();
	}

	attach_timer(helperGroup->recvList);
	return delay_apply_events();
}

int HelperClient::recv_notify_helper_reload_config()
{
	int ret = task->Decode(receiver);
	switch (ret)
	{
	default:
	case DecodeFatalError:
	{
		log_error("decode fatal error retcode [%d] from helper", ret);
		goto ERROR_RETURN;
	}
	case DecodeDataError:
	{
		log_error("decode data error retcode [%d] from helper", ret);
		goto ERROR_RETURN;
	}
	case DecodeWaitData:
	case DecodeIdle:
	{
		attach_timer(helperGroup->recvList);
		return 0;
	}
	case DecodeDone:
	{
		switch (task->result_code())
		{
		case 0:
			break;
		case -EC_RELOAD_CONFIG_FAILED:
		{
			log_error("reload config failed EC_RELOAD_CONFIG_FAILED resultcode [%d] from helper", task->result_code());
			goto ERROR_RETURN;
		}
		default:
		{
			log_error("reload config failed unknow resultcode [%d] from helper", task->result_code());
			goto ERROR_RETURN;
		}
		}
	}
	}
	DELETE(task);

	enable_input();
	disable_output();
	stage = HelperIdleState;
	helperGroup->request_completed(this);
	disable_timer();
	return delay_apply_events();

ERROR_RETURN:
	Reset();
	attach_timer(helperGroup->retryList);
	//check helpergroup task queue expire.
	helperGroup->check_queue_expire();
	return 0;
}
