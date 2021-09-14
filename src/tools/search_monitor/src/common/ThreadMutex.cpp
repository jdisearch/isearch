/**
 *
 * july 16, 2019
 *  created by qiuyu
 *
 */
#include "ThreadMutex.h"
#include <string.h>
#include <iostream>
#include <cassert>

ThreadMutex::ThreadMutex()
{
  int rc;
  pthread_mutexattr_t attr;
  rc = pthread_mutexattr_init(&attr);
  assert(rc == 0);

  rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
  assert(rc == 0);

  rc = pthread_mutex_init(&mMutex, &attr);
  assert(rc == 0);

  rc = pthread_mutexattr_destroy(&attr);
  assert(rc == 0);

  log_info("construct mutex, this:%p", this);
}

ThreadMutex::~ThreadMutex()
{
  int rc = 0;
  rc = pthread_mutex_destroy(&mMutex);
  assert(rc == 0);
  
  log_info("deconstruct mutex, this:%p", this);
}

void ThreadMutex::lock() const
{
  int rc = pthread_mutex_lock(&mMutex);
  if(rc != 0)
  {
    if(rc == EDEADLK)
    {
      log_error("dead lock error. ret:%d", rc);
    }
    else
    {
      log_error("lock error. ret:%d", rc);
    }
  }

  return;
}

bool ThreadMutex::tryLock() const
{
  int rc = pthread_mutex_trylock(&mMutex);
  if (rc != 0 && rc != EBUSY)
  {
    if (rc == EDEADLK)
    {
      log_error("dead lock error. ret:%d", rc);
    }
    else
    {
      log_error("trylock error. ret:%d", rc);
    }

    return false;
  }

  return (rc == 0);
}

void ThreadMutex::unlock() const
{
  int rc = pthread_mutex_unlock(&mMutex);
  if(rc != 0)
  {
    log_error("unlock error. ret:%d", rc);
    // return false;
  }

  return;
}

int ThreadMutex::count() const
{
  return 0;
}

void ThreadMutex::count(int c) const
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThreadRecMutex::ThreadRecMutex()
:
mCount(0)
{
  int rc;
  pthread_mutexattr_t attr;
  
  rc = pthread_mutexattr_init(&attr);
  if(rc != 0) log_error("init error. ret:%d", rc);

  rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  if(rc != 0) log_error("settype error. ret:%d", rc);

  rc = pthread_mutex_init(&mMutex, &attr);
  if(rc != 0) log_error("init error. ret:%d", rc);

  rc = pthread_mutexattr_destroy(&attr);
  if(rc != 0) log_error("destroy error. ret:%d", rc);
}

ThreadRecMutex::~ThreadRecMutex()
{
  while (mCount)
  {
    unlock();
  }

  int rc = 0;
  rc = pthread_mutex_destroy(&mMutex);
  if(rc != 0) log_error("destroy error! ret:%d", rc);
}

int ThreadRecMutex::lock() const
{
  int rc = pthread_mutex_lock(&mMutex);
  if(rc != 0) log_error("lock error. ret:%d", rc);

  // 用户重复加锁的话，在这里自动释放一次锁，那么对于用户来说，还是仅需要一次解锁操作
  if(++mCount > 1)
  {
    rc = pthread_mutex_unlock(&mMutex);
    assert(rc == 0);
  }

  return rc;
}

int ThreadRecMutex::unlock() const
{
  if(--mCount == 0)
  {
    int rc = 0;
    rc = pthread_mutex_unlock(&mMutex);
    return rc;
  }

  return 0;
}

bool ThreadRecMutex::tryLock() const
{
  int rc = pthread_mutex_trylock(&mMutex);
  if(rc != 0 )
  {
    if(rc != EBUSY)
    {
      log_error("trylock error. ret:%d", rc);
      return false;
    }
  }
  else if(++mCount > 1)
  {
    rc = pthread_mutex_unlock(&mMutex);
    if(rc != 0)
    {
      log_error("unlock error. ret:%d", rc);
    }
  }

  return (rc == 0);
}

bool ThreadRecMutex::willUnlock() const
{
  return mCount == 1;
}

int ThreadRecMutex::count() const
{
  int c   = mCount;
  mCount  = 0;
  return c;
}

void ThreadRecMutex::count(int c) const
{
  mCount = c;
}
