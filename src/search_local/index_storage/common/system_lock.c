/*
 * =====================================================================================
 *
 *       Filename:  system_lock.c
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
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "system_lock.h"

int unix_socket_lock(const char *format, ...)
{
	int lockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (lockfd >= 0)
	{
		va_list ap;
		struct sockaddr_un addr;
		int len;

		addr.sun_family = AF_UNIX;

		va_start(ap, format);
		vsnprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1, format, ap);
		va_end(ap);

		addr.sun_path[0] = '@';
		len = SUN_LEN(&addr);
		addr.sun_path[0] = '\0';
		if (bind(lockfd, (struct sockaddr *)&addr, len) == 0)
			fcntl(lockfd, F_SETFD, FD_CLOEXEC);
		else
		{
			close(lockfd);
			lockfd = -1;
		}
	}
	return lockfd;
}
