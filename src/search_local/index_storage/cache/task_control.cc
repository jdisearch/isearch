/*
 * =====================================================================================
 *
 *       Filename:  task_control.cc
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
#include <task_control.h>
#include <log.h>
#include "protocol.h"

TaskControl *TaskControl::serverControl = NULL;

TaskControl::TaskControl(PollThread *o) : TaskDispatcher<TaskRequest>(o), m_output(o)
{
    atomic8_set(&m_readOnly, 0);
    m_statReadonly = statmgr.get_item_u32(SERVER_READONLY);
    m_statReadonly.set((0 == atomic8_read(&m_readOnly)) ? 0 : 1);
}

TaskControl::~TaskControl(void)
{
}

TaskControl *TaskControl::get_instance(PollThread *o)
{
    if (NULL == serverControl)
    {
        NEW(TaskControl(o), serverControl);
    }
    return serverControl;
}

TaskControl *TaskControl::get_instance()
{
    return serverControl;
}

bool TaskControl::is_read_only()
{
    return 0 != atomic8_read(&m_readOnly);
}
void TaskControl::query_mem_info(TaskRequest *cur)
{
    struct DTCServerInfo s_info;
    memset(&s_info, 0x00, sizeof(s_info));

    s_info.version = 0x1;
    s_info.datasize = statmgr.get10_s_item_value(DTC_DATA_SIZE);
    s_info.memsize = statmgr.get10_s_item_value(DTC_CACHE_SIZE);
    log_debug("Memory info is: memsize is %lu , datasize is %lu", s_info.memsize, s_info.datasize);
    cur->resultInfo.set_server_info(&s_info);
}
void TaskControl::deal_server_admin(TaskRequest *cur)
{
    switch (cur->requestInfo.admin_code())
    {
    case DRequest::ServerAdminCmd::SET_READONLY:
    {
        atomic8_set(&m_readOnly, 1);
        m_statReadonly.set(1);
        log_info("set server status to readonly.");
        break;
    }
    case DRequest::ServerAdminCmd::SET_READWRITE:
    {
        atomic8_set(&m_readOnly, 0);
        m_statReadonly.set(0);
        log_info("set server status to read/write.");
        break;
    }
    case DRequest::ServerAdminCmd::QUERY_MEM_INFO:
    {
        log_debug("query meminfo.");
        query_mem_info(cur);
        break;
    }

    default:
    {
        log_debug("unknow cmd: %d", cur->requestInfo.admin_code());
        cur->set_error(-EC_REQUEST_ABORTED, "RequestControl", "Unknown svrAdmin command.");
        break;
    }
    }

    cur->reply_notify();
}

void TaskControl::task_notify(TaskRequest *cur)
{
    log_debug("TaskControl::task_notify Cmd is %d, AdminCmd is %u", cur->request_code(), cur->requestInfo.admin_code());
    //处理ServerAdmin命令
    if (DRequest::SvrAdmin == cur->request_code())
    {
        switch (cur->requestInfo.admin_code())
        {
        case DRequest::ServerAdminCmd::SET_READONLY:
        case DRequest::ServerAdminCmd::SET_READWRITE:
        case DRequest::ServerAdminCmd::QUERY_MEM_INFO:
            deal_server_admin(cur);
            return;

            //allow all admin_code pass
        default:
        {
            log_debug("TaskControl::task_notify admincmd,  tasknotify next process ");
            m_output.task_notify(cur);
            return;
        }
        }
    }

    //当server为readonly，对非查询请求直接返回错误
    if (0 != atomic8_read(&m_readOnly))
    {
        if (DRequest::Get != cur->request_code())
        {
            log_info("server is readonly, reject write operation");
            cur->set_error(-EC_SERVER_READONLY, "RequestControl", "Server is readonly.");
            cur->reply_notify();
            return;
        }
    }
    log_debug("TaskControl::task_notify tasknotify next process ");
    m_output.task_notify(cur);
}
