#include <stdio.h>
#include <sys/un.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>

#include "plugin_agent_mgr.h"
#include "plugin_sync.h"
#include "plugin_unit.h"
#include "poll_thread.h"
#include "log.h"
#include "stat_dtc.h"
#include "mem_check.h"

//static int statEnable=0;
static StatItemU32 statPluginAcceptCount;
static StatItemU32 statPluginCurConnCount;
extern "C"
{
    extern unsigned int get_local_ip();
}

PluginSync::PluginSync(PluginDecoderUnit *plugin_decoder, int fd, void *peer, int peer_size) : PollerObject(plugin_decoder->owner_thread(), fd),
                                                                                               _plugin_stage(PLUGIN_IDLE),
                                                                                               owner(plugin_decoder),
                                                                                               _plugin_request(NULL),
                                                                                               _worker_notifier(NULL),
                                                                                               _plugin_receiver(fd, PluginAgentManager::Instance()->get_dll()),
                                                                                               _plugin_sender(fd, PluginAgentManager::Instance()->get_dll())
{
    _addr_len = peer_size;
    _addr = MALLOC(peer_size);
    memcpy((char *)_addr, (char *)peer, peer_size);

    ++statPluginAcceptCount;
    ++statPluginCurConnCount;
}

PluginSync::~PluginSync(void)
{
    --statPluginCurConnCount;

    if (PLUGIN_PROC == _plugin_stage)
    {
        _plugin_request->_plugin_sync = NULL;
    }
    else
    {
        DELETE(_plugin_request);
    }

    FREE_CLEAR(_addr);
}

int PluginSync::create_request(void)
{
    if (NULL != _plugin_request)
    {
        DELETE(_plugin_request);
        _plugin_request = NULL;
    }

    NEW(PluginStream(this, PluginAgentManager::Instance()->get_dll()), _plugin_request);
    if (NULL == _plugin_request)
    {
        log_error("create plugin request object failed, msg:%s", strerror(errno));
        return -1;
    }

    //set plugin request info
    _plugin_request->_skinfo.sockfd = netfd;
    _plugin_request->_skinfo.type = SOCK_STREAM;
    _plugin_request->_skinfo.local_ip = get_local_ip();
    _plugin_request->_skinfo.local_port = 0;
    _plugin_request->_skinfo.remote_ip = ((struct sockaddr_in *)_addr)->sin_addr.s_addr;
    _plugin_request->_skinfo.remote_port = ((struct sockaddr_in *)_addr)->sin_port;
    _plugin_request->_incoming_notifier = owner->get_incoming_notifier();

    return 0;
}

int PluginSync::Attach()
{
    if (create_request() != 0)
    {
        log_error("create request object failed");
        return -1;
    }

    //get worker notifier
    _worker_notifier = PluginAgentManager::Instance()->get_worker_notifier();
    if (NULL == _worker_notifier)
    {
        log_error("get worker notifier failed.");
        return -1;
    }

    enable_input();

    if (attach_poller() == -1)
    {
        return -1;
    }

    attach_timer(owner->idle_list());
    _plugin_stage = PLUGIN_IDLE;

    return 0;
}

int PluginSync::recv_request()
{
    disable_timer();

    int ret_stage = _plugin_receiver.recv(_plugin_request);

    switch (ret_stage)
    {
    default:
    case NET_FATAL_ERROR:
        return -1;

    case NET_IDLE:
        attach_timer(owner->idle_list());
        _plugin_stage = PLUGIN_IDLE;
        break;

    case NET_RECVING:
        //如果收到部分包，则需要加入idle list, 防止该连接挂死
        attach_timer(owner->idle_list());
        _plugin_stage = PLUGIN_RECV;
        break;

    case NET_RECV_DONE:
        _plugin_request->set_time_info();
        if (_worker_notifier->Push(_plugin_request) != 0)
        {
            log_error("push plugin request failed, fd[%d]", netfd);
            return -1;
        }
        _plugin_stage = PLUGIN_PROC;
        break;
    }

    return 0;
}

int PluginSync::proc_multi_request(void)
{
    _plugin_receiver.set_stage(NET_IDLE);
    switch (_plugin_receiver.proc_remain_packet(_plugin_request))
    {
    default:
    case NET_FATAL_ERROR:
        return -1;

    case NET_RECVING:
        _plugin_stage = PLUGIN_RECV;
        _plugin_sender.set_stage(NET_IDLE);
        _plugin_receiver.set_stage(NET_RECVING);
        break;

    case NET_RECV_DONE:
        _plugin_request->set_time_info();
        if (_worker_notifier->Push(_plugin_request) != 0)
        {
            log_error("push plugin request failed, fd[%d]", netfd);
            return -1;
        }
        _plugin_stage = PLUGIN_PROC;
        break;
    }

    return 0;
}

int PluginSync::Response(void)
{
    if (_plugin_request->recv_only())
    {
        goto proc_multi;
    }

    switch (_plugin_sender.send(_plugin_request))
    {
    default:
    case NET_FATAL_ERROR:
        return -1;

    case NET_SENDING:
        _plugin_stage = PLUGIN_SEND;
        enable_output();
        return 0;

    case NET_SEND_DONE:
        break;
    }

proc_multi:
    //multi request process logic
    if (_plugin_request->multi_packet())
    {
        if (proc_multi_request() != 0)
        {
            return -1;
        }
    }
    else
    {
        _plugin_request->release_buffer();
        _plugin_sender.set_stage(NET_IDLE);
        _plugin_receiver.set_stage(NET_IDLE);
        _plugin_stage = PLUGIN_IDLE;
    }

    //防止多一次output事件触发
    disable_output();
    enable_input();
    attach_timer(owner->idle_list());

    return 0;
}

void PluginSync::input_notify(void)
{
    log_debug("enter input_notify!");
    if (_plugin_stage == PLUGIN_IDLE || _plugin_stage == PLUGIN_RECV)
    {
        if (recv_request() < 0)
        {
            delete this;
            return;
        }
    }
    else /* receive input events again. */
    {
        /*  check whether client close connection. */
        if (check_link_status())
        {
            log_debug("client close connection, delete PluginSync obj, plugin stage=%d", _plugin_stage);
            delete this;
            return;
        }
        else
        {
            DisableInput();
        }
    }

    apply_events();

    return;
}

void PluginSync::output_notify(void)
{
    if (_plugin_stage == PLUGIN_SEND)
    {
        if (Response() < 0)
        {
            delete this;
        }
    }
    else
    {
        disable_output();
        log_info("Spurious PluginSync::output_notify, plugin stage=%d", _plugin_stage);
    }

    return;
}
