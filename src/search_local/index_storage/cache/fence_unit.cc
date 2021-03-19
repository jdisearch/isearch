/*
 * =====================================================================================
 *
 *       Filename:  barrier_unit.cc
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
#include <stdio.h>

#include <fence.h>
#include <fence_unit.h>
#include <poll_thread.h>

#include "log.h"

//-------------------------------------------------------------------------
BarrierUnit::BarrierUnit(PollThread *o, int max, int maxkeycount, E_BARRIER_UNIT_PLACE place) : TaskDispatcher<TaskRequest>(o),
																								count(0),
																								maxBarrier(max),
																								maxKeyCount(maxkeycount),
																								output(o)
{
	freeList.InitList();
	for (int i = 0; i < BARRIER_HASH_MAX; i++)
		hashSlot[i].InitList();
	//stat
	if (IN_FRONT == place)
	{
		statBarrierCount = statmgr.get_item_u32(DTC_FRONT_BARRIER_COUNT);
		statBarrierMaxTask = statmgr.get_item_u32(DTC_FRONT_BARRIER_MAX_TASK);
	}
	else if (IN_BACK == place)
	{
		statBarrierCount = statmgr.get_item_u32(DTC_BACK_BARRIER_COUNT);
		statBarrierMaxTask = statmgr.get_item_u32(DTC_BACK_BARRIER_MAX_TASK);
	}
	else
	{
		log_error("bad place value %d", place);
	}
	statBarrierCount = 0;
	statBarrierMaxTask = 0;
}

BarrierUnit::~BarrierUnit()
{
	while (!freeList.ListEmpty())
	{
		delete static_cast<Barrier *>(freeList.ListNext());
	}
	for (int i = 0; i < BARRIER_HASH_MAX; i++)
	{
		while (!hashSlot[i].ListEmpty())
		{
			delete static_cast<Barrier *>(hashSlot[i].ListNext());
		}
	}
}

Barrier *BarrierUnit::get_barrier(unsigned long key)
{
	ListObject<Barrier> *h = &hashSlot[key2idx(key)];
	ListObject<Barrier> *p;

	for (p = h->ListNext(); p != h; p = p->ListNext())
	{
		if (p->ListOwner()->Key() == key)
			return p->ListOwner();
	}

	return NULL;
}

Barrier *BarrierUnit::get_barrier_by_idx(unsigned long idx)
{
	if (idx >= BARRIER_HASH_MAX)
		return NULL;

	ListObject<Barrier> *h = &hashSlot[idx];
	ListObject<Barrier> *p;

	p = h->ListNext();
	return p->ListOwner();
}

void BarrierUnit::attach_free_barrier(Barrier *bar)
{
	bar->ListMove(freeList);
	count--;
	statBarrierCount = count;
	//Stat.set_barrier_count (count);
}

void BarrierUnit::task_notify(TaskRequest *cur)
{
	if (cur->request_code() == DRequest::SvrAdmin &&
		cur->requestInfo.admin_code() != DRequest::ServerAdminCmd::Migrate)
	{
		//Migrate命令在PrepareRequest的时候已经计算了PackedKey和hash，需要跟普通的task一起排队
		chain_request(cur);
		return;
	}
	if (cur->is_batch_request())
	{
		chain_request(cur);
		return;
	}

	unsigned long key = cur->barrier_key();
	Barrier *bar = get_barrier(key);

	if (bar)
	{
		if (bar->Count() < maxKeyCount)
		{
			bar->Push(cur);
			if (bar->Count() > statBarrierMaxTask) //max key number
				statBarrierMaxTask = bar->Count();
		}
		else
		{
			log_warning("barrier[%s]: overload max key count %d bars %d", owner->Name(), maxKeyCount, count);
			cur->set_error(-EC_SERVER_BUSY, __FUNCTION__,
						  "too many request blocked at key");
			cur->reply_notify();
		}
	}
	else if (count >= maxBarrier)
	{
		log_warning("too many barriers, count=%d", count);
		cur->set_error(-EC_SERVER_BUSY, __FUNCTION__,
					  "too many barriers");
		cur->reply_notify();
	}
	else
	{
		if (freeList.ListEmpty())
		{
			bar = new Barrier(&taskQueueAllocator);
		}
		else
		{
			bar = freeList.NextOwner();
		}
		bar->set_key(key);
		bar->list_move_tail(hashSlot[key2idx(key)]);
		bar->Push(cur);
		count++;
		statBarrierCount = count; //barrier number
		//Stat.set_barrier_count (count);
		chain_request(cur);
	}
}

void BarrierUnit::reply_notify(TaskRequest *cur)
{
	if (cur->request_code() == DRequest::SvrAdmin &&
		cur->requestInfo.admin_code() != DRequest::ServerAdminCmd::Migrate)
	{
		cur->reply_notify();
		return;
	}
	if (cur->is_batch_request())
	{
		cur->reply_notify();
		return;
	}

	unsigned long key = cur->barrier_key();
	Barrier *bar = get_barrier(key);
	if (bar == NULL)
	{
		log_error("return task not in barrier, key=%lu", key);
	}
	else if (bar->Front() == cur)
	{
		if (bar->Count() == statBarrierMaxTask) //max key number
			statBarrierMaxTask--;
		bar->Pop();
		TaskRequest *next = bar->Front();
		if (next == NULL)
		{
			attach_free_barrier(bar);
		}
		else
		{
			queue_request(next);
		}
		//printf("pop bar %lu: count %d\n", key, bar->Count());
	}
	else
	{
		log_error("return task not barrier header, key=%lu", key);
	}

	cur->reply_notify();
}
