/*
 * =====================================================================================
 *
 *       Filename:  proxy_listener.cc
 *
 *    Description:  agent listener
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
#include <errno.h>

#include "proxy_listener.h"
#include "proxy_client.h"
#include "poll_thread.h"
#include "log.h"

AgentListener::AgentListener(PollThread *t, AgentClientUnit *o, SocketAddress &a) : PollerObject(t, 0),
																					ownerThread(t),
																					out(o),
																					addr(a)
{
}

AgentListener::~AgentListener()
{
}

/* part of framework construction */
int AgentListener::Bind(int blog)
{
	if ((netfd = sock_bind(&addr, blog, 0, 0, 1 /*reuse*/, 1 /*nodelay*/, 1 /*defer_accept*/)) == -1)
		return -1;
	return 0;
}

int AgentListener::attach_thread()
{
	enable_input();
	if (PollerObject::attach_poller() < 0)
	{
		log_error("agent listener attach agentInc thread error");
		return -1;
	}
	return 0;
}

void AgentListener::input_notify()
{
	while (1)
	{
		int newfd;
		struct sockaddr peer;
		socklen_t peersize = sizeof(peer);

		newfd = accept(netfd, &peer, &peersize);
		if (newfd < 0)
		{
			if (EINTR != errno && EAGAIN != errno)
				log_error("agent listener accept error, %m");
			break;
		}

		log_debug("new CAgentClient accepted!!");

		ClientAgent *client;
		try
		{
			client = new ClientAgent(ownerThread, out, newfd);
		}
		catch (int err)
		{
			return;
		}

		if (NULL == client)
		{
			log_error("no mem for new client agent");
			break;
		}

		client->attach_thread();
		client->input_notify();
	}
}
