/*
 * =====================================================================================
 *
 *       Filename:  timer_list.h
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
#ifndef __TIMERLIST_H__
#define __TIMERLIST_H__

#include "list.h"
#include "timestamp.h"

class TimerObject;
class TimerUnit;

class TimerList
{
private:
	ListObject<TimerObject> tlist;
	int timeout;
	TimerList *next;
	TimerUnit *timerUnitOwner;

public:
	friend class TimerUnit;
	friend class TimerObject;
	TimerList(int t) : timeout(t), next(NULL)
	{
	}
	TimerList(int t, TimerUnit *timerUnitOwner) : timeout(t), next(NULL), timerUnitOwner(timerUnitOwner)
	{
	}
	int64_t get_time_unit_now_time();
	~TimerList(void) { tlist.FreeList(); }
	int check_expired(int64_t now = 0);
};

class TimerUnit
{
private:
	TimerList pending;
	TimerList *next;

	int64_t m_SystemTime; /*系统时间*/
	int64_t m_NowTime;	  /*应用层时间*/
	int64_t m_TimeOffSet; /*时间拨动后的修正量*/

public:
	friend class TimerObject;
	TimerUnit(void);
	~TimerUnit(void);
	int64_t get_now_time()
	{
		return m_NowTime;
	}
	void update_now_time(int max_wait = 0, int interrupted = 0);
	TimerList *get_timer_list_by_m_seconds(int);
	TimerList *get_timer_list(int t) { return get_timer_list_by_m_seconds(t * 1000); }
	int expire_micro_seconds(int, int = 0); // arg: max/min msec
	int check_expired(int64_t now = 0);
	int check_ready(void);
};

class TimerObject : private ListObject<TimerObject>
{
private:
	int64_t objexp;

public:
	friend class TimerList;
	friend class TimerUnit;
	TimerObject() {}
	virtual ~TimerObject(void);
	virtual void timer_notify(void);
	void disable_timer(void) { ResetList(); }
	void attach_timer(class TimerList *o);
	void attach_ready_timer(class TimerUnit *o) { list_move_tail(o->pending.tlist); }
};

template <class T>
class TimerMember : public TimerObject
{
private:
	T *owner;
	virtual void timer_notify(void) { owner->timer_notify(); }

public:
	TimerMember(T *o) : owner(o) {}
	virtual ~TimerMember() {}
};

class ReadyObject;
class ReadyUnit
{
private:
	ListObject<ReadyObject> pending1;
	ListObject<ReadyObject> pending2;
	ListObject<ReadyObject> *pending;
	ListObject<ReadyObject> *processing;

public:
	friend class ReadyObject;
	ReadyUnit()
	{
		pending = &pending1;
		processing = &pending2;
	}
	virtual ~ReadyUnit() {}

	int check_ready(uint64_t now);
};

class ReadyObject : private ListObject<ReadyObject>
{
public:
	friend class ReadyUnit;
	ReadyObject() {}
	virtual ~ReadyObject() {}

	virtual void ready_notify(uint64_t now);
	void attach_ready(ReadyUnit *o) { list_move_tail(o->pending); }
	void disable_ready() { ResetList(); }
};

#endif
