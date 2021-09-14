/**
 * Non-joinable thread implemention
 * Jul 16, 2019
 *      By qiuyu  
 */

#ifndef __THREAD_H_
#define __THREAD_H_

#include "ThreadMonitor.h"

#include <sys/types.h>
#include <signal.h>
#include <pthread.h>

/**
 * 线程基类，所有自定义线程继承于该类，同时实现run接口即可, 
 */
class Thread
{
protected:
  enum ThreadState
  {
    eIdle,
    eRunning,
    eStopped, // not support now
    eExited
  };

public:
    Thread();
    virtual ~Thread(){};

    // 线程运行
    bool start();

    // 线程是否存活.
    bool isAlive() const;

    // 获取线程id.
    pthread_t id() { return mThreadId; }

protected:

    // 静态函数, 线程入口. 
    static void threadEntry(Thread *pThread);

    // 运行
    virtual void run() = 0;

protected:
    // bool            mIsRunning;
    ThreadState     mThreadState;
    pthread_t       mThreadId;

    // 线程锁
    ThreadLock   mThreadLock;
};
#endif // __THREAD_H_
