/*
 * =====================================================================================
 *
 *       Filename:  plugin_request.cc
 *
 *    Description:  
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

#include "plugin_request.h"
#include "plugin_sync.h"
#include "plugin_dgram.h"
#include "log.h"
#include "plugin_mgr.h"

int PluginStream::handle_process(void)
{
    if (0 != _dll->handle_process(_recv_buf, _real_len, &_send_buf, &_send_len, &_skinfo))
    {
        mark_handle_fail();
        log_error("invoke handle_process failed, worker[%d]", _gettid_());
    }
    else
    {
        mark_handle_succ();
    }

    if (_incoming_notifier->Push(this) != 0)
    {
        log_error("push plugin request to incoming failed, worker[%d]", _gettid_());
        delete this;
        return -1;
    }

    return 0;
}

int PluginDatagram::handle_process(void)
{
    if (0 != _dll->handle_process(_recv_buf, _real_len, &_send_buf, &_send_len, &_skinfo))
    {
        mark_handle_fail();
        log_error("invoke handle_process failed, worker[%d]", _gettid_());
    }
    else
    {
        mark_handle_succ();
    }

    if (_incoming_notifier->Push(this) != 0)
    {
        log_error("push plugin request to incoming failed, worker[%d]", _gettid_());
        delete this;
        return -1;
    }

    return 0;
}

int PluginStream::task_notify(void)
{
    if (disconnect())
    {
        DELETE(_plugin_sync);
        delete this;
        return -1;
    }

    if (!handle_succ())
    {
        DELETE(_plugin_sync);
        delete this;
        return -1;
    }

    if (NULL == _plugin_sync)
    {
        delete this;
        return -1;
    }

    _plugin_sync->set_stage(PLUGIN_SEND);
    _plugin_sync->send_response();

    return 0;
}

int PluginDatagram::task_notify(void)
{
    if (!handle_succ())
    {
        delete this;
        return -1;
    }

    if (NULL == _plugin_dgram)
    {
        delete this;
        return -1;
    }

    _plugin_dgram->send_response(this);

    return 0;
}
