/*
 * =====================================================================================
 *
 *       Filename:  log_alert.cc
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
#include <libgen.h>

#include "log.h"
#include "config.h"
#include "daemon.h"

#include <Attr_API.h>

static char iname[1024];
static int inamelen;
static unsigned int attrid;

static int alert_func_attr(const char *msg, int len)
{
	int ret = 0;
	if (attrid > 600) /* 二级网管允许的最小告警id */
	{
		if (len + inamelen >= (int)sizeof(iname))
		{
			len = sizeof(iname) - inamelen;
		}
		memcpy(iname + inamelen, msg, len);
		if (adv_attr_set(attrid, inamelen + len, iname))
			ret = -1;
	}
	return ret;
}

static int scan_process_name(char name[], size_t size)
{
	char str[1024] = {0};

	if (readlink("/proc/self/cwd", str, 1023) < 0)
		return -1;

	snprintf(name, size, "%s", basename(dirname(str)));
	return 0;
}

void _init_log_alerter_(void)
{
	attrid = gConfig->get_int_val("cache", "OpenningFDAttrID", 0);
	if (attrid > 600) /* 二级网管允许的最小告警id */
	{
		/* 实例名称 */
		if (scan_process_name(iname, sizeof(iname)))
			snprintf(iname, sizeof(iname), "dtc");
		inamelen = strlen(iname);
		// append ": ", report as: "iname: msg"
		iname[inamelen++] = ':';
		iname[inamelen++] = ' ';

		_set_log_alert_hook_(alert_func_attr);
		log_info("log_alert send to attr id=%u\n", attrid);
	}
}
