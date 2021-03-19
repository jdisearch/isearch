/*
 * =====================================================================================
 *
 *       Filename:  poll_thread.h
 *
 *    Description:  poll_thread class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
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
#include "timerlist.h"
#include "thread.h"

class CPollThread : public CThread, public CPollerUnit, public CTimerUnit, public CReadyUnit
{
public:
	CPollThread (const char *name);
	virtual ~CPollThread ();

protected:
	int pollTimeout;
	virtual void *Process(void);
	virtual int Initialize(void);

};

#endif
