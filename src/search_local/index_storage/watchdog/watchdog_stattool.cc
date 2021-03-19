/*
 * =====================================================================================
 *
 *       Filename:  watchdog_statool.cc
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
#include <unistd.h>

#include "watchdog_stattool.h"

WatchDogStatTool::WatchDogStatTool(WatchDog *o, int sec)
	: WatchDogDaemon(o, sec)
{
	strncpy(name, "stattool", sizeof(name));
}

WatchDogStatTool::~WatchDogStatTool(void)
{
}

void WatchDogStatTool::Exec(void)
{
	char *argv[3];

	argv[1] = (char *)"reporter";
	argv[2] = NULL;

	argv[0] = (char *)"stattool32";
	execv(argv[0], argv);
	argv[0] = (char *)"../bin/stattool32";
	execv(argv[0], argv);
	argv[0] = (char *)"stattool";
	execv(argv[0], argv);
	argv[0] = (char *)"../bin/stattool";
	execv(argv[0], argv);
}
