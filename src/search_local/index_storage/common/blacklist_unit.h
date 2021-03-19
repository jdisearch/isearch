/*
 * =====================================================================================
 *
 *       Filename:  blacklist_unit.h
 *
 *    Description:  balcklist attach excute unit, arun expired task and output stastic result at regular intervals.
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

#ifndef __DTC_BLACKLIST_UNIT_H
#define __DTC_BLACKLIST_UNIT_H

#include "namespace.h"
#include "timer_list.h"
#include "blacklist.h"
#include "log.h"
#include "stat_dtc.h"

DTC_BEGIN_NAMESPACE

class TimerObject;
class BlackListUnit : public BlackList, private TimerObject
{
public:
	BlackListUnit(TimerList *timer);
	virtual ~BlackListUnit(void);

	int init_blacklist(const unsigned max, const unsigned type, const unsigned expired = 10 * 60 /*10 mins*/)
	{
		/* init statisitc item */
		stat_blacksize = statmgr.get_sample(BLACKLIST_SIZE);
		stat_blslot_count = statmgr.get_item_u32(BLACKLIST_CURRENT_SLOT);

		return BlackList::init_blacklist(max, type, expired);
	}

	int add_blacklist(const char *key, const unsigned vsize)
	{
		int ret = BlackList::add_blacklist(key, vsize);
		if (0 == ret)
		{
			/* statistic */
			stat_blacksize.push(vsize);
			stat_blslot_count = current_blslot_count;
		}

		return ret;
	}

	int try_expired_blacklist(void)
	{
		int ret = BlackList::try_expired_blacklist();
		if (0 == ret)
		{
			/* statistic */
			stat_blslot_count = current_blslot_count;
		}

		return ret;
	}

	void start_blacklist_expired_task(void)
	{
		log_info("start blacklist-expired task");

		attach_timer(timer);

		return;
	}

public:
	virtual void timer_notify(void);

private:
	TimerList *timer;

	/* for statistic */
	StatSample stat_blacksize; /* black size distribution */
	StatItemU32 stat_blslot_count;
};

DTC_END_NAMESPACE

#endif
