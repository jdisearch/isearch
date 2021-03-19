/*
 * =====================================================================================
 *
 *       Filename:  sock_bind.cc
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
#include <netinet/tcp.h>
#include "net_addr.h"
#include "log.h"

int sock_bind(const SocketAddress *addr, int backlog, int rbufsz, int wbufsz, int reuse, int nodelay, int defer_accept)
{
	int netfd;

	if ((netfd = addr->create_socket()) == -1)
	{
		log_error("%s: make socket error, %m", addr->Name());
		return -1;
	}

	int optval = 1;
	if (reuse)
		setsockopt(netfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (nodelay)
		setsockopt(netfd, SOL_TCP, TCP_NODELAY, &optval, sizeof(optval));

	/* 避免没有请求的空连接唤醒epoll浪费cpu资源 */
	if (defer_accept)
	{
		optval = 60;
		setsockopt(netfd, SOL_TCP, TCP_DEFER_ACCEPT, &optval, sizeof(optval));
	}

	if (addr->bind_socket(netfd) == -1)
	{
		log_error("%s: bind failed, %m", addr->Name());
		close(netfd);
		return -1;
	}

	if (addr->socket_type() == SOCK_STREAM && listen(netfd, backlog) == -1)
	{
		log_error("%s: listen failed, %m", addr->Name());
		close(netfd);
		return -1;
	}

	if (rbufsz)
		setsockopt(netfd, SOL_SOCKET, SO_RCVBUF, &rbufsz, sizeof(rbufsz));
	if (wbufsz)
		setsockopt(netfd, SOL_SOCKET, SO_SNDBUF, &wbufsz, sizeof(wbufsz));

	log_info("%s on %s", addr->socket_type() == SOCK_STREAM ? "listen" : "bind", addr->Name());
	return netfd;
}
