/**
 * july 16, 2019
 *  created by qiuyu
 * 
 */

#ifndef __THREAD_COND_H_
#define __THREAD_COND_H_

#include "log.h"

#include <sys/time.h>
#include <cerrno>
#include <iostream>
#include <assert.h>

class ThreadMutex;

/**
 *  线程信号条件类, 所有锁可以在上面等待信号发生
 *  
 *  和ThreadMutex、ThreadRecMutex配合使用,
 *  
 *  通常不直接使用，而是使用ThreadLock/ThreadRecLock;
 */
class ThreadCond
{
public:
  ThreadCond();
  ~ThreadCond();

  /**
   *  发送信号, 等待在该条件上的一个线程会醒
   */
  void signal();

  /**
   *  等待在该条件的所有线程都会醒
   */
  void broadcast();

  /**
   *  获取绝对等待时间
   */
  timespec abstime(int millsecond) const;

  /**
   *  @brief 无限制等待.
   *  
   * @param M
   */
  template<typename Mutex>
  void wait(const Mutex& mutex) const
  {
    int c = mutex.count();
    int rc = pthread_cond_wait(&mCond, &mutex.mMutex);
    mutex.count(c);
    if(rc != 0)
    {
      log_error("pthread_cond_wait error:%d", errno);
    }
  }

  /**
   * @brief 等待时间. 
   * @return bool, false表示超时, true:表示有事件来了
   */
  template<typename Mutex>
  bool timedWait(const Mutex& mutex, int millsecond) const
  {
    int c = mutex.count();

    timespec ts = abstime(millsecond);

    int rc = pthread_cond_timedwait(&mCond, &mutex.mMutex, &ts);

    mutex.count(c);

    if(rc != 0)
    {
      if(rc != ETIMEDOUT)
      {
        log_error("pthread_cond_timedwait error:%d", errno);
      }
      return false;
    }

    return true;
  }

protected:
  // Not implemented; prevents accidental use.
  ThreadCond(const ThreadCond&);
  ThreadCond& operator=(const ThreadCond&);

private:
  /**
   * 线程条件
   */
  mutable pthread_cond_t mCond;
};
#endif
