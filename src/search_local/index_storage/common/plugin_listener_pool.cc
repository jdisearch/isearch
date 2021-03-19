#include <stdlib.h>

#include "plugin_listener_pool.h"
#include "listener.h"
#include "poll_thread.h"
#include "plugin_unit.h"
#include "poll_thread.h"
#include "unix_socket.h"
#include "config.h"
#include "log.h"
#include "mem_check.h"

PluginListenerPool::PluginListenerPool(void)
{
    memset(_listener, 0, sizeof(_listener));
    memset(_thread, 0, sizeof(_thread));
    memset(_decoder, 0, sizeof(_decoder));
    memset(_udpfd, 0xff, sizeof(_udpfd));
}

PluginListenerPool::~PluginListenerPool(void)
{
    for (int i = 0; i < MAXLISTENERS; i++)
    {
        if (_thread[i])
        {
            _thread[i]->interrupt();
        }

        DELETE(_listener[i]);
        DELETE(_decoder[i]);

        if (_udpfd[i] >= 0)
        {
            close(_udpfd[i]);
        }
    }
}

int PluginListenerPool::init_decoder(int n, int idle)
{
    if (_thread[n] == NULL)
    {
        char name[16];
        snprintf(name, sizeof(name) - 1, "plugin_inc%d", n);

        _thread[n] = NULL;
        NEW(PollThread(name), _thread[n]);
        if (NULL == _thread[n])
        {
            log_error("create PollThread object failed, %m");
            return -1;
        }
        if (_thread[n]->initialize_thread() == -1)
        {
            return -1;
        }

        _decoder[n] = NULL;
        NEW(PluginDecoderUnit(_thread[n], idle), _decoder[n]);
        if (NULL == _decoder[n])
        {
            log_error("create PluginDecoderUnit object failed, %m");
            return -1;
        }

        if (_decoder[n]->attach_incoming_notifier() != 0)
        {
            log_error("attach incoming notifier failed.");
            return -1;
        }
    }

    return 0;
}

int PluginListenerPool::Bind(DTCConfig *gc)
{
    int succ_count = 0;

    int idle = gc->get_int_val("cache", "idle_timeout", 100);
    if (idle < 0)
    {
        log_notice("idle_timeout invalid, use default value: 100");
        idle = 100;
    }

    int single = gc->get_int_val("cache", "SingleIncomingThread", 0);
    int backlog = gc->get_int_val("cache", "MaxListenCount", 256);
    int win = gc->get_int_val("cache", "MaxRequestWindow", 0);

    for (int i = 0; i < MAXLISTENERS; i++)
    {
        const char *errmsg = NULL;
        char bindStr[32];
        char bindPort[32];
        int rbufsz = 0;
        int wbufsz = 0;

        if (i == 0)
        {
            snprintf(bindStr, sizeof(bindStr) - 1, "PluginAddr");
            snprintf(bindPort, sizeof(bindPort) - 1, "PluginPort");
        }
        else
        {
            snprintf(bindStr, sizeof(bindStr) - 1, "PluginAddr%d", i);
            snprintf(bindPort, sizeof(bindPort) - 1, "PluginPort%d", i);
        }

        const char *addrStr = gc->get_str_val("cache", bindStr);
        if (addrStr == NULL)
        {
            continue;
        }

        errmsg = _sockaddr[i].set_address(addrStr, gc->get_str_val("cache", bindPort));
        if (errmsg)
        {
            log_error("bad BindAddr%d/BindPort%d: %s\n", i, i, errmsg);
            continue;
        }

        int n = single ? 0 : i;
        if (_sockaddr[i].socket_type() == SOCK_DGRAM)
        { // DGRAM listener
            rbufsz = gc->get_int_val("cache", "UdpRecvBufferSize", 0);
            wbufsz = gc->get_int_val("cache", "UdpSendBufferSize", 0);
        }
        else
        {
            // STREAM socket listener
            rbufsz = wbufsz = 0;
        }

        _listener[i] = new DTCListener(&_sockaddr[i]);
        _listener[i]->set_request_window(win);
        if (_listener[i]->Bind(backlog, rbufsz, wbufsz) != 0)
        {
            if (i == 0)
            {
                log_crit("Error bind unix-socket");
                return -1;
            }
            else
            {
                continue;
            }
        }

        if (init_decoder(n, idle) != 0)
        {
            return -1;
        }

        if (_listener[i]->Attach(_decoder[n], backlog) < 0)
        {
            return -1;
        }

        //inc succ count
        succ_count++;
    }

    for (int i = 0; i < MAXLISTENERS; i++)
    {
        if (_thread[i] == NULL)
        {
            continue;
        }

        _thread[i]->running_thread();
    }

    if (0 == succ_count)
    {
        log_error("all plugin bind address & port invalid.");
        return -1;
    }

    return 0;
}

int PluginListenerPool::Match(const char *name, int port)
{
    for (int i = 0; i < MAXLISTENERS; i++)
    {
        if (_listener[i] == NULL)
        {
            continue;
        }

        if (_sockaddr[i].Match(name, port))
        {
            return 1;
        }
    }

    return 0;
}

int PluginListenerPool::Match(const char *name, const char *port)
{
    for (int i = 0; i < MAXLISTENERS; i++)
    {
        if (_listener[i] == NULL)
        {
            continue;
        }

        if (_sockaddr[i].Match(name, port))
        {
            return 1;
        }
    }

    return 0;
}
