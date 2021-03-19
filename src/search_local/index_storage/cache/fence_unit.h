/*
 * =====================================================================================
 *
 *       Filename:  fence_unit.h
 *
 *    Description:  barrier uint class definition.
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
#ifndef __BARRIER_UNIT_H__
#define __BARRIER_UNIT_H__

#include <stdint.h>
#include <list.h>
#include "task_request.h"
#include "timer_list.h"
#include "fence.h"
#include "stat_dtc.h"

#define BARRIER_HASH_MAX 1024 * 8

class TaskRequest;
class PollThread;
class BarrierUnit;

class BarrierUnit : public TaskDispatcher<TaskRequest>, public ReplyDispatcher<TaskRequest>
{
public:
	enum E_BARRIER_UNIT_PLACE
	{
		IN_FRONT,
		IN_BACK
	};
	BarrierUnit(PollThread *, int max, int maxkeycount, E_BARRIER_UNIT_PLACE place);
	~BarrierUnit();
	virtual void task_notify(TaskRequest *);
	virtual void reply_notify(TaskRequest *);

	void chain_request(TaskRequest *p)
	{
		p->push_reply_dispatcher(this);
		output.task_notify(p);
	}
	void queue_request(TaskRequest *p)
	{
		p->push_reply_dispatcher(this);
		output.indirect_notify(p);
	}
	PollThread *owner_thread(void) const { return owner; }
	void attach_free_barrier(Barrier *);
	int max_count_by_key(void) const { return maxKeyCount; }
	void bind_dispatcher(TaskDispatcher<TaskRequest> *p) { output.bind_dispatcher(p); }
	int barrier_count() const { return count; }

protected:
	int count;
	LinkQueue<TaskRequest *>::allocator taskQueueAllocator;
	ListObject<Barrier> freeList;
	ListObject<Barrier> hashSlot[BARRIER_HASH_MAX];
	int maxBarrier;

	Barrier *get_barrier(unsigned long key);
	Barrier *get_barrier_by_idx(unsigned long idx);
	int key2idx(unsigned long key) { return key % BARRIER_HASH_MAX; }

private:
	int maxKeyCount;

	RequestOutput<TaskRequest> output;

	//stat
	StatItemU32 statBarrierCount;
	StatItemU32 statBarrierMaxTask;
};

#endif
