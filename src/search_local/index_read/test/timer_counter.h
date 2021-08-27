#ifndef TIMER_COUNTER_H_
#define TIMER_COUNTER_H_

#include <sys/time.h>
#include "../comm/log.h"

#define PERFORMANCE_TEST_SWITCH  1

#define TIME_INIT   Timer timer;

#define TIME_START                     \
    if(PERFORMANCE_TEST_SWITCH){                                    \
    timer.Start();                                                  \
    };

#define TIME_END                      \
    if(PERFORMANCE_TEST_SWITCH){                                    \
        log_error("costtime:%d us",timer.Stop());                   \
    };

class Timer{
public:
    Timer() {};
    ~Timer(){};
public:
    static Timer* GetInstance(){
        static Timer timer;
        return &timer;
    };
public:
    void Start(){
        gettimeofday(&tm_start_ , NULL);
    };

    int Stop(){
        timeval tm_stop;
        gettimeofday(&tm_stop , NULL);

        return ((tm_stop.tv_sec - tm_start_.tv_sec)*1000000   \
            + tm_stop.tv_usec - tm_start_.tv_usec);
    };
private:
    timeval tm_start_;
};
#endif