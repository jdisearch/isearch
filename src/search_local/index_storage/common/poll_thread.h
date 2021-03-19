/*
 * =====================================================================================
 *
 *       Filename:  pool_thread.h
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
#ifndef __POLLTHREAD_H__
#define __POLLTHREAD_H__

#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <map>

#include "timestamp.h"
#include "poller.h"
#include "timer_list.h"
#include "thread.h"

class PollThread : public Thread, public PollerUnit, public TimerUnit, public ReadyUnit
{
public:
	PollThread(const char *name);
	virtual ~PollThread();

protected:
	int pollTimeout;
	virtual void *Process(void);
	virtual int Initialize(void);
};

#endif
