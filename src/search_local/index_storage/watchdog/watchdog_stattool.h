/*
 * =====================================================================================
 *
 *       Filename:  watchdog_statool.h
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
#ifndef __H_WATCHDOG_STATTOOL_H__
#define __H_WATCHDOG_STATTOOL_H__

#include "watchdog_daemon.h"

class WatchDogStatTool : public WatchDogDaemon
{
public:
	WatchDogStatTool(WatchDog *o, int sec);
	virtual ~WatchDogStatTool(void);
	virtual void Exec(void);
};

#endif
