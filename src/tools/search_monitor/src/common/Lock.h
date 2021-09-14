/**
 * 锁模板类:
 * 1.此类非具体锁对象，具体锁类型又模板参数决定
 * 2.需要加锁的类，仅需继承ThreadMutex、ThreadRecMutex、ThreadLock、ThreadRecLock
 *   等类，即可实现锁，ThreadMutex和ThreadLock是不可重入锁，而ThreadRecMutex和
 *   ThreadRecLock是可重入锁；ThreadMutex和ThreadRecMutex是单纯的互斥锁，ThreadLock
 *   和ThreadRecLock是互斥锁和条件变量的结合体，可以用来做线程同步和通信
 * Jul 16, 2019
 *      By qiuyu  
 */

#ifndef __LOCK_H_
#define __LOCK_H_

#include "log.h"

#include <string>
#include <stdexcept>
#include <cerrno>

/**
 * @brief  锁模板类其他具体锁配合使用，
 * 构造时候加锁，析够的时候解锁
 */
template <typename T>
class Lock
{
public:
  /**
   * @brief  构造函数，构造时枷锁
   * @param mutex 锁对象
   */
  Lock(const T& mutex)
  :
  mMutex(mutex)
  {
    mMutex.lock();
    mAcquired = true;
  }

  /**
   * @brief  析构，析构时解锁
   */
  virtual ~Lock()
  {
    if (mAcquired)
    {
      mMutex.unlock();
    }
  }

  /**
   * @brief  上锁, 如果已经上锁,则抛出异常
   */
  bool acquire() const
  {
    if (mAcquired)
    {
      log_error("lock has beed locked!");
      return false;
    }
    mMutex.lock();
    mAcquired = true;
    return true;
  }

  /**
   * @brief  尝试上锁.
   */
  bool tryAcquire() const
  {
    mAcquired = mMutex.tryLock();
    return mAcquired;
  }

  /**
   * @brief  释放锁, 如果没有上过锁, 则抛出异常
   */
  bool release() const
  {
    if (!mAcquired)
    {
      log_error("thread hasn't been locked!");
      return false;
    }

    mMutex.unlock();
    mAcquired = false;
  }

  /**
   * @brief  是否已经上锁.
   * @return  返回true已经上锁，否则返回false
   */
  bool acquired() const
  {
    return mAcquired;
  }

protected:
  /**
   * @brief 构造函数
   * 用于锁尝试操作，与Lock相似
   */
  Lock(const T& mutex, bool)
  :
  mMutex(mutex)
  {
    mAcquired = mMutex.tryLock();
  }

private:
  // Not implemented; prevents accidental use.
  Lock(const Lock&);
  Lock& operator=(const Lock&);

  protected:
  /**
   * 锁对象
   */
  const T&        mMutex;

  /**
   * 是否已经上锁
   */
  mutable bool mAcquired;
};

/**
 * @brief  尝试上锁
 */
template <typename T>
class TryLock : public Lock<T>
{
public:
  TryLock(const T& mutex)
  :
  Lock<T>(mutex, true)
  {
  }
};

/**
 * @brief  空锁, 不做任何锁动作
 */
class EmptyMutex
{
public:
  /**
  * @brief  写锁.
  * @return int, 0 正确
  */
  int lock() const { return 0; }

  /**
   * @brief  解写锁
   */
  int unlock() const { return 0; }

  /**
   * @brief  尝试解锁. 
   * @return int, 0 正确
   */
  bool trylock() const { return true; }
};

/**
 * @brief  读写锁读锁模板类
 * 构造时候加锁，析够的时候解锁
 */

template <typename T>
class RW_RLock
{
public:
  /**
   * @brief  构造函数，构造时枷锁
   * @param lock 锁对象
   */
  RW_RLock(T& lock)
  :
  mRWLock(lock),
  mAcquired(false)
  {
    mRWLock.ReadLock();
    mAcquired = true;
  }

  /**
   * @brief 析构时解锁
   */
  ~RW_RLock()
  {
    if (mAcquired)
    {
      mRWLock.Unlock();
    }
  }
private:
  /**
   *锁对象
   */
  const T& mRWLock;

  /**
   * 是否已经上锁
   */
  mutable bool mAcquired;

  RW_RLock(const RW_RLock&);
  RW_RLock& operator=(const RW_RLock&);
};

template <typename T>
class RW_WLock
{
public:
  /**
   * @brief  构造函数，构造时枷锁
   * @param lock 锁对象
   */
  RW_WLock(T& lock)
  :
  mRWLock(lock),
  mAcquired(false)
  {
    mRWLock.WriteLock();
    mAcquired = true;
  }

  /**
   * @brief 析构时解锁
   */
  ~RW_WLock()
  {
    if(mAcquired)
    {
      mRWLock.Unlock();
    }
  }

private:
  /**
   *锁对象
   */
  const T& mRWLock;
  /**
   * 是否已经上锁
   */
  mutable bool mAcquired;

  RW_WLock(const RW_WLock&);
  RW_WLock& operator=(const RW_WLock&);
};
#endif // __LOCK_H_
