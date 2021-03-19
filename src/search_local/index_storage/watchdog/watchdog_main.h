/*
 * =====================================================================================
 *
 *       Filename:  watchdog_main.h
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
#ifndef __H_WATCHDOG_MAIN_H__
#define __H_WATCHDOG_MAIN_H__

#include "watchdog.h"

class WatchDogEntry : private WatchDogObject
{
private:
	int (*entry)(void *);
	void *args;
	int recovery;
	int corecount;

private:
	virtual void killed_notify(int sig, int cd);
	virtual void exited_notify(int rv);

public:
	WatchDogEntry(WatchDogUnit *o, int (*entry)(void *), void *args, int r);
	virtual ~WatchDogEntry(void);

	int Fork(int enCoreDump = 0);
};

#endif
