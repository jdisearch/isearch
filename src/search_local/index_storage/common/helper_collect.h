/*
 * =====================================================================================
 *
 *       Filename:  helper_collect.h
 *
 *    Description:  database collection helper.
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
#ifndef __HELPER_COLLECT_H_
#define __HELPER_COLLECT_H__

#include "dbconfig.h"
#include "poll_thread.h"
#include "request_base.h"
#include "watchdog_listener.h"
#include <stat_dtc.h>

class HelperGroup;
class TimerList;
class KeyGuard;

enum
{
	MASTER_READ_GROUP_COLUMN = 0,
	MASTER_WRITE_GROUP_COLUMN = 1,
	MASTER_COMMIT_GROUP_COLUMN = 2,
	SLAVE_READ_GROUP_COLUMN = 3,
};
class GroupCollect : public TaskDispatcher<TaskRequest>
{
public:
	GroupCollect();
	~GroupCollect();

	int load_config(struct DbConfig *cfg, int ks, int idx = 0);
	int renew_config(struct DbConfig *cfg);
	void collect_notify_helper_reload_config(TaskRequest *task);
	int build_master_group_mapping(int idx = 0);
	int build_helper_object(int idx = 0);
	int notify_watch_dog(StartHelperPara *para);
	int Cleanup();
	int Cleanup2();
	int start_listener(TaskRequest *task);
	bool is_commit_full(TaskRequest *task);
	bool Dispatch(TaskRequest *t);
	int Attach(PollThread *thread, int idx = 0);
	void set_timer_handler(TimerList *recv, TimerList *conn, TimerList *retry, int idx = 0);
	int disable_commit_group(int idx = 0);
	DbConfig *get_db_config(TaskRequest *task);
	int migrate_db(TaskRequest *t);
	int switch_db(TaskRequest *t);

	int has_dummy_machine(void) const { return hasDummyMachine; }

private:
	virtual void task_notify(TaskRequest *);
	HelperGroup *select_group(TaskRequest *t);

	void stat_helper_group_queue_count(HelperGroup **group, unsigned group_count);
	void stat_helper_group_cur_max_queue_count(int iRequestType);
	int get_queue_cur_max_count(int iColumn);

private:
	struct DbConfig *dbConfig[2];

	int hasDummyMachine;

	HelperGroup **groups[2];
#define GMAP_NONE -1
#define GMAP_DUMMY -2
#define GROUP_DUMMY ((HelperGroup *)-2)
#define GROUP_READONLY ((HelperGroup *)-3)
	short *groupMap[2];
	ReplyDispatcher<TaskRequest> *guardReply;
	LinkQueue<TaskRequest *>::allocator taskQueueAllocator;

	TimerList *recvList;
	TimerList *connList;
	TimerList *retryList;

	std::vector<int> newDb;
	std::map<int, int> new2old;
	int tableNo;

public:
	KeyGuard *guard;

private:
	StatItemU32 statQueueCurCount;			/*所有组当前总的队列大小*/
	StatItemU32 statQueueMaxCount;			/*所有组配置总的队列大小*/
	StatItemU32 statReadQueueCurMaxCount;	/*所有机器所有主读组当前最大的队列大小*/
	StatItemU32 statWriteQueueMaxCount;		/*所有机器所有写组当前最大的队列大小*/
	StatItemU32 statCommitQueueCurMaxCount; /*所有机器所有提交组当前最大的队列大小*/
	StatItemU32 statSlaveReadQueueMaxCount; /*所有机器所有备读组当前最大的队列大小*/
};

#endif
