#ifndef __PLUGIN__DGRAM_H__
#define __PLUGIN__DGRAM_H__

#include <sys/socket.h>

#include "poller.h"
#include "timer_list.h"
#include "plugin_decoder.h"
#include "plugin_unit.h"

class PluginDatagram;

class PluginDgram : public PollerObject
{
public:
    PluginDgram(PluginDecoderUnit *, int fd);
    virtual ~PluginDgram();

    virtual int Attach(void);
    inline int send_response(PluginDatagram *plugin_request)
    {
        int ret = 0;
        _owner->record_request_time(plugin_request->_response_timer.live());

        if (!plugin_request->recv_only())
        {
            ret = _plugin_sender.sendto(plugin_request);
        }

        DELETE(plugin_request);
        return ret;
    }

private:
    virtual void input_notify(void);

protected:
    int recv_request(void);
    int init_request(void);

private:
    int mtu;
    int _addr_len;
    PluginDecoderUnit *_owner;
    worker_notify_t *_worker_notifier;
    PluginReceiver _plugin_receiver;
    PluginSender _plugin_sender;
    uint32_t _local_ip;

private:
    int init_socket_info(void);
};

#endif
