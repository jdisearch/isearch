/*
 * =====================================================================================
 *
 *       Filename:  condition.h
 *
 *    Description:  conditional thread operation.
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
#ifndef __DTC_CONDITION_H__
#define __DTC_CONDITION_H__

#include "non_copyable.h"

class condition : private noncopyable
{
public:
	condition(void)
	{
		pthread_mutex_init(&m_lock, NULL);
		pthread_cond_init(&m_cond, NULL);
	}

	~condition(void)
	{
		pthread_mutex_destroy(&m_lock);
		pthread_cond_destroy(&m_cond);
	}

	void notify_one(void)
	{
		pthread_cond_signal(&m_cond);
	}
	void notify_all(void)
	{
		pthread_cond_broadcast(&m_cond);
	}

	void wait(void)
	{
		pthread_cond_wait(&m_cond, &m_lock);
	}

	void lock(void)
	{
		pthread_mutex_lock(&m_lock);
	}

	void unlock(void)
	{
		pthread_mutex_unlock(&m_lock);
	}

private:
	pthread_cond_t m_cond;
	pthread_mutex_t m_lock;
};

class copedEnterCritical
{
public:
	copedEnterCritical(condition &c) : m_cond(c)
	{
		m_cond.lock();
	}

	~copedEnterCritical(void)
	{
		m_cond.unlock();
	}

private:
	condition &m_cond;
};

class copedLeaveCritical
{
public:
	copedLeaveCritical(condition &c) : m_cond(c)
	{
		m_cond.unlock();
	}

	~copedLeaveCritical(void)
	{
		m_cond.lock();
	}

private:
	condition &m_cond;
};

#endif //__DTC_CONDITION_H__
