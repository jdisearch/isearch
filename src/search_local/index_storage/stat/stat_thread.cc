/*
 * =====================================================================================
 *
 *       Filename:  stat_thread.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Norton, yangshuang68@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <sys/mman.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <mem_check.h>
#include <errno.h>
#include <signal.h>
#include <log.h>

#include "stat_thread.h"

StatThread::StatThread()
{
	pthread_mutex_init(&wakeLock, NULL);
}

StatThread::~StatThread()
{
}

void *StatThread::__threadentry(void *p)
{
	StatThread *my = (StatThread *)p;
	my->thread_loop();
	return NULL;
}

int StatThread::start_background_thread(void)
{
	P __a(this);

	if (pthread_mutex_trylock(&wakeLock) == 0)
	{
		int ret = pthread_create(&threadid, 0, __threadentry, (void *)this);
		if (ret != 0)
		{
			errno = ret;
			return -1;
		}
	}
	return 0;
}

int StatThread::stop_background_thread(void)
{
	P __a(this);

	if (pthread_mutex_trylock(&wakeLock) == 0)
	{
		pthread_mutex_unlock(&wakeLock);
		return 0;
	}
	pthread_mutex_unlock(&wakeLock);
	int ret = pthread_join(threadid, 0);
	if (ret == 0)
		log_info("Thread[stat] stopped.");
	else
		log_notice("cannot stop thread[stat]");
	return ret;
}

static void BlockAllSignals(void)
{
	sigset_t sset;
	sigfillset(&sset);
	sigdelset(&sset, SIGSEGV);
	sigdelset(&sset, SIGBUS);
	sigdelset(&sset, SIGABRT);
	sigdelset(&sset, SIGILL);
	sigdelset(&sset, SIGCHLD);
	sigdelset(&sset, SIGFPE);
	pthread_sigmask(SIG_BLOCK, &sset, &sset);
}

void StatThread::thread_loop(void)
{
	_set_log_thread_name_("stat");
	log_info("thread stat[%d] started.", _gettid_());

	BlockAllSignals();

	time_t next = 0;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	next = (tv.tv_sec / 10) * 10 + 10;
	struct timespec ts;
	ts.tv_sec = next;
	ts.tv_nsec = 0;

	while (pthread_mutex_timedlock(&wakeLock, &ts) != 0)
	{
		gettimeofday(&tv, NULL);
		if (tv.tv_sec >= next)
		{
			run_job_once();
			gettimeofday(&tv, NULL);
			next = (tv.tv_sec / 10) * 10 + 10;
		}
		ts.tv_sec = next;
		ts.tv_nsec = 0;
	}
	pthread_mutex_unlock(&wakeLock);
}
