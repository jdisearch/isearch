/**
 * july 16, 2019
 *  created by qiuyu
 * 
 */

#include "ThreadCond.h"
#include <string.h>
#include <cassert>
#include <iostream>
#include <stdint.h>

using namespace std;


ThreadCond::ThreadCond()
{
    int rc;

    pthread_condattr_t attr;

    rc = pthread_condattr_init(&attr);
    if(rc != 0)
    {
      log_error("pthread_condattr_init error:%d", errno);
    }

    rc = pthread_cond_init(&mCond, &attr);
    if(rc != 0)
    {
      log_error("pthread_cond_init error:%d", errno);
    }

    rc = pthread_condattr_destroy(&attr);
    if(rc != 0)
    {
      log_error("pthread_condattr_destroy error:%d", errno);
    }
}

ThreadCond::~ThreadCond()
{
    int rc = 0;
    rc = pthread_cond_destroy(&mCond);
    if(rc != 0)
    {
       log_error("destroy cond error:%s", string(strerror(rc)).c_str());
    }
}

void ThreadCond::signal()
{
    int rc = pthread_cond_signal(&mCond);
    if(rc != 0)
    {
        log_error("pthread_cond_signal error:%d", errno);
    }
}

void ThreadCond::broadcast()
{
    int rc = pthread_cond_broadcast(&mCond);
    if(rc != 0)
    {
        log_error("pthread_cond_broadcast error:%d", errno);
    }
}

timespec ThreadCond::abstime(int millsecond) const
{
    struct timeval tv;

    gettimeofday(&tv, 0);
    // TC_TimeProvider::getInstance()->getNow(&tv);

    int64_t it  = tv.tv_sec * (int64_t)1000000 + tv.tv_usec + (int64_t)millsecond * 1000;

    tv.tv_sec   = it / (int64_t)1000000;
    tv.tv_usec  = it % (int64_t)1000000;

    timespec ts;
    ts.tv_sec   = tv.tv_sec;
    ts.tv_nsec  = tv.tv_usec * 1000; 
      
    return ts; 
}
