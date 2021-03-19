/*
 * =====================================================================================
 *
 *       Filename:  result_cache.h
 *
 *    Description:  result cache class definition.
 *
 *        Version:  1.0
 *        Created:  22/05/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __RESULT_CACHE_H__
#define __RESULT_CACHE_H__

#include "timerlist.h"
#include "poll_thread.h"
#include "poller.h"
#include "log.h"
#include <string>


class CacheOperator
{
public:
	CacheOperator(CPollThread *thread, int interval);
	virtual ~CacheOperator();
	int Init(int expired, int max_slot);
	int InitIndex(int expired, int max_slot);

private:
	int m_interval;
	CPollThread *m_Owner;
	CTimerList *m_Timer;
};

class UpdateOperator : public CTimerObject
{
public:
	UpdateOperator(CPollThread *thread, int intervalm, std::string appFieldFile);
	virtual ~UpdateOperator();
	void Attach() {AttachTimer(m_updateTimer);}
	virtual void TimerNotify(void);
private:
	int m_interval;
	CPollThread *m_Owner;
	CTimerList *m_updateTimer;
	std::string m_appFieldFile;
};

#endif