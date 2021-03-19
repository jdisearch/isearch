/*
 * =====================================================================================
 *
 *       Filename:  helper_group.cc
 *
 *    Description:  database collection group helper.
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
#include <stdio.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/socket.h>

#include "list.h"
#include "dbconfig.h"
#include "helper_client.h"
#include "helper_group.h"
#include "log.h"
#include "unix_socket.h"

static StatItemU32 statHelperExpireCount;

static void IncHelperExpireCount()
{
	statHelperExpireCount = statmgr.get_item_u32(DATA_SOURCE_EXPIRE_REQ);
	statHelperExpireCount++;
}

class HelperClientList : public ListObject<HelperClientList>
{
public:
	HelperClientList() : helper(NULL) {}
	~HelperClientList() { DELETE(helper); }
	HelperClient *helper;
};

HelperGroup::HelperGroup(const char *s, const char *name_, int hc, int qs,
						 int statIndex) : TaskDispatcher<TaskRequest>(NULL),
										  queueSize(qs),
										  helperCount(0),
										  helperMax(hc),
										  readyHelperCnt(0),
										  fallback(NULL),
										  average_delay(0) /*默认时延为0*/
{
	sockpath = strdup(s);
	freeHelper.InitList();
	helperList = new HelperClientList[hc];

	recvList = connList = retryList = NULL;

	strncpy(name, name_, sizeof(name) - 1);
	name[sizeof(name) - 1] = '0';

	statTime[0] = statmgr.get_sample(statIndex);
	statTime[1] = statmgr.get_sample(statIndex + 1);
	statTime[2] = statmgr.get_sample(statIndex + 2);
	statTime[3] = statmgr.get_sample(statIndex + 3);
	statTime[4] = statmgr.get_sample(statIndex + 4);
	statTime[5] = statmgr.get_sample(statIndex + 5);
}

HelperGroup::~HelperGroup()
{
	DELETE_ARRAY(helperList);
	free(sockpath);
}

void HelperGroup::record_response_delay(unsigned int t)
{
	if (t <= 0)
		t = 1;

	if (average_delay == 0)
		average_delay = t;

	double N = 20E6 / (average_delay + t);

	if ((unsigned)N > 200000)
		N = 200000; /* 2w/s */
	if ((unsigned)N < 5)
		N = 5; /* 0.5/s */

	average_delay = ((N - 1) / N) * average_delay + t / N;
}

void HelperGroup::add_ready_helper()
{
	if (readyHelperCnt == 0)
	{
		log_info("helper_group-%s switch to ONLINE mode", sockpath);
		/* force flush task */
		attach_ready_timer(owner);
	}

	if (readyHelperCnt < helperMax)
		readyHelperCnt++;

	if (readyHelperCnt == helperMax)
		log_debug("%s", "all client ready");
}

void HelperGroup::dec_ready_helper()
{
	if (readyHelperCnt > 0)
		readyHelperCnt--;
	if (readyHelperCnt == 0)
	{
		log_error("helper_group-%s all clients invalid, switch to OFFLINE mode", sockpath);
		/* reply all task */
		attach_ready_timer(owner);
	}
}

void HelperGroup::record_process_time(int cmd, unsigned int usec)
{
	static const unsigned char cmd2type[] =
		{
			/*Nop*/ 0,
			/*result_code*/ 0,
			/*DTCResultSet*/ 0,
			/*HelperAdmin*/ 0,
			/*Get*/ 1,
			/*Purge*/ 0,
			/*Insert*/ 2,
			/*Update*/ 3,
			/*Delete*/ 4,
			/*Replace*/ 5,
			/*Flush*/ 0,
			/*Other*/ 0,
			/*Other*/ 0,
			/*Other*/ 0,
			/*Other*/ 0,
			/*Other*/ 0,
			/*Other*/ 0,	
			/*Replicate*/ 1,
			/*LocalMigrate*/ 1,
		};
	statTime[0].push(usec);
	int t = cmd2type[cmd];
	if (t)
		statTime[t].push(usec);

	/* 计算新的平均时延 */
	record_response_delay(usec);
}

int HelperGroup::Attach(PollThread *thread, LinkQueue<TaskRequest *>::allocator *a)
{
	owner = thread;
	for (int i = 0; i < helperMax; i++)
	{
		helperList[i].helper = new HelperClient(owner, this, i);
		helperList[i].helper->Reconnect();
	}

	queue.SetAllocator(a);
	return 0;
}

int HelperGroup::connect_helper(int fd)
{
	struct sockaddr_un unaddr;
	socklen_t addrlen;

	addrlen = init_unix_socket_address(&unaddr, sockpath);
	return connect(fd, (struct sockaddr *)&unaddr, addrlen);
}

void HelperGroup::queue_back_task(TaskRequest *task)
{
	if (queue.Unshift(task) < 0)
	{
		task->set_error(-EC_SERVER_ERROR, __FUNCTION__, "insufficient memory");
		task->reply_notify();
	}
}

void HelperGroup::request_completed(HelperClient *h)
{
	HelperClientList *h0 = &helperList[h->helperIdx];
	if (h0->ListEmpty())
	{
		h0->ListAdd(freeHelper);
		helperCount--;
		attach_ready_timer(owner);
	}
}

void HelperGroup::connection_reset(HelperClient *h)
{
	HelperClientList *h0 = &helperList[h->helperIdx];
	if (!h0->ListEmpty())
	{
		h0->list_del();
		helperCount++;
	}
}

void HelperGroup::check_queue_expire(void)
{
	attach_ready_timer(owner);
}

void HelperGroup::process_task(TaskRequest *task)
{
	if (DRequest::ReloadConfig == task->request_code() && TaskTypeHelperReloadConfig == task->request_type())
	{
		process_reload_config(task);
		return;
	}
	HelperClientList *h0 = freeHelper.NextOwner();
	HelperClient *helper = h0->helper;

	log_debug("process task.....");
	if (helper->support_batch_key())
		task->mark_field_set_with_key();

	Packet *packet = new Packet;
	if (packet->encode_forward_request(task) != 0)
	{
		delete packet;
		log_error("[2][task=%d]request error: %m", task->Role());
		task->set_error(-EC_BAD_SOCKET, "ForwardRequest", NULL);
		task->reply_notify();
	}
	else
	{
		h0->ResetList();
		helperCount++;

		helper->attach_task(task, packet);
	}
}

void HelperGroup::flush_task(uint64_t now)
{
	//check timeout for helper client
	while (1)
	{
		TaskRequest *task = queue.Front();
		if (task == NULL)
			break;

		if (readyHelperCnt == 0)
		{
			log_debug("no available helper, up stream server maybe offline");
			queue.Pop();
			task->set_error(-EC_UPSTREAM_ERROR, __FUNCTION__,
							"no available helper, up stream server maybe offline");
			task->reply_notify();
			continue;
		}
		else if (task->is_expired(now))
		{
			log_debug("%s", "task is expired in HelperGroup queue");
			IncHelperExpireCount();
			queue.Pop();
			task->set_error(-ETIMEDOUT, __FUNCTION__, "task is expired in HelperGroup queue");
			task->reply_notify();
		}
		else if (!freeHelper.ListEmpty())
		{
			queue.Pop();
			process_task(task);
		}
		else if (fallback && fallback->has_free_helper())
		{
			queue.Pop();
			fallback->process_task(task);
		}
		else
		{
			break;
		}
	}
}

void HelperGroup::timer_notify(void)
{
	uint64_t v = GET_TIMESTAMP() / 1000;
	attach_timer(retryList);
	flush_task(v);
}

int HelperGroup::accept_new_request_fail(TaskRequest *task)
{
	unsigned work_client = helperMax;
	unsigned queue_size = queue.Count();

	/* queue至少排队work_client个任务 */
	if (queue_size <= work_client)
		return 0;

	uint64_t wait_time = queue_size * (uint64_t)average_delay;
	wait_time /= work_client; /* us */
	wait_time /= 1000;		  /* ms */

	uint64_t now = GET_TIMESTAMP() / 1000;

	if (task->is_expired(now + wait_time))
		return 1;

	return 0;
}

void HelperGroup::task_notify(TaskRequest *task)
{
	log_debug("HelperGroup::task_notify()");

	if (DRequest::ReloadConfig == task->request_code() && TaskTypeHelperReloadConfig == task->request_type())
	{
		group_notify_helper_reload_config(task);
		return;
	}

	uint64_t now = GET_TIMESTAMP() / 1000; /* ms*/
	flush_task(now);

	if (readyHelperCnt == 0)
	{
		log_debug("no available helper, up stream server maybe offline");
		task->set_error(-EC_UPSTREAM_ERROR, __FUNCTION__, "no available helper, up stream server maybe offline");
		task->reply_notify();
	}
	else if (task->is_expired(now))
	{
		log_debug("task is expired when sched to HelperGroup");
		IncHelperExpireCount();
		//modify error message
		task->set_error(-ETIMEDOUT, __FUNCTION__, "task is expired when sched to HelperGroup,by DTC");
		task->reply_notify();
	}
	else if (!freeHelper.ListEmpty())
	{
		/* has free helper, sched task */
		process_task(task);
	}
	else
	{
		if (fallback && fallback->has_free_helper())
		{
			fallback->process_task(task);
		}
		else if (accept_new_request_fail(task))
		{
			/* helper 响应变慢，主动踢掉task */
			log_debug("HelperGroup response is slow, give up current task");
			IncHelperExpireCount();
			task->set_error(-EC_SERVER_BUSY, __FUNCTION__, "DB response is very slow, give up current task");
			task->reply_notify();
		}
		else if (!queue_full())
		{
			queue.Push(task);
		}
		else
		{
			/* no free helper */
			task->set_error(-EC_SERVER_BUSY, __FUNCTION__, "No available helper connections");
			log_error("No available helper queue slot,count=%d, max=%d",
					  queue.Count(), queueSize);

			task->reply_notify();
		}
	}
}

void HelperGroup::dump_state(void)
{
	log_info("HelperGroup %s count %d/%d", Name(), helperCount, helperMax);
	int i;
	for (i = 0; i < helperMax; i++)
	{
		log_info("helper %d state %s\n", i, helperList[i].helper->state_string());
	}
}

void HelperGroup::group_notify_helper_reload_config(TaskRequest *task)
{
	//进入到这一步，helper应该是全部处于空闲状态的
	if (!freeHelper.ListEmpty())
		process_task(task);
	else if (fallback && fallback->has_free_helper())
		fallback->process_task(task);
	else
	{
		log_error("not have available helper client, please check!");
		task->set_error(-EC_NOT_HAVE_AVAILABLE_HELPERCLIENT, __FUNCTION__, "not have available helper client");
	}
}

void HelperGroup::process_reload_config(TaskRequest *task)
{
	typedef std::vector<HelperClientList *> HELPERCLIENTLISTVEC;
	typedef std::vector<HelperClientList *>::iterator HELPERCLIENTVIT;
	HELPERCLIENTLISTVEC clientListVec;
	HelperClientList *head = freeHelper.ListOwner();
	for (HelperClientList *pos = head->NextOwner(); pos != head; pos = pos->NextOwner())
	{
		clientListVec.push_back(pos);
	}
	for (HELPERCLIENTVIT vit = clientListVec.begin(); vit != clientListVec.end(); ++vit)
	{
		HelperClientList *pHList = (*vit);
		HelperClient *pHelper = pHList->helper;
		pHList->ResetList();
		helperCount++;
		pHelper->client_notify_helper_reload_config();
	}

	log_error("helpergroup [%s] notify work helper reload config finished!", Name());
}
