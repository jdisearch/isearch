/**
 * Non-joinable thread implemention
 * Jul 16, 2019
 *      By qiuyu  
 */

#include "Thread.h"
#include "log.h"

#include <cerrno>

void sleep(long millsecond)
{
    struct timespec ts;
    ts.tv_sec = millsecond / 1000;
    ts.tv_nsec = (millsecond % 1000)*1000000;
    nanosleep(&ts, 0);
}

Thread::Thread()
:
// mIsRunning(false),
mThreadState(Thread::eIdle),
mThreadId(-1)
{
}

void Thread::threadEntry(Thread *pThread)
{
  // log_error("start thread, id:%lu", pThread->mThreadId);

  // pThread->mIsRunning = true;
  pThread->mThreadState = Thread::eRunning;
  {
    ThreadLock::Lock_t sync(pThread->mThreadLock);
    // 唤醒调用线程
    // pThread->mThreadLock.notifyAll();
    pThread->mThreadLock.notify();
  }

  pThread->run();
  // pThread->mIsRunning = false;
  pThread->mThreadState = Thread::eExited;
}

bool Thread::start()
{
    // 加锁
    ThreadLock::Lock_t sync(mThreadLock);

    // if (mIsRunning)
    if (mThreadState != Thread::eIdle) 
    {
      log_error("thread is still runing! id:%lu, state:%d", mThreadId, mThreadState);
      return false;
    }

    int ret = pthread_create(&mThreadId,
                   0,
                   (void *(*)(void *))&threadEntry,
                   (void *)this);

    if (ret != 0)
    {
      log_error("start thread failed!");
      return false;
    }
    
    ret = pthread_detach(mThreadId);
    if (ret != 0)
    {
      log_error("detach thread failed, ret:%d", ret);
      return false;
    }

    // 在threadEntry中解锁
    mThreadLock.wait();
    // log_error("start thread successful! id:%lu", mThreadId);

    return true;
}

bool Thread::isAlive() const
{
    return mThreadState != Thread::eExited;
}
