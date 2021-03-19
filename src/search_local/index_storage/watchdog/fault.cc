/*
 * =====================================================================================
 *
 *       Filename:  fault.cc
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
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <version.h>
#include <asm/unistd.h>

#include "compiler.h"
#include "fault.h"

__HIDDEN
int FaultHandler::DogPid = 0;

__HIDDEN
FaultHandler FaultHandler::Instance;

extern "C"
	__attribute__((__weak__)) void
	crash_hook(int signo);

extern "C"
{
	__EXPORT volatile int crash_continue;
};

static int __crash_hook = 0;

__HIDDEN
void FaultHandler::Handler(int signo, siginfo_t *dummy, void *dummy2)
{
	signal(signo, SIG_DFL);
	if (DogPid > 1)
	{
		sigval tid;
		tid.sival_int = syscall(__NR_gettid);
		sigqueue(DogPid, SIGWINCH, tid);
		for (int i = 0; i < 50 && !crash_continue; i++)
			usleep(100 * 1000);
		if ((__crash_hook != 0) && (&crash_hook != 0))
			crash_hook(signo);
	}
}

__HIDDEN
FaultHandler::FaultHandler(void)
{
	Initialize(1);
}

__HIDDEN
void FaultHandler::Initialize(int protect)
{
	char *p = getenv(ENV_FAULT_LOGGER_PID);
	if (p && (DogPid = atoi(p)) > 1)
	{
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_sigaction = FaultHandler::Handler;
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		sigaction(SIGSEGV, &sa, NULL);
		sigaction(SIGBUS, &sa, NULL);
		sigaction(SIGILL, &sa, NULL);
		sigaction(SIGABRT, &sa, NULL);
		sigaction(SIGFPE, &sa, NULL);
		if (protect)
			__crash_hook = 1;
	}
}

#if __pic__ || __PIC__
extern "C" const char __invoke_dynamic_linker__[]
	__attribute__((section(".interp")))
	__HIDDEN =
#if __x86_64__
		"/lib64/ld-linux-x86-64.so.2"
#else
		"/lib/ld-linux.so.2"
#endif
	;

extern "C" __HIDDEN int _so_start(void)
{
#define BANNER "DTC FaultLogger v" DTC_VERSION_DETAIL "\n" \
			   "  - USED BY DTCD INTERNAL ONLY!!!\n"
	int unused;
	unused = write(1, BANNER, sizeof(BANNER) - 1);
	_exit(0);
}
#endif
