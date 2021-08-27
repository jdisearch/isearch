#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "daemon.h"
#include "log.h"

volatile int DaemonBase::_stop = 0;

int DaemonBase::EnableFdLimit(int maxfd)
{
	struct rlimit rlim;
	/* raise open files */
	if (maxfd) {
		rlim.rlim_cur = maxfd;
		rlim.rlim_max = maxfd;
		if (setrlimit(RLIMIT_NOFILE, &rlim) == -1) {
			log_notice
			    ("Increase FdLimit failed, set val[%d] errmsg[%s]",
			     maxfd, strerror(errno));
			return -1;
		}
	}
	return 0;
}

int DaemonBase::EnableCoreDump()
{
	struct rlimit rlim;
	/* allow core dump */
	rlim.rlim_cur = RLIM_INFINITY;
	rlim.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_CORE, &rlim) == -1) {
		log_notice("EnableCoreDump failed, errmsg[%s]",
			   strerror(errno));
		return -1;
	}
	return 0;

}

int DaemonBase::DaemonStart(int back)
{
	struct sigaction sa;
	sigset_t sset;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	signal(SIGPIPE, SIG_IGN);
	//signal(SIGCHLD,SIG_IGN);

	sigemptyset(&sset);
	sigaddset(&sset, SIGSEGV);
	sigaddset(&sset, SIGBUS);
	sigaddset(&sset, SIGABRT);
	sigaddset(&sset, SIGILL);
	sigaddset(&sset, SIGCHLD);
	sigaddset(&sset, SIGFPE);
	sigprocmask(SIG_UNBLOCK, &sset, &sset);

	return back ? daemon(1, 1) : 0;
}

int DaemonBase::DaemonWait()
{
	int status;
	//任何一个子进程退出，整个程序退出
	while (wait(&status) == -1 && !DaemonBase::_stop) ;
	return 0;
}
