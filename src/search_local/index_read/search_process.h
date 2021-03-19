/*
 * =====================================================================================
 *
 *       Filename:  search_process.h
 *
 *    Description:  search process class definition.
 *
 *        Version:  1.0
 *        Created:  07/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef SEARCH_PROCESS_H_
#define SEARCH_PROCESS_H_

#include "request_base.h"
#include "comm.h"
#include "process_task.h"

class CPollThread;
class CTaskRequest;

class CSearchProcess : public CTaskDispatcher<CTaskRequest>
{
private:
	CPollThread * ownerThread;
	CRequestOutput<CTaskRequest> output;
	bool statInitFlag;

	string GenReplyStr(RSPCODE code);
	ProcessTask *CreateTask(uint16_t CmdType);

public:
	CSearchProcess(CPollThread * o);
	virtual ~CSearchProcess();

	inline void BindDispatcher(CTaskDispatcher<CTaskRequest> *p)
	{
		output.BindDispatcher(p);
	}
	virtual void TaskNotify(CTaskRequest * curr);
};



#endif /* SEARCH_PROCESS_H_ */
