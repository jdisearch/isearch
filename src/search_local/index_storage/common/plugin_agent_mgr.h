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
class PluginAgentListenerPool;
class PluginWorkerThread;

class PluginAgentManager : private noncopyable
{
public:
    PluginAgentManager(void);
    ~PluginAgentManager(void);

    static PluginAgentManager *Instance(void);
    static void Destroy(void);

    int open(void);
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
    char *plugin_log_path;
    char *plugin_log_name;
    int plugin_log_level;
    int plugin_log_size;
    int plugin_worker_number;
    int plugin_timer_notify_interval;

    inline void set_plugin_name(const char *plugin_name) { _plugin_name = strdup(plugin_name); }
    inline void set_plugin_conf_file(const char *plugin_conf_file) { PluginGlobal::_plugin_conf_file = strdup(plugin_conf_file); }

protected:
protected:
private:
    int register_plugin(const char *file_name);
    void unregister_plugin(void);
    int create_worker_pool(int worker_number);
    int load_plugin_depend();
    int create_plugin_timer(const int interval);

private:
    dll_func_t *_dll;
    DTCConfig *_config;
    char *_plugin_name;
    PluginAgentListenerPool *_plugin_listener_pool;
    Thread **_plugin_worker_pool;
    worker_notify_t *_worker_notifier;
    int _worker_number;
    plugin_log_init_t _sb_if_handle;
    Thread *_plugin_timer;
};

#endif
