/*
 * =====================================================================================
 *
 *       Filename:  watchdog.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  linjinming, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __H_WATCHDOG__H__
#define __H_WATCHDOG__H__

#include "watchdog_unit.h"
#include "poller.h"
#include "timer_list.h"

class WatchDogPipe : public PollerObject
{
private:
	int peerfd;

public:
	WatchDogPipe(void);
	virtual ~WatchDogPipe(void);
	void Wake(void);
	virtual void input_notify(void);
};

class WatchDog : public WatchDogUnit,
				 public TimerUnit
{
private:
	PollerObject *listener;

public:
	WatchDog(void);
	virtual ~WatchDog(void);

	void set_listener(PollerObject *l) { listener = l; }
	void run_loop(void);
};

extern int start_watch_dog(int (*entry)(void *), void *);
#endif
