/*
 * =====================================================================================
 *
 *       Filename:  mutexlock.h
 *
 *    Description:  mutex lock class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef _MUTEX_LOCK_H_
#define _MUTEX_LOCK_H_

#include <pthread.h>

class ReadLock {
public:
	explicit ReadLock( pthread_rwlock_t  *mu) : _Mutex(mu) {
		pthread_rwlock_rdlock(_Mutex);
	}
	~ReadLock() { pthread_rwlock_unlock(_Mutex);; }

private:
	pthread_rwlock_t *_Mutex;
};


class WriteLock {
public:
	explicit WriteLock( pthread_rwlock_t  *mu) : _Mutex(mu) {
		pthread_rwlock_wrlock(_Mutex);
	}
	~WriteLock() { pthread_rwlock_unlock(_Mutex);; }

private:
	pthread_rwlock_t *_Mutex;
};


#endif