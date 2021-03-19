#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sched.h>

#include "plugin_agent_mgr.h"
#include "plugin_timer.h"
#include "plugin_global.h"
#include "log.h"

PluginTimer::PluginTimer(const char *name, const int interval) : Thread(name, Thread::ThreadTypeSync), _interval(interval)
{
    pthread_mutex_init(&_wake_lock, NULL);
}

PluginTimer::~PluginTimer()
{
}

int PluginTimer::Initialize(void)
{
    _dll = PluginAgentManager::Instance()->get_dll();
    if ((NULL == _dll) || (NULL == _dll->handle_timer_notify))
    {
        log_error("get server bench handle failed.");
        return -1;
    }

    return pthread_mutex_trylock(&_wake_lock) == 0 ? 0 : -1;
}

void PluginTimer::interrupt(void)
{
    pthread_mutex_unlock(&_wake_lock);

    return Thread::interrupt();
}

static void BlockAllSignals(void)
{
    sigset_t sset;
    sigfillset(&sset);
    sigdelset(&sset, SIGSEGV);
    sigdelset(&sset, SIGBUS);
    sigdelset(&sset, SIGABRT);
    sigdelset(&sset, SIGILL);
    sigdelset(&sset, SIGCHLD);
    sigdelset(&sset, SIGFPE);
    pthread_sigmask(SIG_BLOCK, &sset, &sset);
}

void *PluginTimer::Process(void)
{
    BlockAllSignals();

    struct timeval tv;
    struct timespec ts;
    time_t nextsec = 0;
    unsigned long long nextnsec = 0;
    int ret_value = 0;

#define ONESEC_NSEC 1000000000ULL
#define ONESEC_MSEC 1000ULL
    gettimeofday(&tv, NULL);
    nextsec = tv.tv_sec;
    nextnsec = (tv.tv_usec + _interval * ONESEC_MSEC) * ONESEC_MSEC;
    while (nextnsec >= ONESEC_NSEC)
    {
        nextsec++;
        nextnsec -= ONESEC_NSEC;
    }

    ts.tv_sec = nextsec;
    ts.tv_nsec = nextnsec;

    while (!Stopping())
    {
        if (pthread_mutex_timedlock(&_wake_lock, &ts) == 0)
        {
            break;
        }

        ret_value = _dll->handle_timer_notify(0, NULL);
        if (0 != ret_value)
        {
            log_error("invoke handle_timer_notify failed, return value:%d timer notify[%d]", ret_value, _gettid_());
        }

        gettimeofday(&tv, NULL);

        nextsec = tv.tv_sec;
        nextnsec = (tv.tv_usec + _interval * ONESEC_MSEC) * ONESEC_MSEC;
        while (nextnsec >= ONESEC_NSEC)
        {
            nextsec++;
            nextnsec -= ONESEC_NSEC;
        }

        ts.tv_sec = nextsec;
        ts.tv_nsec = nextnsec;
    }

    pthread_mutex_unlock(&_wake_lock);

    return NULL;
}
