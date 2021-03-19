/*
 * =====================================================================================
 *
 *       Filename:  buffer_unit.cc
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
#include "log.h"
#include "buffer_process.h"
#include <daemon.h>
#include "buffer_remoteLog.h"

void BufferReplyNotify::reply_notify(TaskRequest *cur)
{
	owner->reply_notify(cur);
}

void BufferProcess::reply_notify(TaskRequest *cur)
{
	if (DRequest::ReloadConfig == cur->request_code() && TaskTypeHelperReloadConfig == cur->request_type())
	{
		/*only delete task */
		log_debug("reload config task reply ,just delete task");
		delete cur;
		return;
	}

	transation_begin(cur);

	if (cur->result_code() < 0)
	{
		buffer_process_error(*cur);
	}
	else if (cur->result_code() > 0)
	{
		log_notice("result_code() > 0: from %s msg %s", cur->resultInfo.error_from(), cur->resultInfo.error_message());
	}
	if (cur->result_code() >= 0 && buffer_process_reply(*cur) != BUFFER_PROCESS_OK)
	{
		if (cur->result_code() >= 0)
			cur->set_error(-EC_SERVER_ERROR, "buffer_process_reply", last_error_message());
	}

	if (!cur->flag_black_hole())
	{
		/* 如果cache操作有失败，则加入黑名单*/
		unsigned blacksize = cur->pop_black_list_size();
		if (blacksize > 0)
		{
			log_debug("add to blacklist, key=%d size=%u", cur->int_key(), blacksize);
			blacklist->add_blacklist(cur->packed_key(), blacksize);
		}
	}

	REMOTE_LOG->write_remote_log(owner->get_now_time() / 1000000, cur, TASK_REPLY_STAGE);
	cur->reply_notify();

	transation_end();

	/* 启动匀速淘汰(deplay purge) */
	Cache.delay_purge_notify();
}

void HotBackReplay::reply_notify(TaskRequest *cur)
{
	log_debug("reply_notify, request type %d", cur->request_type());
	int iRet = cur->result_code();
	if (0 != iRet)
	{
		if ((-ETIMEDOUT == iRet) || (-EC_INC_SYNC_STAGE == iRet) || (-EC_FULL_SYNC_STAGE == iRet))
		{
			log_debug("hotback task , normal fail: from %s msg %s, request type %d", cur->resultInfo.error_from(), cur->resultInfo.error_message(), cur->request_type());
		}
		else
		{
			log_error("hotback task fail: from %s msg %s, request type %d", cur->resultInfo.error_from(), cur->resultInfo.error_message(), cur->request_type());
		}
	}

	if ((TaskTypeWriteHbLog == cur->request_type()) || (TaskTypeWriteLruHbLog == cur->request_type()))
	{
		/*only delete task */
		log_debug("write hotback task reply ,just delete task");
		delete cur;
		return;
	}
	log_debug("read hotback task ,reply to client");
	cur->reply_notify();
}

void FlushReplyNotify::reply_notify(TaskRequest *cur)
{
	owner->transation_begin(cur);
	if (cur->result_code() < 0)
	{
		owner->buffer_process_error(*cur);
	}
	else if (cur->result_code() > 0)
	{
		log_notice("result_code() > 0: from %s msg %s", cur->resultInfo.error_from(), cur->resultInfo.error_message());
	}
	if (cur->result_code() >= 0 && owner->buffer_flush_reply(*cur) != BUFFER_PROCESS_OK)
	{
		if (cur->result_code() >= 0)
			cur->set_error(-EC_SERVER_ERROR, "buffer_flush_reply", owner->last_error_message());
	}
	REMOTE_LOG->write_remote_log(owner->owner->get_now_time() / 1000000, cur, TASK_REPLY_STAGE);
	cur->reply_notify();
	owner->transation_end();
}

void BufferProcess::task_notify(TaskRequest *cur)
{
	tableDef = TableDefinitionManager::Instance()->get_cur_table_def();
	uint64_t now_unix_time = GET_TIMESTAMP() / 1000;
	if (cur->is_expired(now_unix_time))
	{
		log_debug("task time out, throw it for availability, now is [%lld] expire is [%lld]", (long long)now_unix_time, (long long)cur->get_expire_time());
		statBufferProcessExpireCount++;
		cur->set_error(-EC_TASK_TIMEOUT, "buffer_process", "task time out");
		cur->reply_notify();
		return;
	}

	unsigned blacksize = 0;
	transation_begin(cur);

	if (cur->result_code() < 0)
	{
		cur->mark_as_hit(); /* mark as hit if result done */
		cur->reply_notify();
	}
	else if (cur->is_batch_request())
	{
		switch (buffer_process_batch(*cur))
		{
		default:
			cur->set_error(-EC_SERVER_ERROR, "buffer_process", last_error_message());
			cur->mark_as_hit(); /* mark as hit if result done */
			cur->reply_notify();
			break;

		case BUFFER_PROCESS_OK:
			cur->mark_as_hit(); /* mark as hit if result done */
			cur->reply_notify();
			break;

		case BUFFER_PROCESS_ERROR:
			if (cur->result_code() >= 0)
				cur->set_error(-EC_SERVER_ERROR, "buffer_process", last_error_message());
			cur->mark_as_hit(); /* mark as hit if result done */
			cur->reply_notify();
			break;
		}
	}
	else if (nodbMode == false)
	{
		BufferResult result = buffer_process_request(*cur);
		REMOTE_LOG->write_remote_log(owner->get_now_time() / 1000000, cur, TASK_NOTIFY_STAGE);
		switch (result)
		{
		default:
			if (!cur->flag_black_hole())
			{
				/* 如果cache操作有失败，则加入黑名单*/
				blacksize = cur->pop_black_list_size();
				if (blacksize > 0)
				{
					log_debug("add to blacklist, key=%d size=%u", cur->int_key(), blacksize);
					blacklist->add_blacklist(cur->packed_key(), blacksize);
				}
			}
		case BUFFER_PROCESS_ERROR:
			if (cur->result_code() >= 0)
				cur->set_error(-EC_SERVER_ERROR, "buffer_process", last_error_message());

		case BUFFER_PROCESS_OK:
			cur->mark_as_hit(); /* mark as hit if result done */
			cur->reply_notify();
			break;
		case BUFFER_PROCESS_NEXT:
			log_debug("push task to next-unit");
			cur->push_reply_dispatcher(&cacheReply);
			output.task_notify(cur);
			break;
		case BUFFER_PROCESS_PENDING:
			break;
		case BUFFER_PROCESS_REMOTE: //migrate 命令，给远端dtc
			cur->push_reply_dispatcher(&cacheReply);
			remoteoutput.task_notify(cur);
			break;
		case BUFFER_PROCESS_PUSH_HB:
		{
			log_debug("push task to hotback thread");
			break;
		}
		}
	}
	else
	{
		BufferResult result = buffer_process_nodb(*cur);
		REMOTE_LOG->write_remote_log(owner->get_now_time() / 1000000, cur, TASK_NOTIFY_STAGE);
		switch (result)
		{
		default:
		case BUFFER_PROCESS_ERROR:
			if (cur->result_code() >= 0)
				cur->set_error(-EC_SERVER_ERROR, "buffer_process", last_error_message());

		case BUFFER_PROCESS_NEXT:
		case BUFFER_PROCESS_OK:
			cur->mark_as_hit(); /* mark as hit if result done */
			cur->reply_notify();
			break;
		case BUFFER_PROCESS_PENDING:
			break;
		case BUFFER_PROCESS_REMOTE: //migrate 命令，给远端dtc
			cur->push_reply_dispatcher(&cacheReply);
			remoteoutput.task_notify(cur);
			break;
		case BUFFER_PROCESS_PUSH_HB:
		{
			log_debug("push task to hotback thread");
			break;
		}
		}
	}
	transation_end();
	/* 启动匀速淘汰(deplay purge) */
	Cache.delay_purge_notify();
}
