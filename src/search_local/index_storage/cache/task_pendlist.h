/*
 * =====================================================================================
 *
 *       Filename:  task_pendlist.h
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
#ifndef __TASK_REQUEST_PENDINGLIST_H
#define __TASK_REQUEST_PENDINGLIST_H

#include "timer_list.h"
#include "namespace.h"
#include "task_request.h"
#include <list>

DTC_BEGIN_NAMESPACE
/*
 * 请求挂起列表。
 *
 * 如果发现请求暂时没法满足，则挂起，直到
 *     1. 超时
 *     2. 条件满足被唤醒
 */
class BufferProcess;
class CacheBase;
class TaskReqeust;
class TimerObject;
class TaskPendingList : private TimerObject
{
public:
    TaskPendingList(TaskDispatcher<TaskRequest> *o, int timeout = 5);
    ~TaskPendingList();

    void add2_list(TaskRequest *); //加入pending list
    void Wakeup(void);            //唤醒队列中的所有task

private:
    virtual void timer_notify(void);

private:
    TaskPendingList(const TaskPendingList &);
    const TaskPendingList &operator=(const TaskPendingList &);

private:
    int _timeout;
    TimerList *_timelist;
    TaskDispatcher<TaskRequest> *_owner;
    int _wakeup;
    typedef std::pair<TaskRequest *, time_t> slot_t;
    std::list<slot_t> _pendlist;
};

DTC_END_NAMESPACE

#endif
