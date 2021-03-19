/*
 * =====================================================================================
 *
 *       Filename:  watchdog_helper.h
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
#ifndef __H_WATCHDOG_HELPER_H__
#define __H_WATCHDOG_HELPER_H__

#include "watchdog_daemon.h"
#include "watchdog_listener.h"

class WatchDogHelper : public WatchDogDaemon
{
private:
	const char *path;
	int backlog;
	int type;
	int conf;
	int num;

public:
	WatchDogHelper(WatchDog *o, int sec, const char *p, int, int, int, int t, int c = DBHELPER_TABLE_ORIGIN, int n = -1);
	virtual ~WatchDogHelper(void);
	virtual void Exec(void);
	int Verify(void);
};

#endif
