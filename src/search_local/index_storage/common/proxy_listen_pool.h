/*
 * =====================================================================================
 *
 *       Filename:  proxy_listen_pool.h
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
#ifndef __AGENT_LISTEN_POOL_H__
#define __AGENT_LISTEN_POOL_H__

#include "request_base.h"
#include "net_addr.h"

#define MAX_AGENT_LISTENER 10

class AgentClientUnit;
class AgentListener;
class PollThread;
class DTCConfig;
class TaskRequest;
class AgentListenPool
{
private:
	SocketAddress addr[MAX_AGENT_LISTENER];
	PollThread *thread[MAX_AGENT_LISTENER];
	AgentClientUnit *out[MAX_AGENT_LISTENER];
	AgentListener *listener[MAX_AGENT_LISTENER];

public:
	AgentListenPool();
	~AgentListenPool();

	int Bind(DTCConfig *gc, TaskDispatcher<TaskRequest> *out);
	int Bind(DTCConfig *gc, TaskDispatcher<TaskRequest> *out, PollThread *workThread);
	int Run();
	int Match(const char *host, const char *port);
};

#endif
