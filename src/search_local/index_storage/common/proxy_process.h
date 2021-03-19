/*
 * =====================================================================================
 *
 *       Filename:  proxy_process.h
 *
 *    Description:  agent task process.
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
#ifndef __AGENT_PROCESS_H__
#define __AGENT_PROCESS_H__

#include "request_base.h"

class PollThread;
class TaskRequest;

class AgentProcess : public TaskDispatcher<TaskRequest>
{
private:
	PollThread *ownerThread;
	RequestOutput<TaskRequest> output;

public:
	AgentProcess(PollThread *o);
	virtual ~AgentProcess();

	inline void bind_dispatcher(TaskDispatcher<TaskRequest> *p)
	{
		output.bind_dispatcher(p);
	}
	virtual void task_notify(TaskRequest *curr);
};

#endif
