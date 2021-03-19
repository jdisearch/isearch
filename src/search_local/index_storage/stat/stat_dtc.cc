/*
 * =====================================================================================
 *
 *       Filename:  stat_dtc.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Norton, yangshuang68@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <unistd.h>
#include <stdlib.h>
#include "stat_dtc.h"
#include "version.h"
#include "log.h"

StatThread statmgr;

int InitStat(void)
{
	int ret;
	ret = statmgr.init_stat_info("dtcd", STATIDX);
	// -1, recreate, -2, failed
	if (ret == -1)
	{
		unlink(STATIDX);
		char buf[64];
		ret = statmgr.CreateStatIndex("dtcd", STATIDX, StatDefinition, buf, sizeof(buf));
		if (ret != 0)
		{
			log_crit("CreateStatIndex failed: %s", statmgr.error_message());
			exit(ret);
		}
		ret = statmgr.init_stat_info("dtcd", STATIDX);
	}
	if (ret == 0)
	{
		int v1, v2, v3;
		sscanf(DTC_VERSION, "%d.%d.%d", &v1, &v2, &v3);
		statmgr.get_item(S_VERSION) = v1 * 10000 + v2 * 100 + v3;
		statmgr.get_item(C_TIME) = compile_time;
		_init_log_stat_();
	}
	else
	{
		log_crit("init_stat_info failed: %s", statmgr.error_message());
		exit(ret);
	}
	return ret;
}
