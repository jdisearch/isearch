/*
 * =====================================================================================
 *
 *       Filename:  lock.h
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
#ifndef __DTC_LOCK_H__
#define __DTC_LOCK_H__

#include <pthread.h>
#include "namespace.h"

DTC_BEGIN_NAMESPACE

class Mutex
{
    friend class condition;

public:
    inline Mutex(void)
    {
        ::pthread_mutex_init(&_mutex, 0);
    }

    inline void lock(void)
    {
        ::pthread_mutex_lock(&_mutex);
    }

    inline void unlock(void)
    {
        ::pthread_mutex_unlock(&_mutex);
    }

    inline ~Mutex(void)
    {
        ::pthread_mutex_destroy(&_mutex);
    }

private:
    Mutex(const Mutex &m);
    Mutex &operator=(const Mutex &m);

private:
    pthread_mutex_t _mutex;
};

/**
 * *    definition of ScopedLock;
 * **/
class ScopedLock
{
    friend class condition;

public:
    inline ScopedLock(Mutex &mutex) : _mutex(mutex)
    {
        _mutex.lock();
    }

    inline ~ScopedLock(void)
    {
        _mutex.unlock();
    }

private:
    Mutex &_mutex;
};

DTC_END_NAMESPACE

#endif //__DTC_LOCK_H__
