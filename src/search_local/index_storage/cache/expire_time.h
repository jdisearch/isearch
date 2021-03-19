/*
 * =====================================================================================
 *
 *       Filename:  expire_time.h
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
#ifndef __DTC_EXPIRE_TIME_H
#define __DTC_EXPIRE_TIME_H

#include "namespace.h"
#include "timer_list.h"
#include "log.h"
#include "stat_dtc.h"
#include "buffer_pool.h"
#include "data_process.h"
#include "raw_data_process.h"

DTC_BEGIN_NAMESPACE

class TimerObject;
class ExpireTime : private TimerObject
{
public:
	ExpireTime(TimerList *t, DTCBufferPool *c, DataProcess *p, DTCTableDefinition *td, int e);
	virtual ~ExpireTime(void);
	virtual void timer_notify(void);
	void start_key_expired_task(void);
	int try_expire_count();

private:
	TimerList *timer;
	DTCBufferPool *cache;
	DataProcess *process;
	DTCTableDefinition *tableDef;

	StatItemU32 statExpireCount;
	StatItemU32 statGetCount;
	StatItemU32 statInsertCount;
	StatItemU32 statUpdateCount;
	StatItemU32 statDeleteCount;
	StatItemU32 statPurgeCount;

	int maxExpire;
};

DTC_END_NAMESPACE

#endif
