/*
 * =====================================================================================
 *
 *       Filename:  watchdog_fork.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  linjinming, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <signal.h>

#ifndef CLONE_PARENT
#define CLONE_PARENT 0x00008000
#endif

#include "watchdog_listener.h"
#include "log.h"

struct ForkInfo
{
	char name[16];
	int fd;
	int (*entry)(void *);
	void *args;
};

static int CloneEntry(void *arg)
{
	struct ForkInfo *info = (ForkInfo *)arg;

	send(info->fd, info->name, sizeof(info->name), 0);
	//close(info->fd);
	exit(info->entry(info->args));
}

int watch_dog_fork(const char *name, int (*entry)(void *), void *args)
{
	char stack[4096]; // 4K stack is enough for CloneEntry
	struct ForkInfo info;
	char *env = getenv(ENV_WATCHDOG_SOCKET_FD);

	info.fd = env == NULL ? -1 : atoi(env);
	log_debug("watchdog fork, fd: %d, name: %s, args: %p", info.fd, name, args);
	if (info.fd <= 0)
	{
		int pid = fork();
		if (pid == 0)
		{
			exit(entry(args));
		}
		return pid;
	}

	strncpy(info.name, name, sizeof(info.name));
	info.entry = entry;
	info.args = args;

	return clone(
		CloneEntry, // entry
		stack + sizeof(stack) - 16,
		CLONE_PARENT | SIGCHLD, // flag
		&info					// data
	);
};
