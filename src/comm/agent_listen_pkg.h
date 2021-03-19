/*
 * =====================================================================================
 *
 *       Filename:  agent_listen_pkg.h
 *
 *    Description:  agent listen pkg class definition.
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

#ifndef __AGENT_LISTEN_PKG_H__
#define __AGENT_LISTEN_PKG_H__

#include "request_base.h"
#include "sockaddr.h"

#define MAX_AGENT_LISTENER 1

class CAgentClientUnit;
class CAgentListener;
class CPollThread;
class CConfig;
class CTaskRequest;
class CAgentListenPkg
{
    private:
	CSocketAddress addr;
	CPollThread * thread;
	CAgentClientUnit * out;
	CAgentListener * listener;

    public:
	CAgentListenPkg();
	~CAgentListenPkg();

	//two methods below add by liwujun--2020-06-17
	int GetListenFd();
	void CloseListenFd();

	int Bind(const char *bindaddr, CTaskDispatcher<CTaskRequest> *out, int32_t listen_fd);
//	int Bind(char *bindaddr, CTaskDispatcher<CTaskRequest> *out, CPollThread *workThread);
	int Run();
	int Match(const char *host, const char *port);
	CPollThread *GetThread()const{	return thread;	}
};

#endif
