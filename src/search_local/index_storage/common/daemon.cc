/*
 * =====================================================================================
 *
 *       Filename:  daemon.cc
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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>

#include "config.h"
#include "table_def.h"
#include "admin_tdef.h"
#include "daemon.h"
#include "log.h"
#include <pthread.h>
#include "dbconfig.h"
#include "version.h"
#include "table_def_manager.h"

DTCConfig *gConfig = NULL;
DTCTableDefinition *gTableDef[2] = {NULL, NULL};
DbConfig *dbConfig = NULL;

volatile int stop = 0;
volatile int crash_signo = 0;
int background = 1;
extern const char progname[];
extern const char usage_argv[];

pthread_t mainthreadid;

bool flatMode;

void ShowVersionDetail()
{
	printf("%s version: %s\n", progname, version_detail);
}

void ShowCompDate()
{
	printf("%s compile date: %s %s\n", progname, compdatestr, comptimestr);
}

void show_usage()
{
	ShowVersionDetail();
	ShowCompDate();
	printf("Usage:\n    %s  [-d] [-h] [-v] [-V]%s\n", progname, usage_argv);
}

int dtc_daemon_init(int argc, char **argv)
{
	int c;

	while ((c = getopt(argc, argv, "df:t:hvV")) != -1)
	{
		switch (c)
		{
		case 'd':
			background = 0;
			break;
		case 'f':
			strncpy(cacheFile, optarg, sizeof(cacheFile) - 1);
			break;
		case 't':
			strncpy(tableFile, optarg, sizeof(tableFile) - 1);
			break;
		case 'h':
			show_usage();
			exit(0);
		case 'v':
			ShowVersionDetail();
			ShowCompDate();
			exit(0);
		case 'V':
			ShowVersionDetail();
			ShowCompDate();
			exit(0);
		case '?':
			show_usage();
			exit(-1);
		}
	}

	_init_log_("dtcd");
	log_info("%s v%s: starting....", progname, version);

	gConfig = new DTCConfig;
	//load config file and copy it to ../stat
	if (gConfig->parse_config(tableFile, "DB_DEFINE", true) == -1)
		return -1;

	if (gConfig->parse_config(cacheFile, "cache", true))
		return -1;

	_set_log_level_(gConfig->get_idx_val("cache", "LogLevel",
									   ((const char *const[]){
										   "emerg",
										   "alert",
										   "crit",
										   "error",
										   "warning",
										   "notice",
										   "info",
										   "debug",
										   NULL}),
									   6));
	_set_log_thread_name_(progname);
	_init_log_alerter_();

	dbConfig = DbConfig::Load(gConfig);
	if (dbConfig == NULL)
		return -1;

	gTableDef[0] = dbConfig->build_table_definition();
	if (gTableDef[0] == NULL)
		return -1;

	DTCTableDefinition *t;
	if ((t = TableDefinitionManager::Instance()->load_table(tableFile)) == NULL)
		return -1;
	TableDefinitionManager::Instance()->set_cur_table_def(t, 0);

	gTableDef[1] = build_hot_backup_table();
	if (gTableDef[1] == NULL)
		return -1;

	if (!TableDefinitionManager::Instance()->build_hot_backup_table_def())
		return -1;

	return 0;
}

static void sigterm_handler(int signo)
{
	stop = 1;
}

int daemon_start()
{
	struct sigaction sa;
	sigset_t sset;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigterm_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	sigemptyset(&sset);
	sigaddset(&sset, SIGTERM);
	sigaddset(&sset, SIGSEGV);
	sigaddset(&sset, SIGBUS);
	sigaddset(&sset, SIGABRT);
	sigaddset(&sset, SIGILL);
	sigaddset(&sset, SIGCHLD);
	sigaddset(&sset, SIGFPE);
	sigprocmask(SIG_UNBLOCK, &sset, &sset);

	int ret = background ? daemon(1, 1) : 0;
	mainthreadid = pthread_self();
	return ret;
}

void DaemonCrashed(int signo)
{
	stop = 1;
	crash_signo = signo;
	if (mainthreadid)
		pthread_kill(mainthreadid, SIGTERM);
}

void daemon_cleanup(void)
{
#if MEMCHECK
	DELETE(gBConfig);
	DELETE(gConfig);
	DELETE(gTableDef[0]);
	DELETE(gTableDef[1]);
	if (dbConfig)
		dbConfig->Destroy();
#endif
	// CrashProtect: special retval 85 means protected-crash
	if (crash_signo)
		exit(85);
}

int daemon_get_fd_limit(void)
{
	struct rlimit rlim;
	if (getrlimit(RLIMIT_NOFILE, &rlim) == -1)
	{
		log_notice("Query FdLimit failed, errmsg[%s]", strerror(errno));
		return -1;
	}

	return rlim.rlim_cur;
}

int daemon_set_fd_limit(int maxfd)
{
	struct rlimit rlim;
	if (maxfd)
	{
		/* raise open files */
		rlim.rlim_cur = maxfd;
		rlim.rlim_max = maxfd;
		if (setrlimit(RLIMIT_NOFILE, &rlim) == -1)
		{
			log_notice("Increase FdLimit failed, set val[%d] errmsg[%s]", maxfd, strerror(errno));
			return -1;
		}
	}
	return 0;
}

int daemon_enable_core_dump(void)
{
	struct rlimit rlim;

	/* allow core dump  100M */
	rlim.rlim_cur = 100UL << 20;
	rlim.rlim_max = 100UL << 20;

	if (setrlimit(RLIMIT_CORE, &rlim) == -1)
	{
		if (getrlimit(RLIMIT_CORE, &rlim) == 0)
		{
			rlim.rlim_cur = rlim.rlim_max;
			setrlimit(RLIMIT_CORE, &rlim);
		}

		log_notice("EnableCoreDump size[%ld]", (long)rlim.rlim_max);
	}

	return 0;
}
