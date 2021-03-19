/*
 * =====================================================================================
 *
 *       Filename:  agent_client_unit.h
 *
 *    Description:  agent client unit class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __AGENT_CLIENT_UNIT_H__
#define __AGENT_CLIENT_UNIT_H__

#include "stat_index.h"
#include "request_base_all.h"

class CTimerList;
class CPollThread;
class CTaskRequest;
class CAgentClientUnit
{
    public:
	CAgentClientUnit(CPollThread * t, int c);
	virtual ~CAgentClientUnit();

	inline CTimerList * TimerList() { return tlist; }
	void BindDispatcher(CTaskDispatcher<CTaskRequest> * proc);
	inline void TaskNotify(CTaskRequest * req) { output.TaskNotify(req); }
	void RecordRequestTime(int hit, int type, unsigned int usec);
	void RecordRequestTime(CTaskRequest *req);

    private:
	CPollThread * ownerThread;
	CRequestOutput<CTaskRequest> output;
	int check;
	CTimerList * tlist;

    CStatSample statRequestTime;
};

#endif
