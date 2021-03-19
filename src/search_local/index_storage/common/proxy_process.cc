/*
 * =====================================================================================
 *
 *       Filename:  agent_process.cc
 *
 *    Description:  agent task process.
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
#include "proxy_process.h"
#include "poll_thread.h"
#include "task_request.h"
#include "log.h"

AgentProcess::AgentProcess(PollThread *o) : TaskDispatcher<TaskRequest>(o),
                                            ownerThread(o),
                                            output(o)
{
}

AgentProcess::~AgentProcess()
{
}

void AgentProcess::task_notify(TaskRequest *curr)
{
    curr->copy_reply_for_agent_sub_task();
    log_debug("AgentProcess::task_notify start");
    //there is a race condition here:
    //curr may be deleted during process (in task->reply_notify())
    int taskCount = curr->agent_sub_task_count();
    log_debug("AgentProcess::task_notify task count is %d", taskCount);
    for (int i = 0; i < taskCount; i++)
    {
        TaskRequest *task = NULL;

        if (NULL == (task = curr->curr_agent_sub_task(i)))
            continue;

        if (curr->is_curr_sub_task_processed(i))
        {
            log_debug("AgentProcess::task_notify task reply notify");
            task->reply_notify();
        }

        else
        {
            log_debug("AgentProcess::task_notify task_notify next process");
            output.task_notify(task);
        }
    }

    return;
}
