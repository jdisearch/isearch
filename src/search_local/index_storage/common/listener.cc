/*
 * =====================================================================================
 *
 *       Filename:  listener.cc
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
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "unix_socket.h"
#include "listener.h"
#include "poll_thread.h"
#include "decoder_base.h"
#include "log.h"
#include <stat_dtc.h>

extern int gMaxConnCnt;

DTCListener::DTCListener(const SocketAddress *s)
{
	sockaddr = s;
	bind = 0;
	window = 0;
	outPeer = NULL;
}

DTCListener::~DTCListener()
{
	if (sockaddr && sockaddr->socket_family() == AF_UNIX && netfd >= 0)
	{
		if (sockaddr->Name()[0] == '/')
		{
			unlink(sockaddr->Name());
		}
	}
}

int DTCListener::Bind(int blog, int rbufsz, int wbufsz)
{
	if (bind)
		return 0;

	if ((netfd = sock_bind(sockaddr, blog, rbufsz, wbufsz, 1 /*reuse*/, 1 /*nodelay*/, 1 /*defer_accept*/)) == -1)
		return -1;

	bind = 1;

	return 0;
}

int DTCListener::Attach(DecoderUnit *unit, int blog, int rbufsz, int wbufsz)
{
	if (Bind(blog, rbufsz, wbufsz) != 0)
		return -1;

	outPeer = unit;
	if (sockaddr->socket_type() == SOCK_DGRAM)
	{
		if (unit->process_dgram(netfd) < 0)
			return -1;
		else
			netfd = dup(netfd);
		return 0;
	}
	enable_input();
	return attach_poller(unit->owner_thread());
}

void DTCListener::input_notify(void)
{
	log_debug("enter input_notify!!!!!!!!!!!!!!!!!");
	int newfd = -1;
	socklen_t peerSize;
	struct sockaddr peer;

	while (true)
	{
		peerSize = sizeof(peer);
		newfd = accept(netfd, &peer, &peerSize);

		if (newfd == -1)
		{
			if (errno != EINTR && errno != EAGAIN)
				log_notice("[%s]accept failed, fd=%d, %m", sockaddr->Name(), netfd);

			break;
		}

		if (outPeer->process_stream(newfd, window, &peer, peerSize) < 0)
			close(newfd);
	}
}
