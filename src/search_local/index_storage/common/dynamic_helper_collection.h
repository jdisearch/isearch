/*
 * =====================================================================================
 *
 *       Filename:  dynamic_helper_collection.h
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
#ifndef DYNAMIC_HELPER_COLLECTION_H__
#define DYNAMIC_HELPER_COLLECTION_H__

#include <map>
#include <string>
#include "task_request.h"

class HelperGroup;
class PollThread;
class TimerList;

class DynamicHelperCollection : public TaskDispatcher<TaskRequest>,
                                public ReplyDispatcher<TaskRequest>,
                                public TimerObject
{
public:
    DynamicHelperCollection(PollThread *owner, int clientPerGroup);
    ~DynamicHelperCollection();

    void set_timer_handler(TimerList *recv, TimerList *conn,
                         TimerList *retry);

private:
    virtual void task_notify(TaskRequest *t);
    virtual void reply_notify(TaskRequest *t);
    virtual void timer_notify();

    struct helper_group
    {
        HelperGroup *group;
        int used;
    };

    typedef std::map<std::string, helper_group> HelperMapType;
    HelperMapType m_groups;
    LinkQueue<TaskRequest *>::allocator m_taskQueueAllocator;
    TimerList *m_recvTimerList, *m_connTimerList, *m_retryTimerList;
    int m_clientPerGroup;
};

#endif
