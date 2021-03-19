/*
 * =====================================================================================
 *
 *       Filename:  gdb_srv.cc
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
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "gdb.h"

static volatile int stop;
static void sigusr1(int signo) {}
static void sigstop(int signo) { stop = 1; }

void gdb_server(int debug, const char *display)
{
	signal(SIGINT, sigstop);
	signal(SIGTERM, sigstop);
	signal(SIGQUIT, sigstop);
	signal(SIGHUP, sigstop);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_DFL);

	signal(SIGWINCH, sigusr1);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	sigset_t sset;

	sigemptyset(&sset);
	sigaddset(&sset, SIGWINCH);
	sigprocmask(SIG_BLOCK, &sset, NULL);

	const struct timespec timeout = {5, 0};
	siginfo_t info;
	while (!stop)
	{
		if (getppid() == 1)
			break;
		int signo = sigtimedwait(&sset, &info, &timeout);
		if (signo == -1 && errno == -EAGAIN)
			continue;
		if (signo <= 0)
		{
			usleep(100 * 1000);
			continue;
		}

		if (info.si_code != SI_QUEUE)
			continue;

		int tid = info.si_int;
		if (debug == 0)
		{
			gdb_dump(tid);
		}
		else
		{
			gdb_attach(tid, display);
		}
	}
}
