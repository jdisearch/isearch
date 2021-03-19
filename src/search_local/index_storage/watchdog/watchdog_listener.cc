/*
 * =====================================================================================
 *
 *       Filename:  watchdog_listener.cc
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
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "watchdog_listener.h"
#include "watchdog_helper.h"
#include "dbconfig.h"
#include "log.h"

WatchDogListener::WatchDogListener(WatchDog *o, int sec) : owner(o), peerfd(-1), delay(sec){};

WatchDogListener::~WatchDogListener(void)
{
	if (peerfd > 0)
	{
		log_debug("watchdog listener fd: %d closed", peerfd);
		close(peerfd);
	}
};

int WatchDogListener::attach_watch_dog(void)
{
	int fd[2];
	socketpair(AF_UNIX, SOCK_DGRAM, 0, fd);
	netfd = fd[0];
	peerfd = fd[1];

	char buf[30];
	snprintf(buf, sizeof(buf), "%d", peerfd);
	setenv(ENV_WATCHDOG_SOCKET_FD, buf, 1);

	int no;
	setsockopt(netfd, SOL_SOCKET, SO_PASSCRED, (no = 1, &no), sizeof(int));
	fcntl(netfd, F_SETFD, FD_CLOEXEC);
	enable_input();
	owner->set_listener(this);
	return 0;
}

void WatchDogListener::input_notify(void)
{
	char buf[16];
	int n;
	struct msghdr msg = {0};
	char ucbuf[CMSG_SPACE(sizeof(struct ucred))];
	struct iovec iov[1];

	iov[0].iov_base = (void *)&buf;
	iov[0].iov_len = sizeof(buf);
	msg.msg_control = ucbuf;
	msg.msg_controllen = sizeof ucbuf;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;

	while ((n = recvmsg(netfd, &msg, MSG_TRUNC | MSG_DONTWAIT)) > 0)
	{
		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		struct ucred *uc;
		if (msg.msg_controllen < sizeof(ucbuf) ||
			cmsg->cmsg_level != SOL_SOCKET ||
			cmsg->cmsg_type != SCM_CREDENTIALS ||
			cmsg->cmsg_len != CMSG_LEN(sizeof(struct ucred)) ||
			msg.msg_controllen < cmsg->cmsg_len)
			continue;
		uc = (struct ucred *)CMSG_DATA(cmsg);
		if (n != sizeof(buf))
			continue;

		log_debug("new watchdog object: %p, %d, %d, %s", owner, buf[0], uc->pid, buf + 1);
		WatchDogObject *obj = NULL;
		if (buf[0] == WATCHDOG_INPUT_OBJECT)
		{
			obj = new WatchDogObject(owner, buf, uc->pid);
			if (obj == NULL)
			{
				log_error("new WatchDogObject error");
				return;
			}
		}
		else if (buf[0] == WATCHDOG_INPUT_HELPER)
		{
			StartHelperPara *para = (StartHelperPara *)(buf + 1);
			char path[32];
			if (!DbConfig::build_path(path, 32, getpid(), para->mach, para->role, para->type))
			{
				log_error("build helper listen path error");
				return;
			}
			NEW(WatchDogHelper(owner, delay, path, para->mach, para->role, para->backlog, para->type, para->conf, para->num), obj);
			if (obj == NULL)
			{
				log_error("new watchdog helper error");
				return;
			}
			WatchDogHelper *helper = (WatchDogHelper *)obj;
			if (helper->Fork() < 0 || helper->Verify() < 0)
			{
				log_error("fork helper error");
				return;
			}
		}
		else
		{
			log_error("unknown watchdog input type: %d, %s", buf[0], buf + 1);
			return;
		}
		obj->attach_watch_dog();
	}
}
