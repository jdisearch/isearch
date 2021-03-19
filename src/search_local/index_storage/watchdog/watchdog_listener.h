/*
 * =====================================================================================
 *
 *       Filename:  watchdog_listener.h
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
#ifndef __H_WATCHDOG_LISTENER_H__
#define __H_WATCHDOG_LISTENER_H__

#include "watchdog.h"

#define ENV_WATCHDOG_SOCKET_FD "WATCHDOG_SOCKET_FD"

#define WATCHDOG_INPUT_OBJECT 0
#define WATCHDOG_INPUT_HELPER 1

#define DBHELPER_TABLE_ORIGIN 0
#define DBHELPER_TABLE_NEW 1

struct StartHelperPara
{
	uint8_t type;
	uint8_t mach;
	uint8_t role;
	uint8_t conf;
	uint16_t num;
	uint16_t backlog;
};

class WatchDogListener : public PollerObject
{
private:
	WatchDog *owner;
	int peerfd;
	int delay;

public:
	WatchDogListener(WatchDog *o, int sec);
	virtual ~WatchDogListener(void);
	int attach_watch_dog(void);
	virtual void input_notify(void);
};

extern int watch_dog_fork(const char *name, int (*entry)(void *), void *args);
#endif
