/*
 * =====================================================================================
 *
 *       Filename:  dynamic_helper_collection.cc
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
#include "helper_group.h"
#include "stat_dtc_def.h"

#include "dynamic_helper_collection.h"

DynamicHelperCollection::DynamicHelperCollection(PollThread *owner, int clientPerGroup) : TaskDispatcher<TaskRequest>(owner),
                                                                                          m_recvTimerList(0), m_connTimerList(0), m_retryTimerList(0), m_clientPerGroup(clientPerGroup)
{
}

DynamicHelperCollection::~DynamicHelperCollection()
{
    for (HelperMapType::iterator iter = m_groups.begin();
         iter != m_groups.end(); ++iter)
    {
        delete iter->second.group;
    }

    m_groups.clear();
}

void DynamicHelperCollection::task_notify(TaskRequest *t)
{
    log_debug("DynamicHelperCollection::task_notify start, t->remote_addr: %s", t->remote_addr());
    HelperMapType::iterator iter = m_groups.find(t->remote_addr());
    if (iter == m_groups.end())
    {
        HelperGroup *g = new HelperGroup(
            t->remote_addr(),  /* sock path */
            t->remote_addr(),  /* name */
            m_clientPerGroup, /* helper client count */
            m_clientPerGroup * 32 /* queue size */,
            DTC_FORWARD_USEC_ALL);
        g->set_timer_handler(m_recvTimerList, m_connTimerList, m_retryTimerList);
        g->Attach(owner, &m_taskQueueAllocator);
        helper_group hg = {g, 0};
        m_groups[t->remote_addr()] = hg;
        iter = m_groups.find(t->remote_addr());
    }
    t->push_reply_dispatcher(this);
    iter->second.group->task_notify(t);
    iter->second.used = 1;
    log_debug("DynamicHelperCollection::task_notify end");
}

void DynamicHelperCollection::reply_notify(TaskRequest *t)
{
    if (t->result_code() == 0)
    {
        log_debug("reply from remote dtc success,append result start ");

        if (t->result)
        {
            t->prepare_result();
            int iRet = t->pass_all_result(t->result);
            if (iRet < 0)
            {
                log_notice("task append_result error: %d", iRet);
                t->set_error(iRet, "DynamicHelperCollection", "append_result() error");
                t->reply_notify();
                return;
            }
        }
        t->reply_notify();
        return;
    }
    else
    {
        log_debug("reply from remote dtc error:%d", t->result_code());
        t->reply_notify();
        return;
    }
}

void DynamicHelperCollection::set_timer_handler(TimerList *recv,
                                              TimerList *conn, TimerList *retry)
{
    m_recvTimerList = recv;
    m_connTimerList = conn;
    m_retryTimerList = retry;

    attach_timer(m_retryTimerList);
}

void DynamicHelperCollection::timer_notify()
{
    for (HelperMapType::iterator i = m_groups.begin(); i != m_groups.end();)
    {
        if (i->second.used == 0)
        {
            delete i->second.group;
            m_groups.erase(i++);
        }
        else
        {
            i->second.used = 0;
            ++i;
        }
    }
}
