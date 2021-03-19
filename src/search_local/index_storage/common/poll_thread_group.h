/*
 * =====================================================================================
 *
 *       Filename:  poll_thread_group.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  10/05/2014 17:40:19 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  linjinming (prudence), linjinming@jd.com
 *        Company:  JD, China
 *
 * =====================================================================================
 */
#ifndef __POLLTHREADGROUP_H__
#define __POLLTHREADGROUP_H__

#include "poll_thread.h"

class PollThreadGroup
{
public:
	PollThreadGroup(const std::string groupName);
	PollThreadGroup(const std::string groupName, int numThreads, int mp);
	virtual ~PollThreadGroup();

	PollThread *get_poll_thread();
	void Start(int numThreads, int mp);
	void running_threads();
	int get_poll_threadIndex(PollThread *thread);
	int get_poll_threadSize();
	PollThread *get_poll_thread(int threadIdx);

protected:
	int threadIndex;
	int numThreads;
	std::string groupName;
	PollThread **pollThreads;
};

#endif
