/**
 * @file TimeProvider.h
 * @brief 秒级、微妙级时间提供类. 
 */

#ifndef __TIME_PROVIDER_H_
#define __TIME_PROVIDER_H_

#include <string>
#include <string.h>
#include "ThreadMonitor.h"
#include "Thread.h"
#include "AutoPtr.h"

#define rdtsc(low,high) \
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#define TNOW     TimeProvider::getInstance()->getNow()
#define TNOWMS   TimeProvider::getInstance()->getNowMs()

class TimeProvider;

typedef AutoPtr<TimeProvider> TimeProviderPtr;

/**
 * @brief 提供秒级别的时间
 */
class TimeProvider : public Thread, public HandleBase
{
public:

    /**
     * @brief 获取实例. 
     *  
     * @return TimeProvider&
     */
    static TimeProvider* getInstance();

    /**
     * @brief 构造函数
     */
    TimeProvider()
    :
    mTerminate(false),
    mUseTsc(true),
    mCpuCycle(0),
    mBufIdx(0)
    {
        memset(mTimeVal, 0, sizeof(mTimeVal));
        memset(mTimeSc, 0, sizeof(mTimeSc));

        struct timeval tv;
        ::gettimeofday(&tv, NULL);
        mTimeVal[0] = tv;
        mTimeVal[1] = tv;
    }

    /**
     * @brief 析构，停止线程
     */
    ~TimeProvider(); 

    /**
     * @brief 获取时间.
     *
     * @return time_t 当前时间
     */
    time_t getNow() {  return mTimeVal[mBufIdx].tv_sec; }

    /**
     * @brief 获取时间.
     *
     * @para timeval 
     * @return void 
     */
    void getNow(timeval * tv);

    /**
     * @brief 获取ms时间.
     *
     * @para timeval 
     * @return void 
     */
    int64_t getNowMs();
    
    /**
     * @brief 获取cpu主频.
     *  
     * @return float cpu主频
     */  

    float cpuMHz();

    /**
     * @brief 运行
     */
protected:

    virtual void run();

    static ThreadLock        g_tl;

    static TimeProviderPtr   g_tp;

private:
    void setTsc(timeval& tt);

    void addTimeOffset(timeval& tt,const int &idx);

protected:

    bool    mTerminate;

    bool    mUseTsc;

private:
    float           mCpuCycle; 

    volatile int    mBufIdx;

    timeval         mTimeVal[2];

    uint64_t        mTimeSc[2];  
};
#endif
