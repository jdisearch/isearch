/*
 * =====================================================================================
 *
 *       Filename:  watchdog_daemon.h
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
#ifndef __H_WATCHDOG_DAEMON_H__
#define __H_WATCHDOG_DAEMON_H__

#include "watchdog.h"

class WatchDogDaemon : public WatchDogObject,
					   private TimerObject
{
private:
	TimerList *timerList;

private:
	virtual void killed_notify(int, int);
	virtual void exited_notify(int);
	virtual void timer_notify(void);

public:
	WatchDogDaemon(WatchDog *o, int sec);
	~WatchDogDaemon(void);

	virtual int Fork(void);
	virtual void Exec(void) = 0;
};

#endif
