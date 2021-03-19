/*
 * =====================================================================================
 *
 *       Filename:  buffer_bypass.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Norton, yangshuang68@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <buffer_bypass.h>

class ReplyBypass : public ReplyDispatcher<TaskRequest>
{
public:
	ReplyBypass(void) {}
	virtual ~ReplyBypass(void);
	virtual void reply_notify(TaskRequest *task);
};

ReplyBypass::~ReplyBypass(void) {}

void ReplyBypass::reply_notify(TaskRequest *task)
{
	if (task->result)
		task->pass_all_result(task->result);
	task->reply_notify();
}

static ReplyBypass replyBypass;

BufferBypass::~BufferBypass(void)
{
}

void BufferBypass::task_notify(TaskRequest *cur)
{
	if (cur->is_batch_request())
	{
		cur->reply_notify();
		return;
	}

	if (cur->count_only() && (cur->requestInfo.limit_start() || cur->requestInfo.limit_count()))
	{
		cur->set_error(-EC_BAD_COMMAND, "BufferBypass", "There's nothing to limit because no fields required");
		cur->reply_notify();
		return;
	}

	cur->mark_as_pass_thru();
	cur->push_reply_dispatcher(&replyBypass);
	output.task_notify(cur);
}
