/*
 * =====================================================================================
 *
 *       Filename:  watchdog_main.cc
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
#include <signal.h>
#include <string.h>

#include "watchdog_main.h"
#include "daemon.h"
#include "log.h"
#include "proc_title.h"
extern void _set_log_thread_name_(const char *n);
/*
 * recovery value:
 *    0 -- don't refork
 *    1 -- fork again if crashed
 *    2 -- fork & enable core dump if crashed
 *    3 -- fork if killed
 *    4 -- fork again if exit != 0
 *    5 -- always fork again
 */
static const char *RecoveryInfoStr[] = {
	"None", "Crash", "CrashDebug", "Killed", "Error", "Always"};

WatchDogEntry::WatchDogEntry(WatchDogUnit *o, int (*e)(void *), void *a, int r)
	: WatchDogObject(o), entry(e), args(a), recovery(r), corecount(0)
{
	strncpy(name, "main", sizeof(name));
	_set_log_thread_name_("wdog");
	_set_remote_log_fd_();
	if (recovery > 3)
	{
		char comm[24] = "wdog-";
		get_proc_name(comm + 5);
		set_proc_name(comm);
	}
	log_info("Main Process Recovery Mode: %s", RecoveryInfoStr[recovery]);
}

WatchDogEntry::~WatchDogEntry(void)
{
}

int WatchDogEntry::Fork(int en)
{
	if ((pid = fork()) == 0)
	{
		if (en)
			daemon_enable_core_dump();
		exit(entry(args));
	}

	if (pid < 0)
		return -1; // cann't fork main process

	attach_watch_dog();
	return pid;
}

void WatchDogEntry::killed_notify(int sig, int coredump)
{
	int corenable = 0;

	if (recovery == 0 || ((recovery <= 2 && sig == SIGKILL) || sig == SIGTERM))
		// main cache exited, stopping all children
		stop = 1;
	else
	{
		++corecount;
		if (corecount == 1 && recovery >= 2)
			corenable = 1;

		sleep(2);
		if (Fork(corenable) < 0)
			// main fork error, stopping all children
			stop = 1;
	}
	report_kill_alarm(sig, coredump);
}

void WatchDogEntry::exited_notify(int retval)
{
	if (recovery < 4 || retval == 0)
		// main cache exited, stopping all children
		stop = 1;
	else
	{
		sleep(2); // sync sleep
		if (Fork() < 0)
			// main fork error, stopping all children
			stop = 1;
	}
}
