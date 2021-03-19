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
#include <memcheck.h>
#include <errno.h>
#include <signal.h>
#include "log.h"
#include "StatManagerContainerThread.h"


CStatManagerContainerThread *CStatManagerContainerThread::getInstance()
{
	static CStatManagerContainerThread msStatThread;

	return &msStatThread;
}

CStatManagerContainerThread::CStatManagerContainerThread()
{
	pthread_mutex_init(&wakeLock, NULL);
}

CStatManagerContainerThread::~CStatManagerContainerThread()
{
	//log_debug("the ~CStatManagerContainerThread been called when killall!");
	std::map<uint32_t, CStatManager *>::iterator iter = m_statManagers.begin();
	std::map<uint32_t, CStatManager *>::const_iterator endIter = m_statManagers.end();

	for ( ; iter != endIter; ++iter)
	{
		iter->second->clear();
	}
}

void *CStatManagerContainerThread::__threadentry(void *p)
{
	CStatManagerContainerThread *my = (CStatManagerContainerThread *)p;
	my->ThreadLoop();
	return NULL;
}

int CStatManagerContainerThread::StartBackgroundThread(void)
{
	P __a(this);

	if(pthread_mutex_trylock(&wakeLock) == 0)
	{
		int ret = pthread_create(&threadid, 0, __threadentry, (void *)this);
		if(ret != 0)
		{
			errno = ret;
			return -1;
		}
	}
	return 0;
}

int CStatManagerContainerThread::StopBackgroundThread(void)
{
	P __a(this);

	if(pthread_mutex_trylock(&wakeLock) == 0)
	{
		pthread_mutex_unlock(&wakeLock);
		return 0;
	}
	pthread_mutex_unlock(&wakeLock);
	int ret = pthread_join(threadid, 0);
	if(ret==0)
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

void CStatManagerContainerThread::ThreadLoop(void)
{
	stat_set_log_thread_name_("stat");
	log_info("thread stat[%d] started.", gettid());

	BlockAllSignals();

	time_t next = 0;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	next = (tv.tv_sec / 10) * 10 + 10;
	struct timespec ts;
	ts.tv_sec = next;
	ts.tv_nsec = 0;

	while(pthread_mutex_timedlock(&wakeLock, &ts)!=0)
	{
		gettimeofday(&tv, NULL);
		if(tv.tv_sec >= next)
		{
			mLock.lock();
			std::map<uint32_t, CStatManager *>::iterator iter = m_statManagers.begin();
			std::map<uint32_t, CStatManager *>::const_iterator endIter = m_statManagers.end();

			for ( ; iter != endIter; ++iter)
			{
				iter->second->RunJobOnce();
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

int CStatManagerContainerThread::AddStatManager(uint32_t moudleId, CStatManager *stat)
{
	CScopedLock autoLock(mLock);
	if(m_statManagers.find(moudleId) != m_statManagers.end())
	        return -1;

	m_statManagers[moudleId] = stat;
	return 0;
}

int CStatManagerContainerThread::DeleteStatManager(uint32_t moudleId)
{
	CScopedLock autoLock(mLock);
	std::map<uint32_t, CStatManager *>::iterator i = m_statManagers.find(moudleId);
	if(i != m_statManagers.end())
	{
		delete i->second;
		m_statManagers.erase(i);

		return 0;
	}
	return -1;
}

CStatItemU32 CStatManagerContainerThread::operator()(int moduleId,int cId)
{
	CStatItemU32 dummy;
	std::map<uint32_t, CStatManager *>::iterator i = m_statManagers.find(moduleId);
	if(i != m_statManagers.end())
	{
		return i->second->GetItemU32(cId);
	}
	return dummy;
}
