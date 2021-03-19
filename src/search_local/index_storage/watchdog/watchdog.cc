/*
 * =====================================================================================
 *
 *       Filename:  watchdog.cc
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
#include <fcntl.h>
#include <sys/poll.h>

#include "watchdog.h"
#include "daemon.h"
#include "log.h"

WatchDogPipe::WatchDogPipe(void)
{
	int fd[2];
	int unused;

	unused = pipe(fd);
	netfd = fd[0];
	peerfd = fd[1];
	fcntl(netfd, F_SETFL, O_NONBLOCK);
	fcntl(peerfd, F_SETFL, O_NONBLOCK);
	fcntl(netfd, F_SETFD, FD_CLOEXEC);
	fcntl(peerfd, F_SETFD, FD_CLOEXEC);
	enable_input();
}

WatchDogPipe::~WatchDogPipe(void)
{
	if (peerfd >= 0)
		close(peerfd);
}

void WatchDogPipe::input_notify(void)
{
	char buf[100];
	while (read(netfd, buf, sizeof(buf)) == sizeof(buf))
		;
}

void WatchDogPipe::Wake(void)
{
	char c = 0;
	c = write(peerfd, &c, 1);
}

static WatchDogPipe *notifier;
static void sighdlr(int signo) { notifier->Wake(); }

WatchDog::WatchDog(void)
{
	//立马注册进程退出处理函数，解决启动时创建太多进程导致部分进程退出没有收到信号linjinming 2014-06-14
	notifier = new WatchDogPipe;
	signal(SIGCHLD, sighdlr);
}

WatchDog::~WatchDog(void)
{
}

void WatchDog::run_loop(void)
{
	struct pollfd pfd[2];
	notifier->init_poll_fd(&pfd[0]);

	if (listener)
	{
		listener->init_poll_fd(&pfd[1]);
	}
	else
	{
		pfd[1].fd = -1;
		pfd[1].events = 0;
		pfd[1].revents = 0;
	}

	while (!stop)
	{
		int timeout = expire_micro_seconds(3600 * 1000, 1 /*nonzero*/);
		int interrupted = poll(pfd, 2, timeout);
		update_now_time(timeout, interrupted);
		if (stop)
			break;

		if (pfd[0].revents & POLLIN)
			notifier->input_notify();
		if (pfd[1].revents & POLLIN)
			listener->input_notify();

		CheckWatchDog();
		check_expired();
		check_ready();
	}

	log_debug("prepare stopping");
	KillAll();
	CheckWatchDog();

	time_t stoptimer = time(NULL) + 5;
	int stopretry = 0;
	while (stopretry < 6 && ProcessCount() > 0)
	{
		time_t now = time(NULL);
		if (stoptimer <= now)
		{
			stopretry++;
			stoptimer = now + 5;
			log_debug("notify all children again");
			KillAll();
		}
		poll(pfd, 1, 1000);
		if (pfd[0].revents & POLLIN)
			notifier->input_notify();
		CheckWatchDog();
	}

	delete notifier;
	ForceKillAll();
	log_info("all children stopped, watchdog ended");
	exit(0);
}
