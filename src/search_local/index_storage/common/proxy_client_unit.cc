/*
 * =====================================================================================
 *
 *       Filename:  proxy_client_unit.cc
 *
 *    Description:  proxy client unit.
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
#include "proxy_client_unit.h"
#include "timer_list.h"
#include "task_request.h"
#include "poll_thread.h"

AgentClientUnit::AgentClientUnit(PollThread *t, int c) : ownerThread(t),
                                                         output(t),
                                                         check(c)
{
    tlist = t->get_timer_list(c);

    statRequestTime[0] = statmgr.get_sample(REQ_USEC_ALL);
    statRequestTime[1] = statmgr.get_sample(REQ_USEC_GET);
    statRequestTime[2] = statmgr.get_sample(REQ_USEC_INS);
    statRequestTime[3] = statmgr.get_sample(REQ_USEC_UPD);
    statRequestTime[4] = statmgr.get_sample(REQ_USEC_DEL);
    statRequestTime[5] = statmgr.get_sample(REQ_USEC_FLUSH);
    statRequestTime[6] = statmgr.get_sample(REQ_USEC_HIT);
    statRequestTime[7] = statmgr.get_sample(REQ_USEC_REPLACE);
}

AgentClientUnit::~AgentClientUnit()
{
}

void AgentClientUnit::bind_dispatcher(TaskDispatcher<TaskRequest> *proc)
{
    output.bind_dispatcher(proc);
}

void AgentClientUnit::record_request_time(int hit, int type, unsigned int usec)
{
    static const unsigned char cmd2type[] =
        {
            /*Nop*/ 0,
            /*result_code*/ 0,
            /*DTCResultSet*/ 0,
            /*HelperAdmin*/ 0,
            /*Get*/ 1,
            /*Purge*/ 5,
            /*Insert*/ 2,
            /*Update*/ 3,
            /*Delete*/ 4,
            /*Other*/ 0,
            /*Other*/ 0,
            /*Other*/ 0,
            /*Replace*/ 7,
            /*Flush*/ 5,

            /*Invalidate*/ 0,
            /*Monitor*/ 0,
            /*ReloadConfig*/ 0,
            /*Replicate*/ 1,
            /*Migrate*/ 1,
        };
    statRequestTime[0].push(usec);
    unsigned int t = hit ? 6 : cmd2type[type];
    if (t)
        statRequestTime[t].push(usec);
}

void AgentClientUnit::record_request_time(TaskRequest *req)
{
    record_request_time(req->flag_is_hit(), req->request_code(), req->responseTimer.live());
}
