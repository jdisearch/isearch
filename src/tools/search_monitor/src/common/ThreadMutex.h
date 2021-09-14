/**
 *
 * july 16, 2019
 *  created by qiuyu
 *
 */

#ifndef __THREAD_MUTEX_H_
#define __THREAD_MUTEX_H_

#include "Lock.h"

/////////////////////////////////////////////////
class ThreadCond;

/**
* @brief 线程锁 . 
*  
* 不可重复加锁，即同一个线程不可以重复加锁 
*  
* 通常不直接使用，和Monitor配合使用，即ThreadLock; 
*/
class ThreadMutex
{
public:
  ThreadMutex();
  virtual ~ThreadMutex();

  /**
   * @brief 加锁
   */
  void lock() const;

  /**
   * @brief 尝试锁
   */
  bool tryLock() const;

  /**
   * @brief 解锁
   */
  void unlock() const;

  /**
   * @brief 加锁后调用unlock是否会解锁，给Monitor使用的 永远返回true
   */
  bool willUnlock() const { return true;}

protected:
  // noncopyable
  ThreadMutex(const ThreadMutex&);
  void operator=(const ThreadMutex&);

  /**
   * @brief 计数
   */
  int count() const;

  /**
   * @brief 计数
   */
  void count(int c) const;

  friend class ThreadCond;

protected:
  mutable pthread_mutex_t mMutex;
};

/**
* @brief 线程锁类. 
* 采用线程库实现
**/
class ThreadRecMutex
{
public:
  ThreadRecMutex();
  virtual ~ThreadRecMutex();

  int lock() const;
  int unlock() const;
  bool tryLock() const;

  /**
   * @brief 加锁后调用unlock是否会解锁, 给TC_Monitor使用的
   */
  bool willUnlock() const;

protected:
  friend class ThreadCond;

  /**
   * @brief 计数
   */
  int count() const;
  void count(int c) const;

private:
  /**
    锁对象
    */
  mutable pthread_mutex_t mMutex;
  mutable int mCount;
};
#endif
