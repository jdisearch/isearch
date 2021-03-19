/*
 * =====================================================================================
 *
 *       Filename:  helper_group.h
 *
 *    Description:  database collection group helper.
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
#ifndef __HELPER_GROUP_H__
#define __HELPER_GROUP_H__

#include "lqueue.h"
#include "poll_thread.h"

#include "list.h"
#include "value.h"
#include "request_base.h"
#include "stat_dtc.h"

class HelperClient;
class HelperClientList;
class TaskRequest;

class HelperGroup : private TimerObject, public TaskDispatcher<TaskRequest>
{
public:
	HelperGroup(const char *sockpath, const char *name, int hc, int qs,
				int statIndex);
	~HelperGroup();

	int queue_full(void) const { return queue.Count() >= queueSize; }
	int queue_empty(void) const { return queue.Count() == 0; }
	int has_free_helper(void) const { return queue.Count() == 0 && freeHelper.ListEmpty(); }

	/* process or queue a task */
	virtual void task_notify(TaskRequest *);

	int Attach(PollThread *, LinkQueue<TaskRequest *>::allocator *);
	int connect_helper(int);
	const char *sock_path(void) const { return sockpath; }

	void set_timer_handler(TimerList *recv, TimerList *conn, TimerList *retry)
	{
		recvList = recv;
		connList = conn;
		retryList = retry;
		attach_timer(retryList);
	}

	void queue_back_task(TaskRequest *);
	void request_completed(HelperClient *);
	void connection_reset(HelperClient *);
	void check_queue_expire(void);
	void dump_state(void);

	void add_ready_helper();
	void dec_ready_helper();

private:
	virtual void timer_notify(void);
	/* trying pop task and process */
	void flush_task(uint64_t time);
	/* process a task, must has free helper */
	void process_task(TaskRequest *t);
	const char *Name() const { return name; }
	void record_response_delay(unsigned int t);
	int accept_new_request_fail(TaskRequest *);
	void group_notify_helper_reload_config(TaskRequest *task);
	void process_reload_config(TaskRequest *task);

public:
	TimerList *recvList;
	TimerList *connList;
	TimerList *retryList;

private:
	LinkQueue<TaskRequest *> queue;
	int queueSize;

	int helperCount;
	int helperMax;
	int readyHelperCnt;
	char *sockpath;
	char name[24];

	HelperClientList *helperList;
	ListObject<HelperClientList> freeHelper;

public:
	HelperGroup *fallback;

public:
	void record_process_time(int type, unsigned int msec);
	/* queue当前长度 */
	int queue_count(void) const { return queue.Count(); }
	/* queue最大长度*/
	int queue_max_count(void) const { return queueSize; }

private:
	/* 平均请求时延 */
	double average_delay;

	StatSample statTime[6];
};

#endif
