/*
 * =====================================================================================
 *
 *       Filename:  proxy_listen_pool.cc
 *
 *    Description:  agent listen pool
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
#include "proxy_listen_pool.h"
#include "proxy_listener.h"
#include "proxy_client_unit.h"
#include "poll_thread.h"
#include "config.h"
#include "task_request.h"
#include "log.h"
#include "stat_dtc.h"

AgentListenPool::AgentListenPool()
{
	memset(thread, 0, sizeof(PollThread *) * MAX_AGENT_LISTENER);
	memset(out, 0, sizeof(AgentClientUnit *) * MAX_AGENT_LISTENER);
	memset(listener, 0, sizeof(AgentListener *) * MAX_AGENT_LISTENER);

	StatItemU32 statAgentCurConnCount = statmgr.get_item_u32(AGENT_CONN_COUNT);
	statAgentCurConnCount = 0;
}

AgentListenPool::~AgentListenPool()
{
	for (int i = 0; i < MAX_AGENT_LISTENER; i++)
	{
		if (thread[i])
			thread[i]->interrupt();
		delete listener[i];
		delete out[i];
	}
}

/* part of framework construction */
int AgentListenPool::Bind(DTCConfig *gc, TaskDispatcher<TaskRequest> *agentprocess)
{
	char bindstr[64];
	const char *bindaddr;
	const char *errmsg = NULL;
	char threadname[64];
	int checktime;
	int blog;

	checktime = gc->get_int_val("cache", "AgentRcvBufCheck", 5);
	blog = gc->get_int_val("cache", "AgentListenBlog", 256);

	for (int i = 0; i < MAX_AGENT_LISTENER; i++)
	{
		if (i == 0)
			snprintf(bindstr, sizeof(bindstr), "BindAddr");
		else
			snprintf(bindstr, sizeof(bindstr), "BindAddr%d", i);

		bindaddr = gc->get_str_val("cache", bindstr);
		if (NULL == bindaddr)
			continue;

		if ((errmsg = addr[i].set_address(bindaddr, (const char *)NULL)))
			continue;

		snprintf(threadname, sizeof(threadname), "inc%d", i);
		thread[i] = new PollThread(threadname);
		if (NULL == thread[i])
		{
			log_error("no mem to new agent inc thread %d", i);
			return -1;
		}
		if (thread[i]->initialize_thread() < 0)
		{
			log_error("agent inc thread %d init error", i);
			return -1;
		}

		out[i] = new AgentClientUnit(thread[i], checktime);
		if (NULL == out[i])
		{
			log_error("no mem to new agent client unit %d", i);
			return -1;
		}
		out[i]->bind_dispatcher(agentprocess);

		listener[i] = new AgentListener(thread[i], out[i], addr[i]);
		if (NULL == listener[i])
		{
			log_error("no mem to new agent listener %d", i);
			return -1;
		}
		if (listener[i]->Bind(blog) < 0)
		{
			log_error("agent listener %d bind error", i);
			return -1;
		}

		if (listener[i]->attach_thread() < 0)
			return -1;
	}

	return 0;
}

int AgentListenPool::Bind(DTCConfig *gc, TaskDispatcher<TaskRequest> *agentprocess, PollThread *workThread)
{
	char bindstr[64];
	const char *bindaddr;
	const char *errmsg = NULL;
	int checktime;
	int blog;

	checktime = gc->get_int_val("cache", "AgentRcvBufCheck", 5);
	blog = gc->get_int_val("cache", "AgentListenBlog", 256);

	snprintf(bindstr, sizeof(bindstr), "BindAddr");

	bindaddr = gc->get_str_val("cache", bindstr);
	if (NULL == bindaddr)
	{
		log_error("get cache BindAddr configure failed");
		return -1;
	}

	if ((errmsg = addr[0].set_address(bindaddr, (const char *)NULL)))
	{
		log_error("addr[0] setaddress failed");
		return -1;
	}

	thread[0] = workThread;

	out[0] = new AgentClientUnit(thread[0], checktime);
	if (NULL == out[0])
	{
		log_error("no mem to new agent client unit");
		return -1;
	}
	out[0]->bind_dispatcher(agentprocess);

	listener[0] = new AgentListener(thread[0], out[0], addr[0]);
	if (NULL == listener[0])
	{
		log_error("no mem to new agent listener");
		return -1;
	}
	if (listener[0]->Bind(blog) < 0)
	{
		log_error("agent listener bind error");
		return -1;
	}
	if (listener[0]->attach_thread() < 0)
		return -1;

	return 0;
}

int AgentListenPool::Run()
{
	for (int i = 0; i < MAX_AGENT_LISTENER; i++)
	{
		if (thread[i])
			thread[i]->running_thread();
	}

	return 0;
}

int AgentListenPool::Match(const char *host, const char *port)
{
	for (int i = 0; i < MAX_AGENT_LISTENER; i++)
	{
		if (listener[i] == NULL)
			continue;
		if (addr[i].Match(host, port))
			return 1;
	}
	return 0;
}
