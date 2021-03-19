/*
 * =====================================================================================
 *
 *       Filename:  watchdog_unit.h
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
#ifndef __H_WATCHDOG_UNIT_H__
#define __H_WATCHDOG_UNIT_H__
#include <stdlib.h>
#include <string.h>
#include <map>

class WatchDogUnit;

class WatchDogObject
{
protected:
	friend class WatchDogUnit;
	WatchDogUnit *owner;
	int pid;
	char name[16];

public:
	WatchDogObject(void) : owner(NULL), pid(0) { name[0] = '0'; }
	WatchDogObject(WatchDogUnit *u) : owner(u), pid(0) { name[0] = '0'; }
	WatchDogObject(WatchDogUnit *u, const char *n) : owner(u), pid(0)
	{
		strncpy(name, n, sizeof(name));
	}
	WatchDogObject(WatchDogUnit *u, const char *n, int i) : owner(u), pid(i)
	{
		strncpy(name, n, sizeof(name));
	}
	virtual ~WatchDogObject();
	virtual void exited_notify(int retval);
	virtual void killed_notify(int signo, int coredumped);
	const char *Name(void) const { return name; }
	int Pid(void) const { return pid; }
	int attach_watch_dog(WatchDogUnit *o = 0);
	void report_kill_alarm(int signo, int coredumped);
};

class WatchDogUnit
{
private:
	friend class WatchDogObject;
	typedef std::map<int, WatchDogObject *> pidmap_t;
	int pidCount;
	pidmap_t pid2obj;

private:
	int AttachProcess(WatchDogObject *);

public:
	WatchDogUnit(void);
	virtual ~WatchDogUnit(void);
	int CheckWatchDog(void); // return #pid monitored
	int KillAll(void);
	int ForceKillAll(void);
	int ProcessCount(void) const { return pidCount; }
};

#endif
