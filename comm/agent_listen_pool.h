/*
 * =====================================================================================
 *
 *       Filename:  agent_listen_pool.h
 *
 *    Description:  agent_listen_pool class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */



#ifndef __AGENT_LISTEN_POOL_H__
#define __AGENT_LISTEN_POOL_H__

#include "request_base.h"
#include "sockaddr.h"

#define MAX_AGENT_LISTENER 1

class CAgentClientUnit;
class CAgentListener;
class CPollThread;
class CConfig;
class CTaskRequest;
class CAgentListenPool
{
    private:
	CSocketAddress addr[MAX_AGENT_LISTENER];
	CPollThread * thread[MAX_AGENT_LISTENER];
	CAgentClientUnit * out[MAX_AGENT_LISTENER];
	CAgentListener * listener[MAX_AGENT_LISTENER];

    public:
	CAgentListenPool();
	~CAgentListenPool();

	int Bind(const char *bindaddr, CTaskDispatcher<CTaskRequest> *out);
//	int Bind(char *bindaddr, CTaskDispatcher<CTaskRequest> *out, CPollThread *workThread);
	int Run();
	int Match(const char *host, const char *port);
};

#endif
