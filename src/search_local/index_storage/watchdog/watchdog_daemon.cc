/*
 * =====================================================================================
 *
 *       Filename:  watchdog_daemon.cc
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
#include <fcntl.h>
#include <errno.h>

#include "watchdog_daemon.h"
#include "log.h"

WatchDogDaemon::WatchDogDaemon(WatchDog *o, int sec)
	: WatchDogObject(o)
{
	if (o)
		timerList = o->get_timer_list(sec);
}

WatchDogDaemon::~WatchDogDaemon(void)
{
}

int WatchDogDaemon::Fork(void)
{
	// an error detection pipe
	int err, fd[2];
	int unused;

	unused = pipe(fd);

	// fork child process
	pid = fork();
	if (pid == -1)
		return pid;

	if (pid == 0)
	{
		// close pipe if exec succ
		close(fd[0]);
		fcntl(fd[1], F_SETFD, FD_CLOEXEC);
		Exec();
		err = errno;
		log_error("%s: exec(): %m", name);
		unused = write(fd[1], &err, sizeof(err));
		exit(-1);
	}

	close(fd[1]);

	if (read(fd[0], &err, sizeof(err)) == sizeof(err))
	{
		errno = err;
		return -1;
	}
	close(fd[0]);

	attach_watch_dog();
	return pid;
}

void WatchDogDaemon::timer_notify(void)
{
	if (Fork() < 0)
		if (timerList)
			attach_timer(timerList);
}

void WatchDogDaemon::killed_notify(int sig, int cd)
{
	if (timerList)
		attach_timer(timerList);
	report_kill_alarm(sig, cd);
}

void WatchDogDaemon::exited_notify(int retval)
{
	if (timerList)
		attach_timer(timerList);
}
