#include "agent_client_unit.h"
#include "timerlist.h"
#include "task_request.h"
#include "poll_thread.h"

CAgentClientUnit::CAgentClientUnit(CPollThread * t, int c):
    ownerThread(t),
    output(t),
    check(c)
{
	tlist = t->GetTimerList(c);

	statRequestTime = statmgr.GetSample(REQ_USEC_ALL);
//	statRequestTime[1] = statmgr.GetSample(REQ_USEC_GET);
//	statRequestTime[2] = statmgr.GetSample(REQ_USEC_INS);
//	statRequestTime[3] = statmgr.GetSample(REQ_USEC_UPD);
//	statRequestTime[4] = statmgr.GetSample(REQ_USEC_DEL);
//	statRequestTime[5] = statmgr.GetSample(REQ_USEC_FLUSH);
//	statRequestTime[6] = statmgr.GetSample(REQ_USEC_HIT);
//	statRequestTime[7] = statmgr.GetSample(REQ_USEC_REPLACE);
}

CAgentClientUnit::~CAgentClientUnit()
{
}

void CAgentClientUnit::BindDispatcher(CTaskDispatcher<CTaskRequest> * proc) 
{ 
	output.BindDispatcher(proc); 
}

void CAgentClientUnit::RecordRequestTime(int hit, int type, unsigned int usec)
{
//        static const unsigned char cmd2type[] =
//        {
//            /*Nop*/ 0,
//            /*ResultCode*/ 0,
//            /*ResultSet*/ 0,
//            /*HelperAdmin*/ 0,
//            /*Get*/ 1,
//            /*Purge*/ 5,
//            /*Insert*/ 2,
//            /*Update*/ 3,
//            /*Delete*/ 4,
//                /*Other*/ 0,
//            /*Other*/ 0,
//            /*Other*/ 0,
//            /*Replace*/ 7,
//            /*Flush*/ 5,
//            /*Other*/ 0,
//            /*Other*/ 0,
//        };
        statRequestTime.push(usec);
//        unsigned int t = hit ? 6 : cmd2type[type];
//        if(t) statRequestTime[t].push(usec);
}

void CAgentClientUnit::RecordRequestTime(CTaskRequest *req) 
{
        RecordRequestTime(0, 9, req->responseTimer.live());
}
