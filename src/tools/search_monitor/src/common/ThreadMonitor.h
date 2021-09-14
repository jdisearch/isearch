/**
 * 线程锁监控模板类:
 * 通常线程锁，都通过该类来使用，而不是直接用ThreadMutex、ThreadRecMutex 
 * 该类将ThreadMutex/ThreadRecMutex 与ThreadCond结合起来 
 *
 * july 16, 2019
 *  created by qiuyu
 *
 */

#ifndef _ThreadMonitor_H_
#define _ThreadMonitor_H_

#include "ThreadMutex.h"
#include "ThreadCond.h"

template <class T, class P>
class ThreadMonitor
{
public:
  /**
   * @brief 定义锁控制对象
   */
  typedef Lock<ThreadMonitor<T, P> > Lock_t;
  typedef TryLock<ThreadMonitor<T, P> > TryLock_t;

  /**
   * @brief 构造函数
   */
  ThreadMonitor()
  :
  mNotifyNum(0)
  {
  }

  /**
   * @brief 析够
   */
  virtual ~ThreadMonitor()
  {
  }

  /**
   * @brief 锁
   */
  void lock() const
  {
    mThreadMutex.lock();
    mNotifyNum = 0;
  }

  /**
   * @brief 解锁, 根据上锁的次数通知
   */
  void unlock() const
  {
    // 详见notify()，根据notify次数来signal
    notifyImpl(mNotifyNum);
    mThreadMutex.unlock();
  }

  /**
   * @brief 尝试锁.
   *
   * @return bool
   */
  bool tryLock() const
  {
    bool result = mThreadMutex.tryLock();
    if(result)
    {
      // 加锁成功
      mNotifyNum = 0;
    }
    return result;
  }

  /**
   * @brief 等待,当前调用线程在锁上等待，直到事件通知，
   */
  void wait() const
  {
    notifyImpl(mNotifyNum);

    // try
    // {
      mThreadCond.wait(mThreadMutex);
    // }
    // catch(...)
    // {
    //   mNotifyNum = 0;
    //   throw;
    // }

    mNotifyNum = 0;
  }

  /**
   * @brief 等待时间,当前调用线程在锁上等待，直到超时或有事件通知
   *  
   * @param millsecond 等待时间
   * @return           false:超时了, ture:有事件来了
   */
  bool timedWait(int millsecond) const
  {
    notifyImpl(mNotifyNum);

    bool rc;

    // try
    // {
      rc = mThreadCond.timedWait(mThreadMutex, millsecond);
    // }
    // catch(...)
    // {
    //   mNotifyNum = 0;
    //   throw;
    // }

    mNotifyNum = 0;
    return rc;
  }

  /**
   * @brief 通知某一个线程醒来 
   *  
   * 通知等待在该锁上某一个线程醒过来 ,调用该函数之前必须加锁, 
   *  
   * 在解锁的时候才真正通知 
   */
  /*调用此函数前必须加锁，notify函数只是改变mNotifyNum值，而在LOCK此锁析构时，会调用unlock函数，这时候才真的会调用signal函数*/
  void notify()
  {
    if(mNotifyNum != -1)
    {
      ++mNotifyNum;
    }
  }

  /**
   * @brief 通知等待在该锁上的所有线程醒过来，
   * 注意调用该函数时必须已经获得锁.
   *  
   * 该函数调用前之必须加锁, 在解锁的时候才真正通知 
   */
  void notifyAll()
  {
    mNotifyNum = -1;
  }

protected:
  /**
   * @brief 通知实现. 
   *  
   * @param nnotify 上锁的次数
   */
  void notifyImpl(int nnotify) const
  {
    if(nnotify != 0)
    {
      // notifyAll
      if(nnotify == -1)
      {
        mThreadCond.broadcast();
        return;
      }
      else
      {
        // 按照上锁次数进行通知
        while(nnotify > 0)
        {
          mThreadCond.signal();
          --nnotify;
        }
      }
    }
  }

private:
  /** 
   * @brief noncopyable
   */
  ThreadMonitor(const ThreadMonitor&);
  void operator=(const ThreadMonitor&);

protected:
  /**
   * 上锁的次数
   */
  mutable int     mNotifyNum;
  mutable P       mThreadCond;
  T               mThreadMutex;
};

/**
 * 普通线程锁
 */
typedef ThreadMonitor<ThreadMutex, ThreadCond> ThreadLock;

/**
 * 循环锁(一个线程可以加多次锁)
 */
typedef ThreadMonitor<ThreadRecMutex, ThreadCond> ThreadRecLock;

#endif
