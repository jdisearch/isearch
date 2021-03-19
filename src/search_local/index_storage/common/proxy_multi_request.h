/*
 * =====================================================================================
 *
 *       Filename:  proxy_multi_request.h
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
#ifndef AGENT_MULTI_REQUEST_H___
#define AGENT_MULTI_REQUEST_H___

#include "list.h"
#include "value.h"

class TaskRequest;
typedef struct
{
	TaskRequest *volatile task;
	volatile int processed;
} DecodedTask;

class ClientAgent;
class AgentMultiRequest : public ListObject<AgentMultiRequest>
{
public:
	AgentMultiRequest(TaskRequest *o);
	virtual ~AgentMultiRequest();

	int decode_agent_request();
	inline int packet_count() { return packetCnt; }
	inline TaskRequest *curr_task(int index) { return taskList[index].task; }
	void copy_reply_for_sub_task();
	void clear_owner_info();
	inline bool is_completed() { return compleTask == packetCnt; }
	void complete_task(int index);
	inline void detach_from_owner_client() { list_del(); }
	inline bool is_curr_task_processed(int index) { return taskList[index].processed == 1; }
	inline void save_recved_result(char *buff, int len, int pktcnt)
	{
		packets.Set(buff, len);
		packetCnt = pktcnt;
	}

private:
	DTCBinary packets;
	int packetCnt;
	TaskRequest *owner;
	DecodedTask *volatile taskList;
	volatile int compleTask;
	ClientAgent *volatile ownerClient;
	void DecodeOneRequest(char *packetstart, int packetlen, int index);
};

#endif
