/*
 * =====================================================================================
 *
 *       Filename:  blacklist_unit.cc
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
#include <blacklist_unit.h>

DTC_USING_NAMESPACE

BlackListUnit::BlackListUnit(TimerList *t) : timer(t)
{
}

BlackListUnit::~BlackListUnit()
{
}

void BlackListUnit::timer_notify(void)
{
	log_debug("sched blacklist-expired task");

	/* expire all expired eslot */
	try_expired_blacklist();

	BlackList::dump_all_blslot();

	/* set timer agagin */
	attach_timer(timer);

	return;
}
