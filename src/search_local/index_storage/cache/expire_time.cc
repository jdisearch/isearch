/*
 * =====================================================================================
 *
 *       Filename:  expire_time.cc
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
#include "expire_time.h"
#include <time.h>
#include <stdlib.h>

DTC_USING_NAMESPACE

ExpireTime::ExpireTime(TimerList *t, DTCBufferPool *c, DataProcess *p, DTCTableDefinition *td, int e) : timer(t),
																									cache(c),
																									process(p),
																									tableDef(td),
																									maxExpire(e)
{
	statExpireCount = statmgr.get_item_u32(DTC_KEY_EXPIRE_DTC_COUNT);
	statGetCount = statmgr.get_item_u32(DTC_GET_COUNT);
	statInsertCount = statmgr.get_item_u32(DTC_INSERT_COUNT);
	statUpdateCount = statmgr.get_item_u32(DTC_UPDATE_COUNT);
	statDeleteCount = statmgr.get_item_u32(DTC_DELETE_COUNT);
	statPurgeCount = statmgr.get_item_u32(DTC_PURGE_COUNT);
}

ExpireTime::~ExpireTime()
{
}

void ExpireTime::start_key_expired_task(void)
{
	log_info("start key expired task");
	attach_timer(timer);
	return;
}

int ExpireTime::try_expire_count()
{
	int num1 = maxExpire - (statGetCount.get() + statInsertCount.get() +
							statUpdateCount.get() + statDeleteCount.get() +
							statPurgeCount.get()) /
							   10;
	int num2 = cache->total_used_node();
	return num1 < num2 ? num1 : num2;
}

void ExpireTime::timer_notify(void)
{
	log_debug("sched key expire task");
	int start = cache->min_valid_node_id(), end = cache->max_node_id();
	int count, interval = end - start, node_id;
	int i, j, k = 0;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	log_debug("tv.tv_usec: %ld", tv.tv_usec);
	srandom(tv.tv_usec);
	count = try_expire_count();
	log_debug("try_expire_count: %d", count);
	for (i = 0, j = 0; i < count && j < count * 3; ++j)
	{
		Node node;
		node_id = random() % interval + start;
		node = I_SEARCH(node_id);
		uint32_t expire = 0;
		if (!!node && !node.not_in_lru_list() && !cache->is_time_marker(node))
		{
			// read expire time
			// if expired
			// 	purge
			++i;
			if (process->get_expire_time(tableDef, &node, expire) != 0)
			{
				log_error("get expire time error for node: %d", node.node_id());
				continue;
			}
			log_debug("node id: %d, expire: %d, current: %ld", node.node_id(), expire, tv.tv_sec);
			if (expire != 0 && expire < tv.tv_sec)
			{
				log_debug("expire time timer purge node: %d, %d", node.node_id(), ++k);
				cache->inc_total_row(0LL - cache->node_rows_count(node));
				if (cache->purge_node_everything(node) != 0)
				{
					log_error("purge node error, node: %d", node.node_id());
				}
				++statExpireCount;
			}
		}
	}
	log_debug("expire time found %d real node, %d", i, k);

	attach_timer(timer);
	return;
}
