/*
 * =====================================================================================
 *
 *       Filename:  timer_list.cc
 *
 *    Description:  
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
#include <stdint.h>
#include "timer_list.h"
#include "log.h"
#if TIMESTAMP_PRECISION < 1000
#error TIMESTAMP_PRECISION must >= 1000
#endif

#ifndef likely
#if __GCC_MAJOR >= 3
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

TimerObject::~TimerObject(void)
{
}

void TimerObject::timer_notify(void)
{
	log_debug("TimerObject::timer_notify start");
	delete this;
}

void TimerObject::attach_timer(class TimerList *lst)
{
	if (lst->timeout > 0)
		objexp = lst->get_time_unit_now_time() + lst->timeout * (TIMESTAMP_PRECISION / 1000);
	list_move_tail(lst->tlist);
}

int TimerList::check_expired(int64_t now)
{
	int n = 0;
	if (now == 0)
	{
		now = get_time_unit_now_time();
	}
	while (!tlist.ListEmpty())
	{
		TimerObject *tobj = tlist.NextOwner();
		if (tobj->objexp > now)
			break;
		tobj->list_del();
		tobj->timer_notify();
		n++;
	}
	return n;
}

int64_t TimerList::get_time_unit_now_time()
{
	if (NULL == timerUnitOwner)
	{
		return GET_TIMESTAMP();
	}
	return timerUnitOwner->get_now_time();
}

void TimerUnit::update_now_time(int max_wait, int interrupted)
{
	int64_t adjustTime = 0;
	int64_t deadLineTime = 0;
	const int MAX_DELAY_MS = 1000000; /*前向拨动最多量*/
	m_SystemTime = GET_TIMESTAMP();

	if (unlikely(max_wait < 0))
	{
		m_TimeOffSet = 0;
		m_NowTime = m_SystemTime;
		return;
	}

	adjustTime = m_SystemTime + m_TimeOffSet;
	/*时间被向后拨动了*/
	if (adjustTime < m_NowTime)
	{
		adjustTime = interrupted ? (m_NowTime) : (m_NowTime + max_wait * (TIMESTAMP_PRECISION / 1000));
		m_TimeOffSet = adjustTime - m_SystemTime;
		m_NowTime = adjustTime;
		return;
	}

	deadLineTime = m_NowTime + max_wait * (TIMESTAMP_PRECISION / 1000) + MAX_DELAY_MS;
	if (likely(adjustTime < deadLineTime))
	{
		m_NowTime = adjustTime;
		return;
	}
	/*时间被向前拨动了*/
	else
	{
		adjustTime = interrupted ? (m_NowTime) : (m_NowTime + max_wait * (TIMESTAMP_PRECISION / 1000));
		m_TimeOffSet = adjustTime - m_SystemTime;
		m_NowTime = adjustTime;
		return;
	}
}

TimerList *TimerUnit::get_timer_list_by_m_seconds(int to)
{
	TimerList *tl;

	for (tl = next; tl; tl = tl->next)
	{
		if (tl->timeout == to)
			return tl;
	}
	tl = new TimerList(to, this);
	tl->next = next;
	next = tl;
	return tl;
}

TimerUnit::TimerUnit(void) : pending(0), next(NULL), m_TimeOffSet(0)
{
	m_SystemTime = GET_TIMESTAMP();
	m_NowTime = m_SystemTime;
};

TimerUnit::~TimerUnit(void)
{
	while (next)
	{
		TimerList *tl = next;
		next = tl->next;
		delete tl;
	}
};

int TimerUnit::expire_micro_seconds(int msec, int msec0)
{
	int64_t exp;
	TimerList *tl;
	int64_t timestamp = get_now_time();
	exp = timestamp + msec * (TIMESTAMP_PRECISION / 1000);

	for (tl = next; tl; tl = tl->next)
	{
		if (tl->tlist.ListEmpty())
			continue;
		TimerObject *o = tl->tlist.NextOwner();

		if (o->objexp < exp)
			exp = o->objexp;
	}
	exp -= timestamp;
	if (exp <= 0)
		return 0;
	msec = exp / (TIMESTAMP_PRECISION / 1000);
	return msec >= msec0 ? msec : msec0;
}

int TimerUnit::check_ready(void)
{
	int n = 0;
	while (!pending.tlist.ListEmpty())
	{
		TimerObject *tobj = pending.tlist.NextOwner();
		tobj->list_del();
		tobj->timer_notify();
		n++;
	}
	return n;
}

int TimerUnit::check_expired(int64_t now)
{
	if (now == 0)
		now = get_now_time();

	int n = check_ready();
	;
	TimerList *tl;
	for (tl = next; tl; tl = tl->next)
	{
		n += tl->check_expired(now);
	}
	return n;
}

void ReadyObject::ready_notify(uint64_t now)
{
	delete this;
}

int ReadyUnit::check_ready(uint64_t now)
{
	int n = 0;
	ListObject<ReadyObject> *tmp;

	tmp = pending;
	pending = processing;
	processing = tmp;

	while (!processing->ListEmpty())
	{
		ReadyObject *robj = processing->NextOwner();
		robj->list_del();
		robj->ready_notify(now);
		n++;
	}
	return n;
}
