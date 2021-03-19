/*
 * =====================================================================================
 *
 *       Filename:  listener_pool.cc
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
#include <stdlib.h>

#include "listener_pool.h"
#include "listener.h"
#include "client_unit.h"
#include "poll_thread.h"
#include "unix_socket.h"
#include "task_multiplexer.h"
#include "task_request.h"
#include "config.h"
#include "log.h"

// this file is obsoleted, new one is agent_listen_pool.[hc]
extern DTCTableDefinition *gTableDef[];

ListenerPool::ListenerPool(void)
{
	memset(listener, 0, sizeof(listener));
	memset(thread, 0, sizeof(thread));
	memset(decoder, 0, sizeof(decoder));
}

int ListenerPool::init_decoder(int n, int idle, TaskDispatcher<TaskRequest> *out)
{
	if (thread[n] == NULL)
	{
		char name[16];
		snprintf(name, sizeof(name), "inc%d", n);
		thread[n] = new PollThread(name);
		if (thread[n]->initialize_thread() == -1)
			return -1;
		try
		{
			decoder[n] = new DTCDecoderUnit(thread[n], gTableDef, idle);
		}
		catch (int err)
		{
			DELETE(decoder[n]);
			return -1;
		}
		if (decoder[n] == NULL)
			return -1;

		TaskMultiplexer *taskMultiplexer = new TaskMultiplexer(thread[n]);
		taskMultiplexer->bind_dispatcher(out);
		decoder[n]->bind_dispatcher(taskMultiplexer);
	}

	return 0;
}

int ListenerPool::Bind(DTCConfig *gc, TaskDispatcher<TaskRequest> *out)
{
	bool hasBindAddr = false;
	int idle = gc->get_int_val("cache", "idle_timeout", 100);
	if (idle < 0)
	{
		log_notice("idle_timeout invalid, use default value: 100");
		idle = 100;
	}
	int single = gc->get_int_val("cache", "SingleIncomingThread", 0);
	int backlog = gc->get_int_val("cache", "MaxListenCount", 256);
	int win = gc->get_int_val("cache", "MaxRequestWindow", 0);

	for (int i = 0; i < MAXLISTENERS; i++)
	{
		const char *errmsg;
		char bindStr[32];
		char bindPort[32];
		int rbufsz;
		int wbufsz;

		if (i == 0)
		{
			snprintf(bindStr, sizeof(bindStr), "BindAddr");
			snprintf(bindPort, sizeof(bindPort), "BindPort");
		}
		else
		{
			snprintf(bindStr, sizeof(bindStr), "BindAddr%d", i);
			snprintf(bindPort, sizeof(bindPort), "BindPort%d", i);
		}

		const char *addrStr = gc->get_str_val("cache", bindStr);
		if (addrStr == NULL)
			continue;
		errmsg = sockaddr[i].set_address(addrStr, gc->get_str_val("cache", bindPort));
		if (errmsg)
		{
			log_error("bad BindAddr%d/BindPort%d: %s\n", i, i, errmsg);
			continue;
		}

		int n = single ? 0 : i;
		if (sockaddr[i].socket_type() == SOCK_DGRAM)
		{ // DGRAM listener
			rbufsz = gc->get_int_val("cache", "UdpRecvBufferSize", 0);
			wbufsz = gc->get_int_val("cache", "UdpSendBufferSize", 0);
		}
		else
		{
			// STREAM socket listener
			rbufsz = wbufsz = 0;
		}

		listener[i] = new DTCListener(&sockaddr[i]);
		listener[i]->set_request_window(win);
		if (listener[i]->Bind(backlog, rbufsz, wbufsz) != 0)
		{
			if (i == 0)
			{
				log_crit("Error bind unix-socket");
				return -1;
			}
			else
			{
				continue;
			}
		}

		if (init_decoder(n, idle, out) != 0)
			return -1;
		if (listener[i]->Attach(decoder[n], backlog) < 0)
			return -1;
		hasBindAddr = true;
	}
	if (!hasBindAddr)
	{
		log_crit("Must has a BindAddr");
		return -1;
	}

	for (int i = 0; i < MAXLISTENERS; i++)
	{
		if (thread[i] == NULL)
			continue;
		thread[i]->running_thread();
	}
	return 0;
}

ListenerPool::~ListenerPool(void)
{
	for (int i = 0; i < MAXLISTENERS; i++)
	{
		if (thread[i])
		{
			thread[i]->interrupt();
			//delete thread[i];
		}
		DELETE(listener[i]);
		DELETE(decoder[i]);
	}
}

int ListenerPool::Match(const char *name, int port)
{
	for (int i = 0; i < MAXLISTENERS; i++)
	{
		if (listener[i] == NULL)
			continue;
		if (sockaddr[i].Match(name, port))
			return 1;
	}
	return 0;
}

int ListenerPool::Match(const char *name, const char *port)
{
	for (int i = 0; i < MAXLISTENERS; i++)
	{
		if (listener[i] == NULL)
			continue;
		if (sockaddr[i].Match(name, port))
			return 1;
	}
	return 0;
}
