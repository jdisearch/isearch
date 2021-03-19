/*
 * =====================================================================================
 *
 *       Filename:  hb_process.cc
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
#include "hb_process.h"
#include "poll_thread.h"
#include "task_request.h"
#include "log.h"
#include "hotback_task.h"

extern DTCTableDefinition *gTableDef[];

HBProcess::HBProcess(PollThread *o) : TaskDispatcher<TaskRequest>(o),
									  ownerThread(o),
									  output(o),
									  taskPendList(this),
									  hbLog(TableDefinitionManager::Instance()->get_hot_backup_table_def())
{
}

HBProcess::~HBProcess()
{
}
void HBProcess::task_notify(TaskRequest *cur)
{
	log_debug("request type is %d ", cur->request_type());
	THBResult result = HB_PROCESS_ERROR;
	switch (cur->request_type())
	{
	case TaskTypeWriteHbLog:
	{
		result = write_hb_log_process(*cur);
		break;
	}
	case TaskTypeReadHbLog:
	{
		result = read_hb_log_process(*cur);
		break;
	}
	case TaskTypeWriteLruHbLog:
	{
		result = write_lru_hb_log_process(*cur);
		break;
	}
	case TaskTypeRegisterHbLog:
	{
		result = register_hb_log_process(*cur);
		break;
	}
	case TaskTypeQueryHbLogInfo:
	{
		result = query_hb_log_info_process(*cur);
		break;
	}
	default:
	{
		cur->set_error(-EBADRQC, "hb process", "invalid hb cmd code");
		log_notice("invalid hb cmd code[%d]", cur->request_type());
		cur->reply_notify();
		return;
	}
	}

	if (HB_PROCESS_PENDING == result)
	{
		log_debug("hb task is pending ");
		return;
	}
	log_debug("hb task reply");
	cur->reply_notify();
	return;
}

bool HBProcess::Init(uint64_t total, off_t max_size)
{
	log_debug("total: %lu, max_size: %ld", total, max_size);
	if (hbLog.Init("../log/hblog", "hblog", total, max_size))
	{
		log_error("hotback process for hblog init failed");
		return false;
	}

	return true;
}

THBResult HBProcess::write_hb_log_process(TaskRequest &task)
{
	if (0 != hbLog.write_update_log(task))
	{
		task.set_error(-EC_ERR_HOTBACK_WRITEUPDATE, "HBProcess", "write_hb_log_process fail");
		return HB_PROCESS_ERROR;
	}
	taskPendList.Wakeup();
	return HB_PROCESS_OK;
}

THBResult HBProcess::write_lru_hb_log_process(TaskRequest &task)
{
	if (0 != hbLog.write_lru_hb_log(task))
	{
		task.set_error(-EC_ERR_HOTBACK_WRITELRU, "HBProcess", "write_lru_hb_log_process fail");
		return HB_PROCESS_ERROR;
	}
	return HB_PROCESS_OK;
}

THBResult HBProcess::read_hb_log_process(TaskRequest &task)
{
	log_debug("read Hb log begin ");
	JournalID hb_jid = task.versionInfo.hot_backup_id();
	JournalID write_jid = hbLog.get_writer_jid();

	if (hb_jid.GE(write_jid))
	{
		taskPendList.add2_list(&task);
		return HB_PROCESS_PENDING;
	}

	if (hbLog.Seek(hb_jid))
	{
		task.set_error(-EC_BAD_HOTBACKUP_JID, "HBProcess", "read_hb_log_process jid overflow");
		return HB_PROCESS_ERROR;
	}

	task.prepare_result_no_limit();

	int count = hbLog.task_append_all_rows(task, task.requestInfo.limit_count());
	if (count >= 0)
	{
		statIncSyncStep.push(count);
	}
	else
	{
		task.set_error(-EC_ERROR_BASE, "HBProcess", "read_hb_log_process,decode binlog error");
		return HB_PROCESS_ERROR;
	}

	task.versionInfo.set_hot_backup_id((uint64_t)hbLog.get_reader_jid());
	return HB_PROCESS_OK;
}
THBResult HBProcess::register_hb_log_process(TaskRequest &task)
{

	JournalID client_jid = task.versionInfo.hot_backup_id();
	JournalID master_jid = hbLog.get_writer_jid();
	log_notice("hb register, client[serial=%u, offset=%u], master[serial=%u, offset=%u]",
			   client_jid.serial, client_jid.offset, master_jid.serial, master_jid.offset);

	//full sync
	if (client_jid.Zero())
	{
		log_info("full-sync stage.");
		task.versionInfo.set_hot_backup_id((uint64_t)master_jid);
		task.set_error(-EC_FULL_SYNC_STAGE, "HBProcess", "Register,full-sync stage");
		return HB_PROCESS_ERROR;
	}
	else
	{
		//inc sync
		if (hbLog.Seek(client_jid) == 0)
		{
			log_info("inc-sync stage.");
			task.versionInfo.set_hot_backup_id((uint64_t)client_jid);
			task.set_error(-EC_INC_SYNC_STAGE, "HBProcess", "register, inc-sync stage");
			return HB_PROCESS_ERROR;
		}
		//error
		else
		{
			log_info("err-sync stage.");
			task.versionInfo.set_hot_backup_id((uint64_t)0);
			task.set_error(-EC_ERR_SYNC_STAGE, "HBProcess", "register, err-sync stage");
			return HB_PROCESS_ERROR;
		}
	}
}
THBResult HBProcess::query_hb_log_info_process(TaskRequest &task)
{
	struct DTCServerInfo s_info;
	memset(&s_info, 0x00, sizeof(s_info));
	s_info.version = 0x1;

	JournalID jid = hbLog.get_writer_jid();
	s_info.binlog_id = jid.Serial();
	s_info.binlog_off = jid.Offset();
	task.resultInfo.set_server_info(&s_info);
	return HB_PROCESS_OK;
}
