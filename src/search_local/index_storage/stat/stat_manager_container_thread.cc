/*
 * =====================================================================================
 *
 *       Filename:  StatManagerContainerThread.cc
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  27/05/2014 08:50:19 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  linjinming (prudence), linjinming@jd.com
 *        Company:  JD, China
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
#include "stat_manager_container_thread.h"

StatManagerContainerThread *StatManagerContainerThread::getInstance()
{
	static StatManagerContainerThread msStatThread;

	return &msStatThread;
}

StatManagerContainerThread::StatManagerContainerThread()
{
	pthread_mutex_init(&wakeLock, NULL);
}

StatManagerContainerThread::~StatManagerContainerThread()
{
	//log_debug("the ~StatManagerContainerThread been called when killall!");
	std::map<uint32_t, StatManager *>::iterator iter = m_statManagers.begin();
	std::map<uint32_t, StatManager *>::const_iterator endIter = m_statManagers.end();

	for (; iter != endIter; ++iter)
	{
		iter->second->clear();
	}
}

void *StatManagerContainerThread::__threadentry(void *p)
{
	StatManagerContainerThread *my = (StatManagerContainerThread *)p;
	my->thread_loop();
	return NULL;
}

int StatManagerContainerThread::start_background_thread(void)
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

int StatManagerContainerThread::stop_background_thread(void)
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

void StatManagerContainerThread::thread_loop(void)
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
			mLock.lock();
			std::map<uint32_t, StatManager *>::iterator iter = m_statManagers.begin();
			std::map<uint32_t, StatManager *>::const_iterator endIter = m_statManagers.end();

			for (; iter != endIter; ++iter)
			{
				iter->second->run_job_once();
			}
			mLock.unlock();

			gettimeofday(&tv, NULL);
			next = (tv.tv_sec / 10) * 10 + 10;
		}
		ts.tv_sec = next;
		ts.tv_nsec = 0;
	}
	pthread_mutex_unlock(&wakeLock);
}

int StatManagerContainerThread::add_stat_manager(uint32_t moudleId, StatManager *stat)
{
	ScopedLock autoLock(mLock);
	if (m_statManagers.find(moudleId) != m_statManagers.end())
		return -1;

	m_statManagers[moudleId] = stat;
	return 0;
}

int StatManagerContainerThread::delete_stat_manager(uint32_t moudleId)
{
	ScopedLock autoLock(mLock);
	std::map<uint32_t, StatManager *>::iterator i = m_statManagers.find(moudleId);
	if (i != m_statManagers.end())
	{
		delete i->second;
		m_statManagers.erase(i);

		return 0;
	}
	return -1;
}

StatItemU32 StatManagerContainerThread::operator()(int moduleId, int cId)
{
	StatItemU32 dummy;
	std::map<uint32_t, StatManager *>::iterator i = m_statManagers.find(moduleId);
	if (i != m_statManagers.end())
	{
		return i->second->get_item_u32(cId);
	}
	return dummy;
}
