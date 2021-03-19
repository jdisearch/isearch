/*
 * =====================================================================================
 *
 *       Filename:  plugin_mgr.cc
 *
 *    Description:  plugin management.
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
#include <dlfcn.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "plugin_listener_pool.h"
#include "plugin_worker.h"
#include "plugin_mgr.h"
#include "singleton.h"
#include "mem_check.h"
#include "log.h"

extern DTCConfig *gConfig;
typedef void (*so_set_network_mode_t)(void);
typedef void (*so_set_strings_t)(const char *);

PluginManager::PluginManager(void) : _dll(NULL),
                                     _config(NULL),
                                     _plugin_name(NULL),
                                     _plugin_listener_pool(NULL),
                                     _worker_notifier(NULL),
                                     _worker_number(1),
                                     _sb_if_handle(NULL),
                                     _plugin_timer(NULL)
{
}

PluginManager::~PluginManager(void)
{
}

PluginManager *PluginManager::Instance(void)
{
    return Singleton<PluginManager>::Instance();
}

void PluginManager::Destroy(void)
{
    return Singleton<PluginManager>::Destroy();
}

int PluginManager::open(int mode)
{
    //create worker notifier
    NEW(worker_notify_t, _worker_notifier);
    if (NULL == _worker_notifier)
    {
        log_error("create worker notifier failed, msg:%s", strerror(errno));
        return -1;
    }

    if (load_plugin_depend(mode) != 0)
    {
        return -1;
    }

    //create dll info
    _dll = (dll_func_t *)MALLOC(sizeof(dll_func_t));
    if (NULL == _dll)
    {
        log_error("malloc dll_func_t object failed, msg:%s", strerror(errno));
        return -1;
    }
    memset(_dll, 0x00, sizeof(dll_func_t));

    _plugin_name = gConfig->get_str_val("cache", "PluginName");
    if (NULL == _plugin_name)
    {
        log_error("PluginName[%p] is invalid", _plugin_name);
        return -1;
    }

    //get plugin config file
    PluginGlobal::_plugin_conf_file = gConfig->get_str_val("cache", "PluginConfigFile");
    if (NULL == PluginGlobal::_plugin_conf_file)
    {
        log_info("PluginConfigFile[%p] is invalid", PluginGlobal::_plugin_conf_file);
    }

    if (register_plugin(_plugin_name) != 0)
    {
        log_error("register plugin[%s] failed", _plugin_name);
        return -1;
    }

    //invoke handle_init
    if (NULL != _dll->handle_init)
    {
        const char *plugin_config_file[2] = {(const char *)PluginGlobal::_plugin_conf_file, NULL};
        if (_dll->handle_init(1, (char **)plugin_config_file, PROC_MAIN) != 0)
        {
            log_error("invoke plugin[%s]::handle_init() failed", _plugin_name);
            return -1;
        }
    }

    //crate PluginListenerPool object
    NEW(PluginListenerPool, _plugin_listener_pool);
    if (NULL == _plugin_listener_pool)
    {
        log_error("create PluginListenerPool object failed, msg:%s", strerror(errno));
        return -1;
    }
    if (_plugin_listener_pool->Bind(gConfig) != 0)
    {
        return -1;
    }

    //create worker pool
    if (create_worker_pool(gConfig->get_int_val("cache", "PluginWorkerNumber")) != 0)
    {
        log_error("create plugin worker pool failed.");
        return -1;
    }

    //create plugin timer notify
    if (_dll->handle_timer_notify)
    {
        if (create_plugin_timer(gConfig->get_int_val("cache", "PluginTimerNotifyInterval", 10)) != 0)
        {
            log_error("create usr timer notify failed.");
            return -1;
        }
    }

    return 0;
}

int PluginManager::close(void)
{
    //stop plugin timer
    if (_plugin_timer)
    {
        _plugin_timer->interrupt();
        DELETE(_plugin_timer);
        _plugin_timer = NULL;
    }

    //stop worker
    _worker_notifier->Stop(PLUGIN_STOP_REQUEST);

    //destroy worker
    for (int i = 0; i < _worker_number; ++i)
    {
        if (NULL != _plugin_worker_pool[i])
        {
            _plugin_worker_pool[i]->interrupt();
        }

        DELETE(_plugin_worker_pool[i]);
    }
    FREE(_plugin_worker_pool);
    _plugin_worker_pool = NULL;

    //delete plugin listener pool
    DELETE(_plugin_listener_pool);

    //unregister plugin
    if (NULL != _dll->handle_fini)
    {
        _dll->handle_fini(PROC_MAIN);
    }
    unregister_plugin();
    FREE_CLEAR(_dll);

    //destroy work notifier
    DELETE(_worker_notifier);

    return 0;
}

int PluginManager::load_plugin_depend(int mode)
{
    char *error = NULL;
    so_set_network_mode_t so_set_network_mode = NULL;
    so_set_strings_t so_set_server_address = NULL;
    so_set_strings_t so_set_server_tablename = NULL;

    //load server bench so
    void *sb_if_handle = dlopen(SERVER_BENCH_SO_FILE, RTLD_NOW | RTLD_GLOBAL);
    if ((error = dlerror()) != NULL)
    {
        log_error("so file[%s] dlopen error, %s", SERVER_BENCH_SO_FILE, error);
        return -1;
    }

    //load dtc api so
    void *dtcapi_handle = dlopen(DTC_API_SO_FILE, RTLD_NOW | RTLD_GLOBAL);
    if ((error = dlerror()) != NULL)
    {
        log_error("so file[%s] dlopen error, %s", DTC_API_SO_FILE, error);
        return -1;
    }

    //init plugin logger
    const char *plugin_log_path = gConfig->get_str_val("cache", "PluginLogPath");
    if (NULL == plugin_log_path)
    {
        plugin_log_path = (const char *)"../log";
    }
    const char *plugin_log_name = gConfig->get_str_val("cache", "PluginLogName");
    if (NULL == plugin_log_name)
    {
        plugin_log_name = (const char *)"plugin_";
    }
    int plugin_log_level = gConfig->get_idx_val("cache", "LogLevel", ((const char *const[]){"emerg", "alert", "crit", "error", "warning", "notice", "info", "debug", NULL}), 6);
    int plugin_log_size = gConfig->get_int_val("cache", "PluginLogSize", 1 << 28);

    DLFUNC(sb_if_handle, _sb_if_handle, plugin_log_init_t, "log_init");
    if (_sb_if_handle(plugin_log_path, plugin_log_level, plugin_log_size, plugin_log_name) != 0)
    {
        log_error("init plugin logger failed.");
        return -1;
    }

    //set_network_mode
    DLFUNC(dtcapi_handle, so_set_network_mode, so_set_network_mode_t, "set_network_mode");
    if (so_set_network_mode && mode)
    {
        so_set_network_mode();
    }

    //set_server_address
    DLFUNC(dtcapi_handle, so_set_server_address, so_set_strings_t, "set_server_address");
    if (so_set_server_address)
    {
        log_debug("set server address:%s", gConfig->get_str_val("cache", "BindAddr"));
        so_set_server_address(gConfig->get_str_val("cache", "BindAddr"));
    }
    //set_server_tablename
    DLFUNC(dtcapi_handle, so_set_server_tablename, so_set_strings_t, "set_server_tablename");
    if (so_set_server_tablename)
    {
        if (!gConfig->get_str_val("TABLE_DEFINE", "table_name"))
        {
            log_error("can't find tablename in table.conf");
            return 0;
        }
        log_debug("set server tablename:%s", gConfig->get_str_val("TABLE_DEFINE", "table_name"));
        so_set_server_tablename(gConfig->get_str_val("TABLE_DEFINE", "table_name"));
    }

    return 0;

out:
    return -1;
}

int PluginManager::register_plugin(const char *file_name)
{
    char *error = NULL;
    int ret_code = -1;

    _dll->handle = dlopen(file_name, RTLD_NOW);
    if ((error = dlerror()) != NULL)
    {
        log_error("dlopen error, %s", error);
        goto out;
    }

    DLFUNC_NO_ERROR(_dll->handle, _dll->handle_init, handle_init_t, "handle_init");
    DLFUNC_NO_ERROR(_dll->handle, _dll->handle_fini, handle_fini_t, "handle_fini");
    DLFUNC_NO_ERROR(_dll->handle, _dll->handle_open, handle_open_t, "handle_open");
    DLFUNC_NO_ERROR(_dll->handle, _dll->handle_close, handle_close_t, "handle_close");
    DLFUNC_NO_ERROR(_dll->handle, _dll->handle_timer_notify, handle_timer_notify_t, "handle_timer_notify");
    DLFUNC(_dll->handle, _dll->handle_input, handle_input_t, "handle_input");
    DLFUNC(_dll->handle, _dll->handle_process, handle_process_t, "handle_process");

    ret_code = 0;

out:
    if (0 == ret_code)
    {
        log_info("open plugin:%s successful.", file_name);
    }
    else
    {
        log_info("open plugin:%s failed.", file_name);
    }

    return ret_code;
}

void PluginManager::unregister_plugin(void)
{
    if (NULL != _dll)
    {
        if (NULL != _dll->handle)
        {
            dlclose(_dll->handle);
        }

        memset(_dll, 0x00, sizeof(dll_func_t));
    }

    return;
}

int PluginManager::create_worker_pool(int worker_number)
{
    _worker_number = worker_number;

    if (_worker_number < 1 || _worker_number > PluginGlobal::_max_worker_number)
    {
        log_warning("worker number[%d] is invalid, default[%d]", _worker_number, PluginGlobal::_default_worker_number);
        _worker_number = PluginGlobal::_default_worker_number;
    }

    _plugin_worker_pool = (Thread **)calloc(_worker_number, sizeof(Thread *));
    if (NULL == _plugin_worker_pool)
    {
        log_error("calloc worker pool failed, msg:%s", strerror(errno));
        return -1;
    }

    for (int i = 0; i < _worker_number; ++i)
    {
        char name[32];
        snprintf(name, sizeof(name) - 1, "plugin_worker_%d", i);

        NEW(PluginWorkerThread(name, _worker_notifier, i), _plugin_worker_pool[i]);
        if (NULL == _plugin_worker_pool[i])
        {
            log_error("create %s failed, msg:%s", name, strerror(errno));
            return -1;
        }

        if (_plugin_worker_pool[i]->initialize_thread() != 0)
        {
            log_error("%s initialize failed.", name);
            return -1;
        }

        _plugin_worker_pool[i]->running_thread();
    }

    return 0;
}

int PluginManager::create_plugin_timer(const int interval)
{
    char name[32] = {'\0'};
    int plugin_timer_interval = 0;

    plugin_timer_interval = (interval < 1) ? 1 : interval;
    snprintf(name, sizeof(name) - 1, "%s", "plugin_timer");

    NEW(PluginTimer(name, plugin_timer_interval), _plugin_timer);
    if (NULL == _plugin_timer)
    {
        log_error("create %s failed, msg:%s", name, strerror(errno));
        return -1;
    }

    if (_plugin_timer->initialize_thread() != 0)
    {
        log_error("%s initialize failed.", name);
        return -1;
    }

    _plugin_timer->running_thread();

    return 0;
}
