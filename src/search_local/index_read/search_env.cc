/*
 * =====================================================================================
 *
 *       Filename:  search_env.h
 *
 *    Description:  search env class definition.
 *
 *        Version:  1.0
 *        Created:  16/03/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "log.h"
#include "search_env.h"
#include <arpa/inet.h>
#include "comm.h"


int SearchEnv::cpa_atoi(char *line, size_t n) {
	int value;

	if (n == 0) {
		return -RT_PARA_ERR;
	}

	for (value = 0; n--; line++) {
		if (*line < '0' || *line > '9') {
			return -RT_PARA_ERR;
		}

		value = value * 10 + (*line - '0');
	}

	if (value < 0) {
		return -RT_PARA_ERR;
	}

	return value;
}

char *SearchEnv::UnresolveAddr(struct sockaddr *addr, int addrlen)
{
	static char unresolve[NI_MAXHOST + NI_MAXSERV];
	static char host[NI_MAXHOST], service[NI_MAXSERV];
	int status;

	status = getnameinfo(addr, addrlen, host, sizeof(host), service,
		sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV);
	if (status < 0)
	{
		log_error("get sock name err, errno %d, errmsg %s", status, gai_strerror(status));
		snprintf(unresolve, sizeof(unresolve), "unknown");
		return unresolve;
	}

	snprintf(unresolve, sizeof(unresolve), "%s:%s", host, service);
	log_error("addr:%s", unresolve);
	return unresolve;
}

char *SearchEnv::UnresolveDesc(int sd)
{
	static char unresolve[NI_MAXHOST + NI_MAXSERV];
	struct sockaddr *addr;
	socklen_t addrlen = sizeof(struct sockaddr);
	int status;
	addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
	if (addr == NULL)
	{
		log_error("no mem");
		snprintf(unresolve, sizeof(unresolve), "unknown");
		return unresolve;
	}
	status = getsockname(sd, addr, &addrlen);
	if (status < 0) {
		log_error("get sock name err, errno %d, errmsg %s", errno, strerror(errno));
		snprintf(unresolve, sizeof(unresolve), "unknown");
		return unresolve;
	}
	sockaddr_in sin;
	memcpy(&sin, addr, sizeof(sin));
	return UnresolveAddr(addr, addrlen);
}

int SearchEnv::WriteSocketEnv(int fd)
{
	int len = 0;
	memcpy(os_environ, NC_ENV_FDS, strlen(NC_ENV_FDS));
	len = strlen(NC_ENV_FDS);
	memcpy(os_environ + len, "=", strlen("="));
	len += strlen("=");
	sprintf(os_environ + len, "%d;", fd);
	log_debug("global env is %s", os_environ);
	return 0;
}

int SearchEnv::ReadSocketEnv(struct sockaddr *addr, int addrlen)
{
	char *listen_address = UnresolveAddr(addr, addrlen);
	char *p, *q;
	int sock;
	char *inherited;
	inherited = getenv(NC_ENV_FDS);

	char address[NI_MAXHOST + NI_MAXSERV];

	if (inherited == NULL) {
		return 0;
	}


	strncpy(address, listen_address, sizeof(address));
	log_debug("address %s", address);
	log_debug("inherited %s", inherited);
	for (p = inherited, q = inherited; *p; p++) {
		if (*p == ';') {
			sock = cpa_atoi(q, p - q);
			if (strcmp(address, UnresolveDesc(sock)) == 0) {
				log_debug("get inherited socket %d for '%s' from '%s'", sock,
					address, inherited);
				sock = dup(sock);
				log_debug("dup inherited socket as %d", sock);
				return sock;
			}
			q = p + 1;
		}
	}

	return 0;
}

int SearchEnv::CleanSocketEnv()
{
	int sock = 0;
	char *inherited;
	char *p, *q;

	inherited = getenv(NC_ENV_FDS);
	if (inherited == NULL) {
		return 0;
	}

	for (p = inherited, q = inherited; *p; p++) {
		if (*p == ';') {
			sock = cpa_atoi(q, p - q);
			close(sock);
			q = p + 1;
		}
	}
	return 0;
}