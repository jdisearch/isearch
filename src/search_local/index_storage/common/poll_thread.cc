/*
 * =====================================================================================
 *
 *       Filename:  pool_thread.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#include "poll_thread.h"
#include "mem_check.h"
#include "poller.h"
#include "myepoll.h"
#include "log.h"
#include <sched.h>

volatile extern int stop;

PollThread::PollThread(const char *name) : Thread(name, Thread::ThreadTypeAsync), PollerUnit(1000), pollTimeout(2000)
{
}

PollThread::~PollThread()
{
}

int PollThread::Initialize(void)
{
	if (Thread::g_autoconf != NULL)
	{
		int mp0 = get_max_pollers();
		int mp1;
		mp1 = g_autoconf->get_int_val("MaxIncomingPollers", Name(), mp0);
		log_debug("autoconf thread %s MaxIncomingPollers %d", taskname, mp1);
		if (mp1 > mp0)
		{
			set_max_pollers(mp1);
		}
	}
	if (initialize_poller_unit() < 0)
		return -1;
	return 0;
}

void *PollThread::Process(void)
{
	while (!Stopping())
	{
		// if previous event loop has no events,
		// don't allow zero epoll wait time
		int timeout = expire_micro_seconds(pollTimeout, nrEvents == 0);
		int interrupted = wait_poller_events(timeout);
		update_now_time(timeout, interrupted);

		if (Stopping())
			break;

		process_poller_events();
		check_expired(get_now_time());
		TimerUnit::check_ready();
		ReadyUnit::check_ready(get_now_time());
		delay_apply_events();
	}
	return 0;
}
