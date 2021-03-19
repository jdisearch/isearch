/*
 * =====================================================================================
 *
 *       Filename:  proxy_multi_request.cc
 *
 *    Description:  agent multi request.
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
#include "proxy_multi_request.h"
#include "task_request.h"
#include "proxy_client.h"
#include "table_def_manager.h"

extern DTCTableDefinition *gTableDef[];

AgentMultiRequest::AgentMultiRequest(TaskRequest *o) : packetCnt(0),
                                                       owner(o),
                                                       taskList(NULL),
                                                       compleTask(0),
                                                       ownerClient(NULL)
{
    if (o)
        ownerClient = o->owner_client();
}

AgentMultiRequest::~AgentMultiRequest()
{
    list_del();

    if (taskList)
    {
        for (int i = 0; i < packetCnt; i++)
            if (taskList[i].task)
                delete taskList[i].task;

        delete[] taskList;
    }

    if (!!packets)
        free(packets.ptr);
}

void AgentMultiRequest::complete_task(int index)
{
    if (taskList[index].task)
    {
        delete taskList[index].task;
        taskList[index].task = NULL;
    }

    compleTask++;
    /* delete owner taskrequest along with us if all sub request's result putted into ClientAgent's send queue */

    if (compleTask == packetCnt)
    {
        delete owner;
    }
}

void AgentMultiRequest::clear_owner_info()
{
    ownerClient = NULL;

    if (taskList == NULL)
        return;

    for (int i = 0; i < packetCnt; i++)
    {
        if (taskList[i].task)
            taskList[i].task->clear_owner_client();
    }
}

/*
error case: set this task processed
1. no mem: set task processed
2. decode error: set task processed, reply this task
*/
void AgentMultiRequest::DecodeOneRequest(char *packetstart, int packetlen, int index)
{
    int err = 0;
    TaskRequest *task = NULL;
    DecodeResult decoderes;

    task = new TaskRequest(TableDefinitionManager::Instance()->get_cur_table_def());
    if (NULL == task)
    {
        log_crit("not enough mem for new task creation, client wont recv response");
        compleTask++;
        return;
    }

    task->set_hotbackup_table(TableDefinitionManager::Instance()->get_hot_backup_table_def());
    decoderes = task->Decode(packetstart, packetlen, 2);
    switch (decoderes)
    {
    default:
    case DecodeFatalError:
        if (errno != 0)
            log_notice("decode fatal error, msg = %m");
        break;
    case DecodeDataError:
        task->response_timer_start();
        task->mark_as_hit();
        taskList[index].processed = 1;
        break;
    case DecodeDone:
        if ((err = task->prepare_process()) < 0)
        {
            log_error("build packed key error: %d, %s", err, task->resultInfo.error_message());
            taskList[index].processed = 1;
        }
        break;
    }

    task->set_owner_info(this, index, NULL);
    task->set_owner_client(this->ownerClient);

    taskList[index].task = task;

    return;
}

int AgentMultiRequest::decode_agent_request()
{
    int cursor = 0;

    taskList = new DecodedTask[packetCnt];
    if (NULL == taskList)
    {
        log_crit("no mem new taskList");
        return -1;
    }
    memset((void *)taskList, 0, sizeof(DecodedTask) * packetCnt);

    /* whether can work, reply on input buffer's correctness */
    for (int i = 0; i < packetCnt; i++)
    {
        char *packetstart;
        int packetlen;

        packetstart = packets.ptr + cursor;
        packetlen = packet_body_len(*(PacketHeader *)packetstart) + sizeof(PacketHeader);

        DecodeOneRequest(packetstart, packetlen, i);

        cursor += packetlen;
    }

    return 0;
}

void AgentMultiRequest::copy_reply_for_sub_task()
{
    for (int i = 0; i < packetCnt; i++)
    {
        if (taskList[i].task)
            taskList[i].task->copy_reply_path(owner);
    }
}
