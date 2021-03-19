/*
 * =====================================================================================
 *
 *       Filename:  watchdog_helper.cc
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
#include <sys/un.h>
#include "unix_socket.h"

#include "dbconfig.h"
#include "thread.h"
#include "watchdog_helper.h"
#include "daemon.h"
#include "log.h"
#include "dtc_global.h"
#include <sstream>

WatchDogHelper::WatchDogHelper(WatchDog *o, int sec, const char *p, int g, int r, int b, int t, int c, int n)
	: WatchDogDaemon(o, sec)
{
	/*snprintf(name, 20, "helper%u%c", g, MACHINEROLESTRING[r]);*/
	std::stringstream oss;
	oss << "helper" << g << MACHINEROLESTRING[r];
	memcpy(name, oss.str().c_str(), oss.str().length());
	path = p;
	backlog = b;
	type = t;
	conf = c;
	num = n;
}

WatchDogHelper::~WatchDogHelper(void)
{
}

const char *HelperName[] =
	{
		NULL,
		NULL,
		"rocksdb_helper",
		"tdb_helper",
		"custom_helper",
};

void WatchDogHelper::Exec(void)
{
	struct sockaddr_un unaddr;
	int len = init_unix_socket_address(&unaddr, path);
	int listenfd = socket(unaddr.sun_family, SOCK_STREAM, 0);
	bind(listenfd, (sockaddr *)&unaddr, len);
	listen(listenfd, backlog);

	// relocate listenfd to stdin
	dup2(listenfd, 0);
	close(listenfd);

	char *argv[9];
	int argc = 0;

	argv[argc++] = NULL;
	argv[argc++] = (char *)"-d";
	if (strcmp(cacheFile, CACHE_CONF_NAME))
	{
		argv[argc++] = (char *)"-f";
		argv[argc++] = cacheFile;
	}
	if (conf == DBHELPER_TABLE_NEW)
	{
		argv[argc++] = (char *)"-t";
		char tableName[64];
		snprintf(tableName, 64, "../conf/table%d.conf", num);
		//argv[argc++] = (char *)"../conf/table2.conf";
		argv[argc++] = tableName;
	}
	else if (conf == DBHELPER_TABLE_ORIGIN && strcmp(tableFile, TABLE_CONF_NAME))
	{
		argv[argc++] = (char *)"-t";
		argv[argc++] = tableFile;
	}
	argv[argc++] = name + 6;
	argv[argc++] = (char *)"-";
	argv[argc++] = NULL;

	Thread *helperThread = new Thread(name, Thread::ThreadTypeProcess);
	helperThread->initialize_thread();
	argv[0] = (char *)HelperName[type];
	execv(argv[0], argv);
	log_error("helper[%s] execv error: %m", argv[0]);
}

int WatchDogHelper::Verify(void)
{
	struct sockaddr_un unaddr;
	int len = init_unix_socket_address(&unaddr, path);

	// delay 100ms and verify socket
	usleep(100 * 1000);
	int s = socket(unaddr.sun_family, SOCK_STREAM, 0);
	if (connect(s, (sockaddr *)&unaddr, len) < 0)
	{
		close(s);
		log_error("verify connect: %m");
		return -1;
	}
	close(s);

	return pid;
}
