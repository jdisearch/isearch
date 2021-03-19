/*
 * =====================================================================================
 *
 *       Filename:  watchdog_logger.cc
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
#include <string.h>

#include "daemon.h"
#include "proc_title.h"
#include "config.h"
#include "gdb.h"
#include "log.h"
#include "watchdog.h"
#include "watchdog_unit.h"
#include "fault.h"
#include "thread.h"

#define HOOKSO "../bin/faultlogger"

static void Set_LD_PRELOAD(void)
{
	if (access(HOOKSO, R_OK) == 0)
	{
		char *preload = canonicalize_file_name(HOOKSO);
		char *p = getenv("LD_PRELOAD");
		if (p == NULL)
		{
			setenv("LD_PRELOAD", preload, 1);
		}
		else
		{
			char *concat = NULL;
			int unused;
			unused = asprintf(&concat, "%s:%s", p, preload);
			setenv("LD_PRELOAD", concat, 1);
		}
	}
}

int start_fault_logger(WatchDog *wdog)
{
	// CarshProtect/CrashLog mode:
	// 0 -- disabled
	// 1 -- log-only
	// 2 -- protect
	// 3 -- screen
	// 4 -- xterm
	int mode = 2; // default protect
	const char *display;

	display = gConfig->get_str_val("cache", "FaultLoggerMode");
	if (display == NULL || !display[0])
	{
		mode = 2; // protect
	}
	else if (!strcmp(display, "log"))
	{
		mode = 1; // log
	}
	else if (!strcmp(display, "dump"))
	{
		mode = 1; // log
	}
	else if (!strcmp(display, "protect"))
	{
		mode = 2; // protect
	}
	else if (!strcmp(display, "screen"))
	{
		mode = 3; // screen
	}
	else if (!strcmp(display, "xterm"))
	{
		if (getenv("DISPLAY"))
		{
			mode = 4; // xterm
			display = NULL;
		}
		else
		{
			log_warning("FaultLoggerTarget set to \"xterm\", but no DISPLAY found");
			mode = 2; // screen
		}
	}
	else if (!strncmp(display, "xterm:", 6))
	{
		mode = 4; // xterm
		display += 6;
	}
	else if (!strcasecmp(display, "disable"))
	{
		mode = 0;
	}
	else if (!strcasecmp(display, "disabled"))
	{
		mode = 0;
	}
	else
	{
		log_warning("unknown FaultLoggerMode \"%s\"", display);
	}

	log_info("FaultLoggerMode is %s\n", ((const char *[]){"disable", "log", "protect", "screen", "xterm"})[mode]);
	if (mode == 0)
		return 0;

	int pid = fork();

	if (pid < 0)
		return -1;

	if (pid == 0)
	{
		set_proc_title("FaultLogger");
		Thread *loggerThread = new Thread("faultlogger", Thread::ThreadTypeProcess);
		loggerThread->initialize_thread();
		gdb_server(mode >= 3 /*debug*/, display);
		exit(0);
	}

	char buf[20];
	snprintf(buf, sizeof(buf), "%d", pid);
	setenv(ENV_FAULT_LOGGER_PID, buf, 1);
	FaultHandler::Initialize(mode >= 2);
	Set_LD_PRELOAD();

	WatchDogObject *obj = new WatchDogObject(wdog, "FaultLogger", pid);
	obj->attach_watch_dog();
	return 0;
}
