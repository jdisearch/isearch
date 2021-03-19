/*
 * =====================================================================================
 *
 *       Filename:  agent_process.h
 *
 *    Description:  agent_process class definition.
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


#ifndef __AGENT_PROCESS_H__
#define __AGENT_PROCESS_H__

#include "request_base.h"

class CPollThread;
class CTaskRequest;

class CAgentProcess : public CTaskDispatcher<CTaskRequest>
{
    private:
	CPollThread * ownerThread;
	CRequestOutput<CTaskRequest> output;

    public:
	CAgentProcess(CPollThread * o);
	virtual ~CAgentProcess();

	inline void BindDispatcher(CTaskDispatcher<CTaskRequest> *p)
	{
	    output.BindDispatcher(p);
	}
	virtual void TaskNotify(CTaskRequest * curr);
};

#endif
