/*
 * =====================================================================================
 *
 *       Filename:  listener_bind.cc
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
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/tcp.h>

#include "unix_socket.h"
#include "log.h"

int sock_bind(const char *addr, uint16_t port, int backlog)
{
	struct sockaddr_in inaddr;
	int reuse_addr = 1;
	int netfd;

	bzero(&inaddr, sizeof(struct sockaddr_in));
	inaddr.sin_family = AF_INET;
	inaddr.sin_port = htons(port);

	const char *end = strchr(addr, ':');
	if (end)
	{
		char *p = (char *)alloca(end - addr + 1);
		memcpy(p, addr, end - addr);
		p[end - addr] = '\0';
		addr = p;
	}
	if (strcmp(addr, "*") != 0 &&
		inet_pton(AF_INET, addr, &inaddr.sin_addr) <= 0)
	{
		log_error("invalid address %s:%d", addr, port);
		return -1;
	}

	if ((netfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		log_error("make tcp socket error, %m");
		return -1;
	}

	setsockopt(netfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
	setsockopt(netfd, SOL_TCP, TCP_NODELAY, &reuse_addr, sizeof(reuse_addr));
	reuse_addr = 60;
	/* 避免没有请求的空连接唤醒epoll浪费cpu资源 */
	setsockopt(netfd, SOL_TCP, TCP_DEFER_ACCEPT, &reuse_addr, sizeof(reuse_addr));

	if (bind(netfd, (struct sockaddr *)&inaddr, sizeof(struct sockaddr)) == -1)
	{
		log_error("bind tcp %s:%u failed, %m", addr, port);
		close(netfd);
		return -1;
	}

	if (listen(netfd, backlog) == -1)
	{
		log_error("listen tcp %s:%u failed, %m", addr, port);
		close(netfd);
		return -1;
	}

	log_info("listen on tcp %s:%u", addr, port);
	return netfd;
}

int udp_sock_bind(const char *addr, uint16_t port, int rbufsz, int wbufsz)
{
	struct sockaddr_in inaddr;
	int netfd;

	bzero(&inaddr, sizeof(struct sockaddr_in));
	inaddr.sin_family = AF_INET;
	inaddr.sin_port = htons(port);

	const char *end = strchr(addr, ':');
	if (end)
	{
		char *p = (char *)alloca(end - addr + 1);
		memcpy(p, addr, end - addr);
		p[end - addr] = '\0';
		addr = p;
	}
	if (strcmp(addr, "*") != 0 &&
		inet_pton(AF_INET, addr, &inaddr.sin_addr) <= 0)
	{
		log_error("invalid address %s:%d", addr, port);
		return -1;
	}

	if ((netfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		log_error("make udp socket error, %m");
		return -1;
	}

	if (bind(netfd, (struct sockaddr *)&inaddr, sizeof(struct sockaddr)) == -1)
	{
		log_error("bind udp %s:%u failed, %m", addr, port);
		close(netfd);
		return -1;
	}

	if (rbufsz)
		setsockopt(netfd, SOL_SOCKET, SO_RCVBUF, &rbufsz, sizeof(rbufsz));
	if (wbufsz)
		setsockopt(netfd, SOL_SOCKET, SO_SNDBUF, &wbufsz, sizeof(wbufsz));

	log_info("listen on udp %s:%u", addr, port);
	return netfd;
}

int unix_sock_bind(const char *path, int backlog)
{
	struct sockaddr_un unaddr;
	int netfd;

	socklen_t addrlen = init_unix_socket_address(&unaddr, path);
	if ((netfd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1)
	{
		log_error("%s", "make unix socket error, %m");
		return -1;
	}

	if (bind(netfd, (struct sockaddr *)&unaddr, addrlen) == -1)
	{
		log_error("bind unix %s failed, %m", path);
		close(netfd);
		return -1;
	}

	if (listen(netfd, backlog) == -1)
	{
		log_error("listen unix %s failed, %m", path);
		close(netfd);
		return -1;
	}

	log_info("listen on unix %s, fd=%d", path, netfd);
	return netfd;
}
