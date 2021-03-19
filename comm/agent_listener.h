/*
 * =====================================================================================
 *
 *       Filename:  agent_listener.h
 *
 *    Description:  agent_listener class definition.
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


#ifndef __AGENT_LISTENER_H__
#define __AGENT_LISTENER_H__

#include "poller.h"
#include "sockaddr.h"

class CPollThread;
class CAgentClientUnit;
class CAgentListener : public CPollerObject
{
    private:
	CPollThread * ownerThread;
	CAgentClientUnit * out;
	const CSocketAddress addr;
	
    public:
	CAgentListener(CPollThread * t, CAgentClientUnit * o, CSocketAddress & a);
	virtual ~CAgentListener();

	int Bind(int blog);
	int AttachThread();

	//three methods below add by liwujun--2020-06-17
	int GetFd() { return netfd;}
	void CloseListenFd();
	void SetListenNetfd(int32_t fd) {netfd=fd;}
    private:
	virtual void InputNotify();
};

#endif
