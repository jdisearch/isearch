/*
 * =====================================================================================
 *
 *       Filename:  buffer_flush.cc
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
#include "buffer_flush.h"
#include "buffer_process.h"
#include "global.h"

DTCFlushRequest::DTCFlushRequest(BufferProcess *o, const char *key) : owner(o),
																	 numReq(0),
																	 badReq(0),
																	 wait(NULL)
{
}

DTCFlushRequest::~DTCFlushRequest()
{
	if (wait)
	{
		wait->reply_notify();
		wait = NULL;
	}
}

class DropDataReply : public ReplyDispatcher<TaskRequest>
{
public:
	DropDataReply() {}
	virtual void reply_notify(TaskRequest *cur);
};

void DropDataReply::reply_notify(TaskRequest *cur)
{
	DTCFlushRequest *req = cur->OwnerInfo<DTCFlushRequest>();
	if (req == NULL)
		delete cur;
	else
		req->complete_row(cur, cur->owner_index());
}

static DropDataReply dropReply;

int DTCFlushRequest::flush_row(const RowValue &row)
{
	TaskRequest *pTask = new TaskRequest;
	if (pTask == NULL)
	{
		log_error("cannot flush row, new task error, possible memory exhausted\n");
		return -1;
	}

	if (pTask->Copy(row) < 0)
	{
		log_error("cannot flush row, from: %s   error: %s \n",
				  pTask->resultInfo.error_from(),
				  pTask->resultInfo.error_message());
		return -1;
	}
	pTask->set_request_type(TaskTypeCommit);
	pTask->push_reply_dispatcher(&dropReply);
	pTask->set_owner_info(this, numReq, NULL);
	owner->inc_async_flush_stat();
	//TaskTypeCommit never expired
	//pTask->set_expire_time(3600*1000/*ms*/);
	numReq++;
	owner->push_flush_queue(pTask);
	return 0;
}

void DTCFlushRequest::complete_row(TaskRequest *req, int index)
{
	delete req;
	numReq--;
	if (numReq == 0)
	{
		if (wait)
		{
			wait->reply_notify();
			wait = NULL;
		}
		owner->complete_flush_request(this);
	}
}

MARKER_STAMP BufferProcess::calculate_current_marker()
{
	time_t now;

	time(&now);
	return now - (now % markerInterval);
}

void BufferProcess::set_drop_count(int c)
{
	//	Cache.set_drop_count(c);
}

void BufferProcess::get_dirty_stat()
{
	uint64_t ullMaxNode;
	uint64_t ullMaxRow;
	const double rate = 0.9;

	if (DTCBinMalloc::Instance()->user_alloc_size() >= DTCBinMalloc::Instance()->total_size() * rate)
	{
		ullMaxNode = Cache.total_used_node();
		ullMaxRow = Cache.total_used_row();
	}
	else
	{
		if (DTCBinMalloc::Instance()->user_alloc_size() > 0)
		{
			double enlarge = DTCBinMalloc::Instance()->total_size() * rate / DTCBinMalloc::Instance()->user_alloc_size();
			ullMaxNode = (uint64_t)(Cache.total_used_node() * enlarge);
			ullMaxRow = (uint64_t)(Cache.total_used_row() * enlarge);
		}
		else
		{
			ullMaxNode = 0;
			ullMaxRow = 0;
		}
	}
}

void BufferProcess::set_flush_parameter(
	int intvl,
	int mreq,
	int mintime,
	int maxtime)
{
	// require v4 cache
	if (Cache.get_cache_info()->version < 4)
		return;

	/*
	if(intvl < 60)
		intvl = 60;
	else if(intvl > 43200)
		intvl = 43200;
	*/

	/* marker time interval changed to 1sec */
	intvl = 1;
	markerInterval = intvl;

	/* 1. make sure at least one time marker exist
	 * 2. init first marker time and last marker time
	 * */
	Node stTimeNode = Cache.first_time_marker();
	if (!stTimeNode)
		Cache.insert_time_marker(calculate_current_marker());
	Cache.first_time_marker_time();
	Cache.last_time_marker_time();

	if (mreq <= 0)
		mreq = 1;
	if (mreq > 10000)
		mreq = 10000;

	if (mintime < 10)
		mintime = 10;
	if (maxtime <= mintime)
		maxtime = mintime * 2;

	maxFlushReq = mreq;
	minDirtyTime = mintime;
	maxDirtyTime = maxtime;

	//get_dirty_stat();

	/*attach timer only if async mode or sync mode but mem dirty*/
	if (updateMode == MODE_ASYNC ||
		(updateMode == MODE_SYNC && mem_dirty == true))
	{
		/* check for expired dirty node every second */
		flushTimer = owner->get_timer_list(1);
		attach_timer(flushTimer);
	}
}

int BufferProcess::commit_flush_request(DTCFlushRequest *req, TaskRequest *callbackTask)
{
	req->wait = callbackTask;

	if (req->numReq == 0)
		delete req;
	else
		nFlushReq++;

	statCurrFlushReq = nFlushReq;
	return 0;
}

void BufferProcess::complete_flush_request(DTCFlushRequest *req)
{
	delete req;
	nFlushReq--;
	statCurrFlushReq = nFlushReq;

	calculate_flush_speed(0);

	if (nFlushReq < mFlushReq)
		flush_next_node();
}

void BufferProcess::timer_notify(void)
{
	log_debug("flush timer event...");
	int ret = 0;

	MARKER_STAMP cur = calculate_current_marker();
	if (Cache.first_time_marker_time() != cur)
		Cache.insert_time_marker(cur);

	calculate_flush_speed(1);

	/* flush next node return
	 * 1: no dirty node exist, sync dtc, should not attach timer again
	 * 0: one flush request created, nFlushReq inc in flush_next_node, notinue
	 * others: on flush request created due to some reason, should break for another flush timer event, otherwise may be    
	 * block here, eg. no dirty node exist, and in async mode
	 * */
	while (nFlushReq < mFlushReq)
	{
		ret = flush_next_node();
		if (ret == 0)
		{
			continue;
		}
		else
		{
			break;
		}
	}

	/*SYNC + mem_dirty/ASYNC need to reattach flush timer*/
	if ((updateMode == MODE_SYNC && mem_dirty == true) || updateMode == MODE_ASYNC)
		attach_timer(flushTimer);
}

int BufferProcess::oldest_dirty_node_alarm()
{
	Node stHead = Cache.dirty_lru_head();
	Node stNode = stHead.Prev();

	if (Cache.is_time_marker(stNode))
	{
		stNode = stNode.Prev();
		if (Cache.is_time_marker(stNode) || stNode == stHead)
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else if (stNode == stHead)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

/*flush speed(nFlushReq) only depend on oldest dirty node existing time*/
void BufferProcess::calculate_flush_speed(int is_flush_timer)
{
	delete_tail_time_markers();

	// time base
	int m, v;
	unsigned int t1 = Cache.first_time_marker_time();
	unsigned int t2 = Cache.last_time_marker_time();
	//initialized t1 and t2, so no need of test for this
	v = t1 - t2;

	//if start with sync and mem dirty, flush as fast as we can
	if (updateMode == MODE_SYNC)
	{
		if (mem_dirty == false)
		{
			mFlushReq = 0;
		}
		else
		{
			mFlushReq = maxFlushReq;
		}
		goto __stat;
	}

	//alarm if oldest dirty node exist too much time, flush at fastest speed
	if (v >= maxDirtyTime)
	{
		mFlushReq = maxFlushReq;
		if (oldest_dirty_node_alarm() && is_flush_timer)
		{
			log_notice("oldest dirty node exist time > max dirty time");
		}
	}
	else if (v >= minDirtyTime)
	{
		m = 1 + (v - minDirtyTime) * (maxFlushReq - 1) / (maxDirtyTime - minDirtyTime);
		if (m > mFlushReq)
			mFlushReq = m;
	}
	else
	{
		mFlushReq = 0;
	}

__stat:
	if (mFlushReq > maxFlushReq)
		mFlushReq = maxFlushReq;

	statMaxFlushReq = mFlushReq;
	statOldestDirtyTime = v;
}

/* return -1: encount the only time marker
 * return  1: no dirty node exist, clear mem dirty
 * return  2: no dirty node exist, in async mode
 * return -2: no flush request created
 * return  0: one flush request created
 * */
int BufferProcess::flush_next_node(void)
{
	unsigned int uiFlushRowsCnt = 0;
	MARKER_STAMP stamp;
	static MARKER_STAMP last_rm_stamp;

	Node stHead = Cache.dirty_lru_head();
	Node stNode = stHead;
	Node stPreNode = stNode.Prev();

	/*case 1: delete continues time marker, until 
     *        encount a normal node/head node, go next
     *        encount the only time marker*/
	while (1)
	{
		stNode = stPreNode;
		stPreNode = stNode.Prev();

		if (!Cache.is_time_marker(stNode))
			break;

		if (Cache.first_time_marker_time() == stNode.Time())
		{
			if (updateMode == MODE_SYNC && mem_dirty == true)
			{
				/* delete this time marker, flush all dirty node */
				Cache.remove_time_marker(stNode);
				stNode = stPreNode;
				stPreNode = stNode.Prev();
				while (stNode != stHead)
				{
					buffer_flush_data_timer(stNode, uiFlushRowsCnt);
					stNode = stPreNode;
					stPreNode = stNode.Prev();
				}

				disable_timer();
				mem_dirty = false;
				log_notice("mem clean now for sync cache");
				return 1;
			}
			return -1;
		}

		stamp = stNode.Time();
		if (stamp > last_rm_stamp)
		{
			last_rm_stamp = stamp;
		}

		log_debug("remove time marker in dirty lru, time %u", stNode.Time());
		Cache.remove_time_marker(stNode);
	}

	/*case 2: this the head node, clear mem dirty if nessary, return, should not happen*/
	if (stNode == stHead)
	{
		if (updateMode == MODE_SYNC && mem_dirty == true)
		{
			disable_timer();
			mem_dirty = false;
			log_notice("mem clean now for sync cache");
			return 1;
		}
		else
		{
			return 2;
		}
	}

	/*case 3: this a normal node, flush it.
     * 	  return -2 if no flush request added to cache process
     * */
	int iRet = buffer_flush_data_timer(stNode, uiFlushRowsCnt);
	if (iRet == -1 || iRet == -2 || iRet == -3 || iRet == 1)
	{
		return -2;
	}

	return 0;
}

void BufferProcess::delete_tail_time_markers()
{
	Node stHead = Cache.dirty_lru_head();
	Node stNode = stHead;
	Node stPreNode = stNode.Prev();

	while (1)
	{
		stNode = stPreNode;
		stPreNode = stNode.Prev();

		if (stNode == stHead || Cache.first_time_marker_time() == stNode.Time())
			break;

		if (Cache.is_time_marker(stNode) && Cache.is_time_marker(stPreNode))
			Cache.remove_time_marker(stNode);
		else
			break;
	}
}
