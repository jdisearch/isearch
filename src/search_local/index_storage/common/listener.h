/*
 * =====================================================================================
 *
 *       Filename:  listener.h
 *
 *    Description:  listener module.
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
#ifndef __LISTENER_H__
#define __LISTENER_H__
#include "poller.h"
#include "net_addr.h"

int unix_sock_bind(const char *path, int backlog = 0);
int udp_sock_bind(const char *addr, uint16_t port, int rbufsz, int wbufsz);
int sock_bind(const char *addr, uint16_t port, int backlog = 0);

class DecoderUnit;
class DTCListener : public PollerObject
{
public:
	DTCListener(const SocketAddress *);
	~DTCListener();

	int Bind(int blog, int rbufsz, int wbufsz);
	virtual void input_notify(void);
	virtual int Attach(DecoderUnit *, int blog = 0, int rbufsz = 0, int wbufsz = 0);
	void set_request_window(int w) { window = w; };
	int FD(void) const { return netfd; }

private:
	DecoderUnit *outPeer;
	const SocketAddress *sockaddr;
	int bind;
	int window;
};
#endif
