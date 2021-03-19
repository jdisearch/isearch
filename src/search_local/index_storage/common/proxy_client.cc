/*
 * =====================================================================================
 *
 *       Filename:  proxy_client.cc
 *
 *    Description:  agent client class.
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
#include <errno.h>

#include "proxy_client.h"
#include "proxy_client_unit.h"
#include "proxy_receiver.h"
#include "proxy_sender.h"
#include "log.h"
#include "poll_thread.h"
#include "proxy_multi_request.h"
#include "packet.h"
#include "task_request.h"
#include "proxy_multi_request.h"
#include "stat_dtc.h"
#include "table_def_manager.h"

extern DTCTableDefinition *gTableDef[];

static StatItemU32 statAgentAcceptCount;
static StatItemU32 statAgentCurConnCount;
static StatItemU32 statAgentExpireCount;
static StatItemU32 statTaskClientTimeout;

AgentResultQueue::~AgentResultQueue()
{
    Packet *p;

    while (NULL != (p = packet.Pop()))
    {
        p->free_result_buff();
        delete p;
    }
}

class AgentReply : public ReplyDispatcher<TaskRequest>
{
public:
    AgentReply()
    {
        statInitFlag = false;
    }
    virtual ~AgentReply() {}
    virtual void reply_notify(TaskRequest *task);

private:
    bool statInitFlag;
};

void AgentReply::reply_notify(TaskRequest *task)
{
    log_debug("AgentReply::reply_notify start");

    ClientAgent *client = task->owner_client();
    if (client == NULL)
    {
        /* client gone, finish this task */
        task->done_one_agent_sub_request();
        return;
    }

    client->record_request_process_time(task);

    int client_timeout = task->requestInfo.tag_present(1) == 0 ? task->default_expire_time() : task->requestInfo.get_expire_time(task->versionInfo.CTLibIntVer());
    int req_delaytime = task->responseTimer.live();

    if (!statInitFlag)
    {
        statAgentExpireCount = statmgr.get_item_u32(INCOMING_EXPIRE_REQ);
        statTaskClientTimeout = statmgr.get_item_u32(TASK_CLIENT_TIMEOUT);
        statInitFlag = true;
    }

    statTaskClientTimeout = client_timeout;
    log_debug("task client_timeout: %d", client_timeout);

    if ((req_delaytime / 1000) >= client_timeout) //ms
    {
        log_debug("AgentReply::reply_notify client_timeout[%d]ms, req delay time[%d]us", client_timeout, req_delaytime);
        task->done_one_agent_sub_request();
        statAgentExpireCount++;
        return;
    }
    Packet *packet = new Packet();
    if (packet == NULL)
    {
        /* make response error, finish this task */
        task->done_one_agent_sub_request();
        log_crit("no mem new Packet");
        return;
    }

    packet->encode_result(task);
    task->detach_result_in_result_writer();
    task->done_one_agent_sub_request();

    client->add_packet(packet);
    if (client->send_result() < 0)
    {
        log_error("cliengAgent send_result error");
        delete client;
        return;
    }

    log_debug("AgentReply::reply_notify stop");
}

static AgentReply agentReply;

/* sender and receiver should inited ok */
ClientAgent::ClientAgent(PollThread *o, AgentClientUnit *u, int fd) : PollerObject(o, fd),
                                                                      ownerThread(o),
                                                                      owner(u),
                                                                      tlist(NULL)
{
    tlist = u->get_timer_list();
    sender = new AgentSender(fd);
    if (NULL == sender)
    {
        log_error("no mem to new sender");
        throw(int) - ENOMEM;
    }

    if (sender && sender->Init() < 0)
    {
        delete sender;
        sender = NULL;
        log_error("no mem to init sender");
        throw(int) - ENOMEM;
    }

    if (sender)
    {
        receiver = new AgentReceiver(fd);
        if (NULL == receiver)
        {
            log_error("no mem to new receiver");
            throw(int) - ENOMEM;
        }

        if (receiver && receiver->Init() < 0)
        {
            log_error("no mem to init receiver");
            throw(int) - ENOMEM;
        }
    }

    statAgentAcceptCount = statmgr.get_item_u32(AGENT_ACCEPT_COUNT);
    statAgentCurConnCount = statmgr.get_item_u32(AGENT_CONN_COUNT);

    statAgentAcceptCount++;
    statAgentCurConnCount++;
}

ClientAgent::~ClientAgent()
{
    log_debug("~ClientAgent start");
    ListObject<AgentMultiRequest> *node = rememberReqHeader.ListNext();
    AgentMultiRequest *req;

    /* notify all request of this client I'm gone */
    while (node != &rememberReqHeader)
    {
        req = node->ListOwner();
        req->clear_owner_info();
        req->detach_from_owner_client();
        node = rememberReqHeader.ListNext();
    }

    if (receiver)
        delete receiver;
    if (sender)
        delete sender;

    detach_poller();

    statAgentCurConnCount--;
    log_debug("~ClientAgent end");
}

int ClientAgent::attach_thread()
{
    disable_output();
    enable_input();

    if (attach_poller() < 0)
    {
        log_error("client agent attach agengInc thread failed");
        return -1;
    }

    /* no idle test */
    return 0;
}

void ClientAgent::remember_request(TaskRequest *request)
{
    request->link_to_owner_client(rememberReqHeader);
}

TaskRequest *ClientAgent::prepare_request(char *recvbuff, int recvlen, int pktcnt)
{
    TaskRequest *request;

    request = new TaskRequest(TableDefinitionManager::Instance()->get_cur_table_def());
    if (NULL == request)
    {
        free(recvbuff);
        log_crit("no mem allocate for new agent request");
        return NULL;
    }

    request->set_hotbackup_table(TableDefinitionManager::Instance()->get_hot_backup_table_def());
    request->set_owner_info(this, 0, NULL);
    request->set_owner_client(this);
    request->push_reply_dispatcher(&agentReply);
    request->save_recved_result(recvbuff, recvlen, pktcnt);

    /* assume only a few sub request decode error */
    if (request->decode_agent_request() < 0)
    {
        delete request;
        return NULL;
    }

    /* no mem new task case */
    if (request->is_agent_request_completed())
    {
        delete request;
        return NULL;
    }

    remember_request(request);

    return request;
}

int ClientAgent::recv_request()
{
    RecvedPacket packets;
    char *recvbuff = NULL;
    int recvlen = 0;
    int pktcnt = 0;
    TaskRequest *request = NULL;

    packets = receiver->Recv();

    if (packets.err < 0)
        return -1;
    else if (packets.pktCnt == 0)
        return 0;

    recvbuff = packets.buff;
    recvlen = packets.len;
    pktcnt = packets.pktCnt;

    request = prepare_request(recvbuff, recvlen, pktcnt);
    if (request != NULL)
        owner->task_notify(request);

    return 0;
}

/* exit when recv error*/
void ClientAgent::input_notify()
{
    log_debug("ClientAgent::input_notify() start");
    if (recv_request() < 0)
    {
        log_debug("erro when recv");
        delete this;
        return;
    }
    delay_apply_events();
    log_debug("ClientAgent::input_notify() stop");
    return;
}

/*
return error if sender broken
*/
int ClientAgent::send_result()
{
    if (sender->is_broken())
    {
        log_error("sender broken");
        return -1;
    }

    while (1)
    {

        Packet *frontPkt = resQueue.Front();
        if (NULL == frontPkt)
        {

            break;
        }

        if (frontPkt->vec_count() + sender->vec_count() > SENDER_MAX_VEC)
        {
            /*这个地方打印error，如果在10s内会5次走入此次分支的话，统计子进程会上报告警*/
            log_error("the sum value of front packet veccount[%d] and sender veccount[%d]is greater than SENDER_MAX_VEC[%d]",
                      frontPkt->vec_count(), sender->vec_count(), SENDER_MAX_VEC);
            break;
        }
        else
        {
            Packet *pkt;
            pkt = resQueue.Pop();
            if (NULL == pkt)
            {
                break;
            }
            if (sender->add_packet(pkt) < 0)
            {
                return -1;
            }
        }
    }

    if (sender->Send() < 0)
    {
        log_error("agent client send error");
        return -1;
    }

    if (sender->left_len() != 0)
        enable_output();
    else
        disable_output();

    delay_apply_events();

    return 0;
}

void ClientAgent::output_notify()
{
    log_debug("ClientAgent::output_notify() start");
    if (send_result() < 0)
    {
        log_debug("error when response");
        delete this;
        return;
    }
    log_debug("ClientAgent::output_notify() stop");

    return;
}

void ClientAgent::record_request_process_time(TaskRequest *task)
{
    owner->record_request_time(task);
}
