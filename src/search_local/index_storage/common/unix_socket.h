/*
 * =====================================================================================
 *
 *       Filename:  unix_socket.h
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
#include <sys/un.h>
#include <sys/socket.h>

static inline int is_unix_socket_path(const char *path)
{
	return path[0] == '/' || path[0] == '@';
}

static inline int init_unix_socket_address(struct sockaddr_un *addr, const char *path)
{

	bzero(addr, sizeof(struct sockaddr_un));
	addr->sun_family = AF_LOCAL;
	strncpy(addr->sun_path, path, sizeof(addr->sun_path) - 1);
	socklen_t addrlen = SUN_LEN(addr);
	if (path[0] == '@')
		addr->sun_path[0] = '\0';
	return addrlen;
}
