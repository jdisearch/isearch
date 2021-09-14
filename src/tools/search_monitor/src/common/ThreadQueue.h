/**
 * 线程安全队列
 * july 16, 2019
 *  created by qiuyu
 *
 */

#ifndef __THREADmQueueStorage_H_
#define __THREADmQueueStorage_H_

#include <deque>
#include <vector>
#include <cassert>
#include "ThreadMonitor.h"

template<typename T, typename D = std::deque<T> >
class ThreadQueue : protected ThreadLock
{
public:
  ThreadQueue()
  :
  mQueueSize(0)
  {
  };

public:
  typedef D QueueType_t;

  /**
   * @brief 从头部获取数据, 没有数据则等待.
   *
   * @param t 
   * @param millsecond   阻塞等待时间(ms) 
   *                    0 表示不阻塞 
   *                      -1 永久等待
   * @return bool: true, 获取了数据, false, 无数据
   */
  bool pop_front(T& t, size_t millsecond = 0);

  /**
   * @brief 通知等待在队列上面的线程都醒过来
   */
  void notifyT();

  /**
   * @brief 放数据到队列后端. 
   *  
   * @param t
   */
  void push_back(const T& t);

  /**
   * @brief  放数据到队列后端. 
   *  
   * @param vt
   */
  void push_back(const QueueType_t &qt);

  /**
   * @brief  放数据到队列前端. 
   *  
   * @param t
   */
  void push_front(const T& t);

  /**
   * @brief  放数据到队列前端. 
   *  
   * @param vt
   */
  void push_front(const QueueType_t &qt);

  /**
   * @brief  等到有数据才交换. 
   *  
   * @param q
   * @param millsecond  阻塞等待时间(ms) 
   *                   0 表示不阻塞 
   *                      -1 如果为则永久等待
   * @return 有数据返回true, 无数据返回false
   */
  bool swap(QueueType_t &q, size_t millsecond = 0);

  /**
   * @brief  队列大小.
   *
   * @return size_t 队列大小
   */
  size_t size() const;

  /**
   * @brief  清空队列
   */
  void clear();

  /**
   * @brief  是否数据为空.
   *
   * @return bool 为空返回true，否则返回false
   */
  bool empty() const;

protected:
  /**
   * 队列
   */
  QueueType_t          mQueueStorage;

  /**
   * 队列长度
   */
  size_t              mQueueSize;
};

template<typename T, typename D> 
bool ThreadQueue<T, D>::pop_front(T& t, size_t millsecond)
{
  Lock_t lock(*this);

  if (mQueueStorage.empty())
  {
    if(millsecond == 0)
    {
      return false;
    }
    if(millsecond == (size_t)-1)
    {
      wait();
    }
    else
    {
      //超时了
      if(!timedWait(millsecond))
      {
        return false;
      }
    }
  }

  if (mQueueStorage.empty())
  {
    return false;
  }

  t = mQueueStorage.front();
  mQueueStorage.pop_front();
  assert(mQueueSize > 0);
  --mQueueSize;

  return true;
}

template<typename T, typename D>
void ThreadQueue<T, D>::notifyT()
{
  Lock_t lock(*this);
  notifyAll();
}

template<typename T, typename D>
void ThreadQueue<T, D>::push_back(const T& t)
{
  Lock_t lock(*this);

  // 唤醒等待消费的线程
  notify();

  // request入队
  mQueueStorage.push_back(t);
  ++mQueueSize;
}

template<typename T, typename D>
void ThreadQueue<T, D>::push_back(const QueueType_t &qt)
{
  Lock_t lock(*this);

  typename QueueType_t::const_iterator it = qt.begin();
  typename QueueType_t::const_iterator itEnd = qt.end();
  while(it != itEnd)
  {
    mQueueStorage.push_back(*it);
    ++it;
    ++mQueueSize;
    notify();
  }
}

template<typename T, typename D>
void ThreadQueue<T, D>::push_front(const T& t)
{
  Lock_t lock(*this);

  notify();

  mQueueStorage.push_front(t);

  ++mQueueSize;
}

template<typename T, typename D>
void ThreadQueue<T, D>::push_front(const QueueType_t &qt)
{
  Lock_t lock(*this);

  typename QueueType_t::const_iterator it = qt.begin();
  typename QueueType_t::const_iterator itEnd = qt.end();
  while(it != itEnd)
  {
    mQueueStorage.push_front(*it);
    ++it;
    ++mQueueSize;

    notify();
  }
}

template<typename T, typename D>
bool ThreadQueue<T, D>::swap(QueueType_t &q, size_t millsecond)
{
  Lock_t lock(*this);

  if (mQueueStorage.empty())
  {
    if(millsecond == 0)
    {
      return false;
    }
    if(millsecond == (size_t)-1)
    {
      wait();
    }
    else
    {
      //超时了
      if(!timedWait(millsecond))
      {
        return false;
      }
    }
  }

  if (mQueueStorage.empty())
  {
    return false;
  }

  q.swap(mQueueStorage);
  //mQueueSize = q.size();
  mQueueSize = mQueueStorage.size();

  return true;
}

template<typename T, typename D>
size_t ThreadQueue<T, D>::size() const
{
  Lock_t lock(*this);
  //return mQueueStorage.size();
  return mQueueSize;
}

template<typename T, typename D>
void ThreadQueue<T, D>::clear()
{
  Lock_t lock(*this);
  mQueueStorage.clear();
  mQueueSize = 0;
}

template<typename T, typename D>
bool ThreadQueue<T, D>::empty() const
{
  Lock_t lock(*this);
  return mQueueStorage.empty();
}
#endif

