/*
 * =====================================================================================
 *
 *       Filename:  log_stat.cc
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
#include "log.h"
#include "stat_dtc.h"
extern StatItemU32 *__statLogCount;
extern unsigned int __localStatLogCnt[8];

void _init_log_stat_(void)
{
	__statLogCount = new StatItemU32[8];
	for (unsigned i = 0; i < 8; i++)
	{
		__statLogCount[i] = statmgr.get_item_u32(LOG_COUNT_0 + i);
		__statLogCount[i].set(__localStatLogCnt[i]);
	}
}

// prevent memory leak if module unloaded
// unused-yet because this file never link into module
__attribute__((__constructor__)) static void clean_logstat(void)
{
	if (__statLogCount != NULL)
	{
		delete __statLogCount;
		__statLogCount = NULL;
	}
}
