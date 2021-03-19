/*
 * =====================================================================================
 *
 *       Filename:  plugin_mgr.h
 *
 *    Description:  plugin manager
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __DTC_PLUGIN_MANAGER_H__
#define __DTC_PLUGIN_MANAGER_H__

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "non_copyable.h"
#include "plugin_request.h"
#include "plugin_timer.h"

class DTCConfig;
class PluginListenerPool;
class PluginWorkerThread;

class PluginManager : private noncopyable
{
public:
    PluginManager(void);
    ~PluginManager(void);

    static PluginManager *Instance(void);
    static void Destroy(void);

    int open(int mode);
    int close(void);

    inline dll_func_t *get_dll(void)
    {
        return _dll;
    }

    inline worker_notify_t *get_worker_notifier(void)
    {
        return _worker_notifier;
    }

public:
protected:
protected:
private:
    int register_plugin(const char *file_name);
    void unregister_plugin(void);
    int create_worker_pool(int worker_number);
    int load_plugin_depend(int mode);
    int create_plugin_timer(const int interval);

private:
    dll_func_t *_dll;
    DTCConfig *_config;
    const char *_plugin_name;
    PluginListenerPool *_plugin_listener_pool;
    Thread **_plugin_worker_pool;
    worker_notify_t *_worker_notifier;
    int _worker_number;
    plugin_log_init_t _sb_if_handle;
    Thread *_plugin_timer;
};

#endif
