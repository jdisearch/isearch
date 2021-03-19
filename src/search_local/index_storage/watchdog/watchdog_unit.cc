/*
 * =====================================================================================
 *
 *       Filename:  watchdog_unit.cc
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
#include <sys/wait.h>
#include <errno.h>

#include "config.h"
#include "watchdog_unit.h"
#include "log.h"
#include "stat_alarm_reporter.h"
#include <sstream>
#include <dtc_global.h>

WatchDogObject::~WatchDogObject(void)
{
}

int WatchDogObject::attach_watch_dog(WatchDogUnit *u)
{
	if (u && owner == NULL)
		owner = u;

	return owner ? owner->AttachProcess(this) : -1;
}

void WatchDogObject::exited_notify(int retval)
{
	delete this;
}

void WatchDogObject::killed_notify(int sig, int cd)
{
	report_kill_alarm(sig, cd);
	delete this;
}
void WatchDogObject::report_kill_alarm(int signo, int coredumped)
{

	if (!ALARM_REPORTER->init_alarm_cfg(std::string(ALARM_CONF_FILE), true))
	{
		log_error("init alarm conf file fail");
		return;
	}
	ALARM_REPORTER->set_time_out(1);
	std::stringstream oss;
	oss << "child process[" << pid << "][ " << name << " ]killed by signal " << signo;
	if (coredumped)
	{
		oss << " core dumped";
	}
	ALARM_REPORTER->report_alarm(oss.str());
}
WatchDogUnit::WatchDogUnit(void)
	: pidCount(0){};

WatchDogUnit::~WatchDogUnit(void)
{
	pidmap_t::iterator i;
	for (i = pid2obj.begin(); i != pid2obj.end(); i++)
	{
		WatchDogObject *obj = i->second;
		delete obj;
	}
};

int WatchDogUnit::CheckWatchDog(void)
{
	while (1)
	{
		int status;
		int pid = waitpid(-1, &status, WNOHANG);
		int err = errno;

		if (pid < 0)
		{
			switch (err)
			{
			case EINTR:
			case ECHILD:
				break;
			default:
				log_notice("wait() return pid %d errno %d", pid, err);
				break;
			}
			break;
		}
		else if (pid == 0)
		{
			break;
		}
		else
		{
			pidmap_t::iterator itr = pid2obj.find(pid);
			if (itr == pid2obj.end())
			{
				log_notice("wait() return unknown pid %d status 0x%x", pid, status);
			}
			else
			{
				WatchDogObject *obj = itr->second;
				const char *const name = obj->Name();

				pid2obj.erase(itr);

				// special exit value return-ed by CrashProtector
				if (WIFEXITED(status) && WEXITSTATUS(status) == 85 && strncmp(name, "main", 5) == 0)
				{ // treat it as a fake SIGSEGV
					status = W_STOPCODE(SIGSEGV);
				}

				if (WIFSIGNALED(status))
				{
					const int sig = WTERMSIG(status);
					const int core = WCOREDUMP(status);

					log_alert("%s: killed by signal %d", name, sig);

					log_error("child %.16s pid %d killed by signal %d%s",
							  name, pid, sig,
							  core ? " (core dumped)" : "");
					pidCount--;
					obj->killed_notify(sig, core);
				}
				else if (WIFEXITED(status))
				{
					const int retval = (signed char)WEXITSTATUS(status);
					if (retval == 0)
						log_debug("child %.16s pid %d exit status %d",
								  name, pid, retval);
					else
						log_info("child %.16s pid %d exit status %d",
								 name, pid, retval);
					pidCount--;
					obj->exited_notify(retval);
				}
			}
		}
	}
	return pidCount;
}

int WatchDogUnit::AttachProcess(WatchDogObject *obj)
{
	const int pid = obj->pid;
	pidmap_t::iterator itr = pid2obj.find(pid);
	if (itr != pid2obj.end() || pid <= 1)
		return -1;

	pid2obj[pid] = obj;
	pidCount++;
	return 0;
}

int WatchDogUnit::KillAll(void)
{
	pidmap_t::iterator i;
	int n = 0;
	for (i = pid2obj.begin(); i != pid2obj.end(); i++)
	{
		WatchDogObject *obj = i->second;
		if (obj->Pid() > 1)
		{
			log_debug("killing child %.16s pid %d SIGTERM",
					  obj->Name(), obj->Pid());
			kill(obj->Pid(), SIGTERM);
			n++;
		}
	}
	return n;
}

int WatchDogUnit::ForceKillAll(void)
{
	pidmap_t::iterator i;
	int n = 0;
	for (i = pid2obj.begin(); i != pid2obj.end(); i++)
	{
		WatchDogObject *obj = i->second;
		if (obj->Pid() > 1)
		{
			log_error("child %.16s pid %d didn't exit in timely, sending SIGKILL.",
					  obj->Name(), obj->Pid());
			kill(obj->Pid(), SIGKILL);
			n++;
		}
	}
	return n;
}
