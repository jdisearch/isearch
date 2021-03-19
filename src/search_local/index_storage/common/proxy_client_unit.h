/*
 * =====================================================================================
 *
 *       Filename:  proxy_client_unit.h
 *
 *    Description:  agent client unit.
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
#ifndef __AGENT_CLIENT_UNIT_H__
#define __AGENT_CLIENT_UNIT_H__

#include "request_base_all.h"
#include "stat_dtc.h"

class TimerList;
class PollThread;
class TaskRequest;
class AgentClientUnit
{
public:
	AgentClientUnit(PollThread *t, int c);
	virtual ~AgentClientUnit();

	inline TimerList *get_timer_list() { return tlist; }
	void bind_dispatcher(TaskDispatcher<TaskRequest> *proc);
	inline void task_notify(TaskRequest *req) { output.task_notify(req); }
	void record_request_time(int hit, int type, unsigned int usec);
	void record_request_time(TaskRequest *req);

private:
	PollThread *ownerThread;
	RequestOutput<TaskRequest> output;
	int check;
	TimerList *tlist;
	StatSample statRequestTime[8];
};

#endif
