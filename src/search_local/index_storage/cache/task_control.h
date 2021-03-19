/*
 * =====================================================================================
 *
 *       Filename:  task_control.h
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
#ifndef __REQUEST_CONTROL_H
#define __REQUEST_CONTROL_H

#include <task_request.h>
#include <stat_dtc.h>

class TaskControl : public TaskDispatcher<TaskRequest>
{
protected:
    static TaskControl *serverControl;
    TaskControl(PollThread *o);

public:
    //返回实例，如果实例尚未构造，则构造一个新的实例返回
    static TaskControl *get_instance(PollThread *o);
    //仅是返回，如果实例尚未构造，则返回空
    static TaskControl *get_instance();
    virtual ~TaskControl(void);
    void bind_dispatcher(TaskDispatcher<TaskRequest> *p) { m_output.bind_dispatcher(p); }
    bool is_read_only();

private:
    RequestOutput<TaskRequest> m_output;
    //server是否为只读状态
    atomic8_t m_readOnly;
    //Readonly的统计对象
    StatItemU32 m_statReadonly;

private:
    virtual void task_notify(TaskRequest *);
    //处理serveradmin 命令
    void deal_server_admin(TaskRequest *cur);
    void query_mem_info(TaskRequest *cur);
};

#endif
