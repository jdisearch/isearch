#ifndef __PLUGIN_LISTENER_POOL_H__
#define __PLUGIN_LISTENER_POOL_H__

#include "daemon.h"
#include "plugin_unit.h"
#include "net_addr.h"

class DTCListener;
class PollThread;

class PluginListenerPool
{
public:
	PluginListenerPool();
	~PluginListenerPool();
	int Bind(DTCConfig *gc);

	int Match(const char *, int = 0);
	int Match(const char *, const char *);

private:
	SocketAddress _sockaddr[MAXLISTENERS];
	DTCListener *_listener[MAXLISTENERS];
	PollThread *_thread[MAXLISTENERS];
	PluginDecoderUnit *_decoder[MAXLISTENERS];
	int _udpfd[MAXLISTENERS];

	int init_decoder(int n, int idle);
};

#endif
