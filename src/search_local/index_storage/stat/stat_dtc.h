/*
 * =====================================================================================
 *
 *       Filename:  stat_dtc.h
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
#ifndef _STAT_DTC_H__
#define _STAT_DTC_H__

#include "stat_thread.h"
#include "stat_dtc_def.h"

#define STATIDX "../stat/dtc.stat.idx"

extern StatThread statmgr;
extern int InitStat(void);

#endif
