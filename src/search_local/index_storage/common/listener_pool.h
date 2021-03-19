/*
 * =====================================================================================
 *
 *       Filename:  listener_pool.h
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
#include "daemon.h"
#include "net_addr.h"

// this file is obsoleted, new one is agent_listen_pool.[hc]
class DTCListener;
class PollThread;
class DecoderUnit;
class TaskRequest;
class DTCDecoderUnit;
template <typename T>
class TaskDispatcher;

class ListenerPool
{
private:
	SocketAddress sockaddr[MAXLISTENERS];
	DTCListener *listener[MAXLISTENERS];
	PollThread *thread[MAXLISTENERS];
	DTCDecoderUnit *decoder[MAXLISTENERS];

	int init_decoder(int n, int idle, TaskDispatcher<TaskRequest> *out);

public:
	ListenerPool();
	~ListenerPool();
	int Bind(DTCConfig *, TaskDispatcher<TaskRequest> *);

	int Match(const char *, int = 0);
	int Match(const char *, const char *);
};
