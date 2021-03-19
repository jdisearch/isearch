/*
 * =====================================================================================
 *
 *       Filename:  plugin_request.h
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
#ifndef __DTC_PLUGIN_REQUESET_H__
#define __DTC_PLUGIN_REQUESET_H__

#include "value.h"
#include "plugin_global.h"
#include "mtpqueue.h"
#include "wait_queue.h"
#include "mem_check.h"
#include "stop_watch.h"

template <typename T>
class PluginIncomingNotify : public ThreadingPipeQueue<T *, PluginIncomingNotify<T> >
{
public:
    PluginIncomingNotify(void)
    {
    }

    virtual ~PluginIncomingNotify(void)
    {
    }

    void task_notify(T *p)
    {
        if (p)
        {
            p->task_notify();
        }

        return;
    }
};

enum
{
    PLUGIN_REQ_SUCC = 1,
    PLUGIN_REQ_MULTI = 2,
};

class PluginSync;
class PluginDgram;
class PluginRequest;

typedef PluginRequest plugin_request_t;
typedef PluginIncomingNotify<plugin_request_t> incoming_notify_t;
typedef threading_wait_queue<plugin_request_t *> worker_notify_t;

class PluginRequest
{
public:
    PluginRequest(void) : _recv_buf(NULL),
                          _recv_len(0),
                          _send_buf(0),
                          _send_len(0),
                          _sent_len(0),
                          _real_len(0),
                          _incoming_notifier(NULL),
                          _flags(0)
    {
    }

    virtual ~PluginRequest(void)
    {
    }

    inline void set_time_info(void)
    {
        struct timeval now;
        gettimeofday(&now, NULL);

        _response_timer = (int)(now.tv_sec * 1000000ULL + now.tv_usec);
        _skinfo.recvtm = now.tv_sec;
        _skinfo.tasktm = now.tv_sec;
    }

    inline void mark_handle_succ(void)
    {
        _flags |= PLUGIN_REQ_SUCC;
    }

    inline void mark_handle_fail(void)
    {
        _flags &= ~PLUGIN_REQ_SUCC;
    }

    inline int handle_succ(void)
    {
        return (_flags & PLUGIN_REQ_SUCC);
    }

    inline int recv_only(void)
    {
        return (_skinfo.flags & PLUGIN_RECV_ONLY);
    }

    inline int disconnect(void)
    {
        return (_skinfo.flags & PLUGIN_DISCONNECT);
    }

    virtual int handle_process(void) = 0;
    virtual int task_notify(void) = 0;

public:
    char *_recv_buf;
    int _recv_len;

    char *_send_buf;
    int _send_len;
    int _sent_len;

    int _real_len;

    incoming_notify_t *_incoming_notifier;
    stopwatch_usec_t _response_timer;
    skinfo_t _skinfo;

protected:
    /* 预留标志,按照bit操作         */
    /* 0:  handle_process 执行失败  */
    /* 1:  handle_process 执行成功  */
    /* 2:  粘包请求                 */
    uint64_t _flags;
};

class PluginStream : public PluginRequest, private noncopyable
{
public: //methods
    PluginStream(PluginSync *sync, dll_func_t *dll) : _plugin_sync(sync),
                                                      _dll(dll),
                                                      _all_len(0),
                                                      _recv_remain_len(0)
    {
    }

    virtual ~PluginStream(void)
    {
        release_buffer();
        _incoming_notifier = NULL;
        _plugin_sync = NULL;
        _dll = NULL;
    }

    inline void recalc_multipacket(void)
    {
        const int max_recv_len = PluginGlobal::_max_plugin_recv_len;

        FREE_CLEAR(_send_buf);
        _send_len = 0;
        _sent_len = 0;

        _all_len = max_recv_len;
        _recv_remain_len = max_recv_len;
        _recv_len -= _real_len;
        memmove(_recv_buf, _recv_buf + _real_len, _recv_len);
        _real_len = 0;
        mark_single_packet();

        return;
    }

    inline void release_buffer(void)
    {
        FREE_CLEAR(_recv_buf);
        FREE_CLEAR(_send_buf);

        _recv_len = 0;
        _send_len = 0;
        _sent_len = 0;
        _real_len = 0;
        _all_len = 0;
        _recv_remain_len = 0;
    }

    inline void mark_multi_packet(void)
    {
        _flags |= PLUGIN_REQ_MULTI;
    }

    inline void mark_single_packet(void)
    {
        _flags &= ~PLUGIN_REQ_MULTI;
    }

    inline int multi_packet(void)
    {
        return (_flags & PLUGIN_REQ_MULTI);
    }

    virtual int handle_process(void);
    virtual int task_notify(void);

public: //property
    PluginSync *_plugin_sync;
    dll_func_t *_dll;

    int _all_len;
    int _recv_remain_len;
};

class PluginDatagram : public PluginRequest, private noncopyable
{
public: //methods
    PluginDatagram(PluginDgram *dgram, dll_func_t *dll) : _addr(NULL),
                                                          _addr_len(0),
                                                          _plugin_dgram(dgram),
                                                          _dll(dll)
    {
    }

    virtual ~PluginDatagram(void)
    {
        release_buffer();
        _incoming_notifier = NULL;
        _plugin_dgram = NULL;
        _dll = NULL;
    }

    inline void release_buffer(void)
    {
        FREE_CLEAR(_recv_buf);
        _recv_len = 0;

        FREE_CLEAR(_send_buf);
        _send_len = 0;
        _sent_len = 0;

        _real_len = 0;

        FREE_CLEAR(_addr);
        _addr_len = 0;
    }

    virtual int handle_process(void);
    virtual int task_notify(void);

public: //property
    void *_addr;
    socklen_t _addr_len;
    PluginDgram *_plugin_dgram;
    dll_func_t *_dll;
};

#endif
