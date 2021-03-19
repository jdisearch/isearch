/*
 * =====================================================================================
 *
 *       Filename:  task_multiplexer.cc
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
#include "task_request.h"
#include "task_multiplexer.h"
#include "log.h"

class ReplyMultiplexer : public ReplyDispatcher<TaskRequest>
{
public:
	ReplyMultiplexer(void) {}
	virtual ~ReplyMultiplexer(void);
	virtual void reply_notify(TaskRequest *task);
};

ReplyMultiplexer::~ReplyMultiplexer(void) {}

void ReplyMultiplexer::reply_notify(TaskRequest *cur)
{
	log_debug("ReplyMultiplexer::reply_notify start");
	MultiRequest *req = cur->get_batch_key_list();
	/* reset BatchKey state */
	cur->set_batch_cursor(-1);
	if (cur->result_code() < 0)
	{
		req->second_pass(-1);
	}
	else if (req->remain_count() <= 0)
	{
		req->second_pass(0);
	}
	else if (req->split_task() != 0)
	{
		log_error("split task error");
		cur->set_error(-ENOMEM, __FUNCTION__, "split task error");
		req->second_pass(-1);
	}
	else
	{
		req->second_pass(0);
	}
	log_debug("ReplyMultiplexer::reply_notify end");
}

static ReplyMultiplexer replyMultiplexer;

TaskMultiplexer::~TaskMultiplexer(void)
{
}

void TaskMultiplexer::task_notify(TaskRequest *cur)
{
	log_debug("TaskMultiplexer::task_notify start, flag_multi_key_val: %d", cur->flag_multi_key_val());

	if (!cur->flag_multi_key_val())
	{
		// single task, no dispatcher needed
		output.task_notify(cur);
		return;
	}

#if 0
	// multi-task
	if(cur->flag_pass_thru()){
		log_error("multi-task not support under pass-through mode");
		cur->set_error(-EC_TOO_MANY_KEY_VALUE, __FUNCTION__, "multi-task not support under pass-through mode");
		cur->reply_notify();
		return;
	}
#endif

	switch (cur->request_code())
	{
	case DRequest::Insert:
	case DRequest::Replace:
		if (cur->table_definition()->has_auto_increment())
		{
			log_error("table has autoincrement field, multi-insert/replace not support");
			cur->set_error(-EC_TOO_MANY_KEY_VALUE, __FUNCTION__, "table has autoincrement field, multi-insert/replace not support");
			cur->reply_notify();
			return;
		}
		break;

	case DRequest::Get:
		if (cur->requestInfo.limit_start() != 0 || cur->requestInfo.limit_count() != 0)
		{
			log_error("multi-task not support limit()");
			cur->set_error(-EC_TOO_MANY_KEY_VALUE, __FUNCTION__, "multi-task not support limit()");
			cur->reply_notify();
			return;
		}
	case DRequest::Delete:
	case DRequest::Purge:
	case DRequest::Update:
	case DRequest::Flush:
		break;

	default:
		log_error("multi-task not support other than get/insert/update/delete/purge/replace/flush request");
		cur->set_error(-EC_TOO_MANY_KEY_VALUE, __FUNCTION__, "bad batch request type");
		cur->reply_notify();
		return;
	}

	MultiRequest *req = new MultiRequest(this, cur);
	if (req == NULL)
	{
		log_error("new MultiRequest error: %m");
		cur->set_error(-ENOMEM, __FUNCTION__, "new MultiRequest error");
		cur->reply_notify();
		return;
	}

	if (req->decode_key_list() <= 0)
	{
		/* empty batch or errors */
		cur->reply_notify();
		delete req;
		return;
	}

	cur->set_batch_key_list(req);
	replyMultiplexer.reply_notify(cur);

	log_debug("TaskMultiplexer::task_notify end");
	return;
}
