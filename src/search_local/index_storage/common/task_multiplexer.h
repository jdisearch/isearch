/*
 * =====================================================================================
 *
 *       Filename:  task_multiplexer.h
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
#ifndef __H_TASK_MULTIPLEXER_H__
#define __H_TASK_MULTIPLEXER_H__

#include "request_base.h"
#include "multi_request.h"

class TaskRequest;

class TaskMultiplexer : public TaskDispatcher<TaskRequest>
{
public:
	TaskMultiplexer(PollThread *o) : TaskDispatcher<TaskRequest>(o), output(o), fastUpdate(0){};
	virtual ~TaskMultiplexer(void);

	void bind_dispatcher(TaskDispatcher<TaskRequest> *p) { output.bind_dispatcher(p); }
	void push_task_queue(TaskRequest *req) { output.indirect_notify(req); }
	void enable_fast_update(void) { fastUpdate = 1; }

private:
	RequestOutput<TaskRequest> output;
	virtual void task_notify(TaskRequest *);
	int fastUpdate;
};

#endif
