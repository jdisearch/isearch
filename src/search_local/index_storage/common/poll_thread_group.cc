/*
 * =====================================================================================
 *
 *       Filename:  poll_thread_group.cc
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
#include "poll_thread_group.h"

PollThreadGroup::PollThreadGroup(const std::string groupName) : threadIndex(0), pollThreads(NULL)
{
	this->groupName = groupName;
}

PollThreadGroup::PollThreadGroup(const std::string groupName, int numThreads, int mp) : threadIndex(0), pollThreads(NULL)
{
	this->groupName = groupName;
	Start(numThreads, mp);
}

PollThreadGroup::~PollThreadGroup()
{
	if (pollThreads != NULL)
	{
		for (int i = 0; i < this->numThreads; i++)
		{
			if (pollThreads[i])
			{
				pollThreads[i]->interrupt();
				delete pollThreads[i];
			}
		}

		delete pollThreads;

		pollThreads = NULL;
	}
}

PollThread *PollThreadGroup::get_poll_thread()
{
	if (pollThreads != NULL)
		return pollThreads[threadIndex++ % numThreads];

	return NULL;
}

PollThread *PollThreadGroup::get_poll_thread(int threadIdx)
{
	if (threadIdx > numThreads)
	{
		return NULL;
	}
	else
	{
		return pollThreads[threadIdx];
	}
}

void PollThreadGroup::Start(int numThreads, int mp)
{
	char threadName[256];
	this->numThreads = numThreads;

	pollThreads = new PollThread *[this->numThreads];

	for (int i = 0; i < this->numThreads; i++)
	{
		snprintf(threadName, sizeof(threadName), "%s@%d", groupName.c_str(), i);

		pollThreads[i] = new PollThread(threadName);
		//set_max_pollers一定要再InitializeThread前调用，否则不生效
		pollThreads[i]->set_max_pollers(mp);
		pollThreads[i]->initialize_thread();
	}
}

void PollThreadGroup::running_threads()
{
	if (pollThreads == NULL)
		return;

	for (int i = 0; i < this->numThreads; i++)
	{
		pollThreads[i]->running_thread();
	}
}

int PollThreadGroup::get_poll_threadIndex(PollThread *thread)
{
	for (int i = 0; i < this->numThreads; i++)
	{
		if (thread == pollThreads[i])
			return i;
	}
	return -1;
}

int PollThreadGroup::get_poll_threadSize()
{
	return numThreads;
}
