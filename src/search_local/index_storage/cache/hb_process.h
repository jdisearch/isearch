/*
 * =====================================================================================
 *
 *       Filename:  hb_process.h
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
#ifndef _HB_PROCESS_H_
#define _HB_PROCESS_H_

#include "request_base.h"
#include "hb_log.h"
#include "task_pendlist.h"
#include "stat_manager.h"
#include <map>

class PollThread;
class TaskRequest;
enum THBResult
{
	HB_PROCESS_ERROR = -1,
	HB_PROCESS_OK = 0,
	HB_PROCESS_PENDING = 2,
};

class HBProcess : public TaskDispatcher<TaskRequest>
{
public:
	HBProcess(PollThread *o);
	virtual ~HBProcess();

	virtual void task_notify(TaskRequest *cur);
	bool Init(uint64_t total, off_t max_size);

private:
	/*concrete hb operation*/
	THBResult write_hb_log_process(TaskRequest &task);
	THBResult read_hb_log_process(TaskRequest &task);
	THBResult write_lru_hb_log_process(TaskRequest &task);
	THBResult register_hb_log_process(TaskRequest &task);
	THBResult query_hb_log_info_process(TaskRequest &task);

private:
	PollThread *ownerThread;
	RequestOutput<TaskRequest> output;
	TaskPendingList taskPendList;
	HBLog hbLog;
	StatSample statIncSyncStep;
};

#endif
