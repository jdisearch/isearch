#ifndef __H_DTC_PLUGIN_UNIT_H__
#define __H_DTC_PLUGIN_UNIT_H__

#include "stat_dtc.h"
#include "decoder_base.h"
#include "plugin_request.h"
#include "poll_thread.h"

class PluginDecoderUnit : public DecoderUnit
{
public:
    PluginDecoderUnit(PollThread *owner, int idletimeout);
    virtual ~PluginDecoderUnit();

    virtual int process_stream(int fd, int req, void *peer, int peerSize);
    virtual int process_dgram(int fd);

    inline void record_request_time(unsigned int msec)
    {
    }

    inline incoming_notify_t *get_incoming_notifier(void)
    {
        return &_incoming_notify;
    }

    inline int attach_incoming_notifier(void)
    {
        return _incoming_notify.attach_poller(owner);
    }

private:
    incoming_notify_t _incoming_notify;
};

#endif
