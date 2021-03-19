#ifndef __PLUGIN_SYNC_H__
#define __PLUGIN_SYNC_H__

#include "poller.h"
#include "timer_list.h"
#include "plugin_unit.h"
#include "plugin_decoder.h"

class PluginDecoderUnit;

typedef enum
{
    PLUGIN_IDLE,
    PLUGIN_RECV, //wait for recv request, server side
    PLUGIN_SEND, //wait for send response, server side
    PLUGIN_PROC  //IN processing
} plugin_state_t;

class PluginSync : public PollerObject, private TimerObject
{
public:
    PluginSync(PluginDecoderUnit *plugin_decoder, int fd, void *peer, int peer_size);
    virtual ~PluginSync();

    int Attach(void);
    virtual void input_notify(void);

    inline int send_response(void)
    {
        owner->record_request_time(_plugin_request->_response_timer.live());
        if (Response() < 0)
        {
            delete this;
        }
        else
        {
            delay_apply_events();
        }

        return 0;
    }

    inline void set_stage(plugin_state_t stage)
    {
        _plugin_stage = stage;
    }

private:
    virtual void output_notify(void);

protected:
    plugin_state_t _plugin_stage;

    int recv_request(void);
    int Response(void);

private:
    int create_request(void);
    int proc_multi_request(void);

private:
    PluginDecoderUnit *owner;
    PluginStream *_plugin_request;
    worker_notify_t *_worker_notifier;
    void *_addr;
    int _addr_len;
    PluginReceiver _plugin_receiver;
    PluginSender _plugin_sender;
};

#endif
