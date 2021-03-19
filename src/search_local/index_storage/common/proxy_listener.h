/*
 * =====================================================================================
 *
 *       Filename:  proxy_listener.h
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
#ifndef __AGENT_LISTENER_H__
#define __AGENT_LISTENER_H__

#include "poller.h"
#include "net_addr.h"

class PollThread;
class AgentClientUnit;
class AgentListener : public PollerObject
{
private:
	PollThread *ownerThread;
	AgentClientUnit *out;
	const SocketAddress addr;
	virtual void input_notify();

public:
	AgentListener(PollThread *t, AgentClientUnit *o, SocketAddress &a);
	virtual ~AgentListener();

	int Bind(int blog);
	int attach_thread();
};

#endif
