
#include "TimeProvider.h"

ThreadLock TimeProvider::g_tl;
TimeProviderPtr TimeProvider::g_tp = NULL;

TimeProvider* TimeProvider::getInstance()
{
    if(!g_tp)
    {
        ThreadLock::Lock_t lock(g_tl);

        if(!g_tp)
        {
            g_tp = new TimeProvider();

            g_tp->start();
        }
    }
    return g_tp.get();
}

TimeProvider::~TimeProvider() 
{ 
    {
        ThreadLock::Lock_t lock(g_tl);
        mTerminate = true; 
        g_tl.notify(); 
    }
    
    pthread_join(mThreadId, NULL);
}

void TimeProvider::getNow(timeval *tv)  
{ 
    int idx = mBufIdx;
    *tv = mTimeVal[idx];

    if(mCpuCycle != 0 && mUseTsc)     //cpu-cycle在两个interval周期后采集完成
    {    
        addTimeOffset(*tv,idx); 
    }
    else
    {
        ::gettimeofday(tv, NULL);
    }
}

int64_t TimeProvider::getNowMs()
{
    struct timeval tv;
    getNow(&tv);
    return tv.tv_sec * (int64_t)1000 + tv.tv_usec/1000;
}

void TimeProvider::run()
{
    while(!mTerminate)
    {
        timeval& tt = mTimeVal[!mBufIdx];

        ::gettimeofday(&tt, NULL);

        setTsc(tt);  

        mBufIdx = !mBufIdx;

        ThreadLock::Lock_t lock(g_tl);

        g_tl.timedWait(800); //修改800时 需对应修改addTimeOffset中offset判读值

    }
}

float TimeProvider::cpuMHz()
{
    if(mCpuCycle != 0)
        return 1.0/mCpuCycle;

    return 0;
}

void TimeProvider::setTsc(timeval& tt)
{
    uint32_t low    = 0;
    uint32_t high   = 0;
    rdtsc(low,high);
    uint64_t current_tsc    = ((uint64_t)high << 32) | low;

    uint64_t& last_tsc      = mTimeSc[!mBufIdx];
    timeval& last_tt        = mTimeVal[mBufIdx];

    if(mTimeSc[mBufIdx] == 0 || mTimeSc[!mBufIdx] == 0 )
    {
        mCpuCycle      = 0;
        last_tsc       = current_tsc;
    }
    else
    {
        time_t sptime   = (tt.tv_sec -  last_tt.tv_sec)*1000*1000 + (tt.tv_usec - last_tt.tv_usec);  
        mCpuCycle      = (float)sptime/(current_tsc - mTimeSc[mBufIdx]); //us 
        last_tsc        = current_tsc;
    } 
}

void TimeProvider::addTimeOffset(timeval& tt,const int &idx)
{
    uint32_t low    = 0;
    uint32_t high   = 0;
    rdtsc(low,high);
    uint64_t current_tsc = ((uint64_t)high << 32) | low;
    int64_t t =  (int64_t)(current_tsc - mTimeSc[idx]);
    time_t offset =  (time_t)(t*mCpuCycle);
    if(t < -1000 || offset > 1000000)//毫秒级别
    {
        mUseTsc = false;
        ::gettimeofday(&tt, NULL);
        return;
    }
    tt.tv_usec += offset;
    while (tt.tv_usec >= 1000000) { tt.tv_usec -= 1000000; tt.tv_sec++;} 
}
