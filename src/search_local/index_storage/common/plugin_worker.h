#ifndef __PLUGIN_WORKER_THREAD_H__
#define __PLUGIN_WORKER_THREAD_H__

#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <linux/unistd.h>

#include "timestamp.h"
#include "thread.h"
#include "plugin_request.h"

class PluginWorkerThread : public Thread
{
public:
    PluginWorkerThread(const char *name, worker_notify_t *worker_notify, int seq_no);
    virtual ~PluginWorkerThread();

    inline int get_seq_no(void)
    {
        return _seq_no;
    }

protected:
    virtual void *Process(void);
    virtual int Initialize(void);
    virtual void Prepare(void);

private:
    worker_notify_t *_worker_notify;
    dll_func_t *_dll;
    int _seq_no;
};

#endif
