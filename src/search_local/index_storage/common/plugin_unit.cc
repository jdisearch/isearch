#include <errno.h>
#include <unistd.h>

#include "plugin_unit.h"
#include "plugin_sync.h"
#include "plugin_dgram.h"
#include "log.h"
#include "mem_check.h"

PluginDecoderUnit::PluginDecoderUnit(PollThread *o, int it) : DecoderUnit(o, it)
{
}

PluginDecoderUnit::~PluginDecoderUnit()
{
}

int PluginDecoderUnit::process_stream(int newfd, int req, void *peer, int peerSize)
{
    PluginSync *plugin_client = NULL;
    NEW(PluginSync(this, newfd, peer, peerSize), plugin_client);

    if (0 == plugin_client)
    {
        log_error("create PluginSync object failed, errno[%d], msg[%m]", errno);
        return -1;
    }

    if (plugin_client->Attach() == -1)
    {
        log_error("Invoke PluginSync::Attach() failed");
        delete plugin_client;
        return -1;
    }

    /* accept唤醒后立即recv */
    plugin_client->input_notify();

    return 0;
}

int PluginDecoderUnit::process_dgram(int newfd)
{
    PluginDgram *plugin_dgram = NULL;
    NEW(PluginDgram(this, newfd), plugin_dgram);

    if (0 == plugin_dgram)
    {
        log_error("create PluginDgram object failed, errno[%d], msg[%m]", errno);
        return -1;
    }

    if (plugin_dgram->Attach() == -1)
    {
        log_error("Invoke PluginDgram::Attach() failed");
        delete plugin_dgram;
        return -1;
    }

    return 0;
}
