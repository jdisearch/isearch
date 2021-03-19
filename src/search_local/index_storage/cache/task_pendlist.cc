/*
 * =====================================================================================
 *
 *       Filename:  task_pendlist.cc
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
#include "task_pendlist.h"
#include "buffer_process.h"
#include "log.h"

DTC_USING_NAMESPACE

TaskPendingList::TaskPendingList(TaskDispatcher<TaskRequest> *o, int to) : _timeout(to),
                                                                           _timelist(0),
                                                                           _owner(o),
                                                                           _wakeup(0)
{
    _timelist = _owner->owner->get_timer_list(_timeout);
}

TaskPendingList::~TaskPendingList()
{
    std::list<slot_t>::iterator it;
    for (it = _pendlist.begin(); it != _pendlist.end(); ++it)
    {
        //把所有请求踢回客户端
        it->first->set_error(-ETIMEDOUT, __FUNCTION__, "object deconstruct");
        it->first->reply_notify();
    }
}

void TaskPendingList::add2_list(TaskRequest *task)
{

    if (task)
    {
        if (_pendlist.empty())
            attach_timer(_timelist);

        _pendlist.push_back(std::make_pair(task, time(NULL)));
    }

    return;
}

// 唤醒队列中所有已经pending的task
void TaskPendingList::Wakeup(void)
{

    log_debug("TaskPendingList Wakeup");

    //唤醒所有task
    _wakeup = 1;

    attach_ready_timer(_owner->owner);

    return;
}

void TaskPendingList::timer_notify(void)
{

    std::list<slot_t> copy;
    copy.swap(_pendlist);
    std::list<slot_t>::iterator it;

    if (_wakeup)
    {
        for (it = copy.begin(); it != copy.end(); ++it)
        {
            _owner->task_notify(it->first);
        }

        _wakeup = 0;
    }
    else
    {

        time_t now = time(NULL);

        for (it = copy.begin(); it != copy.end(); ++it)
        {
            //超时处理
            if (it->second + _timeout >= now)
            {
                _pendlist.push_back(*it);
            }
            else
            {
                it->first->set_error(-ETIMEDOUT, __FUNCTION__, "pending task is timedout");
                it->first->reply_notify();
            }
        }

        if (!_pendlist.empty())
            attach_timer(_timelist);
    }

    return;
}
