/*
 * =====================================================================================
 *
 *       Filename:  task_request.h
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
#ifndef __H_DTC_REQUEST_REAL_H__
#define __H_DTC_REQUEST_REAL_H__

#include "request_base_all.h"
#include "task_base.h"
#include "stop_watch.h"
#include "hotback_task.h"
class DecoderUnit;
class MultiRequest;
class NCKeyValueList;
class NCRequest;
class AgentMultiRequest;
class ClientAgent;

class TaskOwnerInfo
{
private:
	const struct sockaddr *clientaddr;
	void *volatile ownerInfo;
	int ownerIndex;

public:
	TaskOwnerInfo(void) : clientaddr(NULL), ownerInfo(NULL), ownerIndex(0) {}
	virtual ~TaskOwnerInfo(void) {}

	void set_owner_info(void *info, int idx, const struct sockaddr *addr)
	{
		ownerInfo = info;
		ownerIndex = idx;
		clientaddr = addr;
	}

	inline void Clean() { ownerInfo = NULL; }

	void clear_owner_info(void) { ownerInfo = NULL; }

	template <typename T>
	T *OwnerInfo(void) const { return (T *)ownerInfo; }

	const struct sockaddr *OwnerAddress(void) const { return clientaddr; }
	int owner_index(void) const { return ownerIndex; }
};

class TaskRequest : public DTCTask, public TaskReplyList<TaskRequest, 10>, public TaskOwnerInfo
{
public:
	TaskRequest(DTCTableDefinition *t = NULL) : DTCTask(t, TaskRoleServer),
												blacklist_size(0),
												timestamp(0),
												barHash(0),
												packedKey(NULL),
												expire_time(0),
												multiKey(NULL),
												keyList(NULL),
												batchKey(NULL),
												agentMultiReq(NULL),
												ownerClient(NULL),
												recvBuff(NULL),
												recvLen(0),
												recvPacketCnt(0),
												resourceId(0),
												resourceOwner(NULL),
												resourceSeq(0){};

	virtual ~TaskRequest();

	inline TaskRequest(const TaskRequest &rq)
	{
		TaskRequest();
		Copy(rq);
	}

	int Copy(const TaskRequest &rq);
	int Copy(const TaskRequest &rq, const DTCValue *newkey);
	int Copy(const RowValue &);
	int Copy(NCRequest &, const DTCValue *);

public:
	void Clean();
	// msecond: absolute ms time
	uint64_t default_expire_time(void) { return 5000 /*default:5000ms*/; }
	const uint64_t get_expire_time(void) const { return expire_time; }

	int is_expired(uint64_t now) const
	{
		// client gone, always expired
		if (OwnerInfo<void>() == NULL)
			return 1;
		// flush cmd never time out
		if (requestType == TaskTypeCommit)
			return 0;
		return expire_time <= now;
	}
	uint32_t Timestamp(void) const { return timestamp; }
	void renew_timestamp(void) { timestamp = time(NULL); }

	const char *packed_key(void) const { return packedKey; }
	unsigned long barrier_key(void) const { return barHash; }
	DTCValue *multi_key_array(void) { return multiKey; }

public:
	int build_packed_key(void);
	void calculate_barrier_key(void);

	int prepare_process(void);
	int update_packed_key(uint64_t);
	void push_black_list_size(const unsigned size) { blacklist_size = size; }
	unsigned pop_black_list_size(void)
	{
		register unsigned ret = blacklist_size;
		blacklist_size = 0;
		return ret;
	}

	void dump_update_info(const char *prefix) const;
	void update_key(RowValue &r)
	{
		if (multiKey)
			r.copy_value(multiKey, 0, key_fields());
		else
			r[0] = *request_key();
	}
	void update_key(RowValue *r)
	{
		if (r)
			update_key(*r);
	}
	int update_row(RowValue &row)
	{
		row.update_timestamp(timestamp,
							requestCode != DRequest::Update ||
								(updateInfo && updateInfo->has_type_commit()));
		return DTCTask::update_row(row);
	}

	void set_remote_addr(const char *addr)
	{
		strncpy(remoteAddr, addr, sizeof(remoteAddr));
		remoteAddr[sizeof(remoteAddr) - 1] = 0;
	}

	const char *remote_addr() { return remoteAddr; }
	HotBackTask &get_hot_back_task()
	{
		return hotbacktask;
	}

private:
	/* following filed should be clean:
	 * blacklist_size
	 * timestamp
	 * barHash
	 * expire_time
	 *
	 * */
	/* 加入黑名单的大小 */
	unsigned blacklist_size;
	uint32_t timestamp;

	unsigned long barHash;
	char *packedKey;
	char packedKeyBuf[8];
	uint64_t expire_time; /* ms */ /* derived from packet */
	DTCValue *multiKey;
	char remoteAddr[32];

private:
	int build_multi_key_values(void);
	int build_single_string_key(void);
	int build_single_int_key(void);
	int build_multi_int_key(void);

	void free_packed_key(void)
	{
		if (packedKey && packedKey != packedKeyBuf)
			FREE_CLEAR(packedKey);
		FREE_IF(multiKey);
	}
	/* for batch request*/
private:
	const NCKeyValueList *keyList;
	/* need clean when task begin in use(deleted when batch request finished) */
	MultiRequest *batchKey;

	/* for agent request */
private:
	AgentMultiRequest *agentMultiReq;
	ClientAgent *ownerClient;
	char *recvBuff;
	int recvLen;
	int recvPacketCnt;

public:
	unsigned int key_val_count() const { return versionInfo.get_tag(11) ? versionInfo.get_tag(11)->s64 : 1; }
	const Array *key_type_list() const { return (Array *)&(versionInfo.get_tag(12)->bin); }
	const Array *key_name_list() const { return (Array *)&(versionInfo.get_tag(13)->bin); }
	const Array *key_val_list() const { return (Array *)&(requestInfo.Key()->bin); }
	const NCKeyValueList *internal_key_val_list() const { return keyList; }

	int is_batch_request(void) const { return batchKey != NULL; }
	int get_batch_size(void) const;
	void set_batch_key_list(MultiRequest *bk) { batchKey = bk; }
	MultiRequest *get_batch_key_list(void) { return batchKey; }
	int set_batch_cursor(int i);
	void done_batch_cursor(int i);

public:
	/* for agent request */
	void set_owner_client(ClientAgent *client);
	ClientAgent *owner_client();
	void clear_owner_client();

	int decode_agent_request();
	inline void save_recved_result(char *buff, int len, int pktcnt)
	{
		recvBuff = buff;
		recvLen = len;
		recvPacketCnt = pktcnt;
	}
	bool is_agent_request_completed();
	void done_one_agent_sub_request();

	void link_to_owner_client(ListObject<AgentMultiRequest> &head);

	int agent_sub_task_count();
	void copy_reply_for_agent_sub_task();
	TaskRequest *curr_agent_sub_task(int index);

	bool is_curr_sub_task_processed(int index);

private:
	void pass_recved_result_to_agent_muti_req();

public: // timing
	stopwatch_usec_t responseTimer;
	void response_timer_start(void) { responseTimer.start(); }
	unsigned int resourceId;
	DecoderUnit *resourceOwner;
	uint32_t resourceSeq;

private:
	/*use as async hotback task*/
	HotBackTask hotbacktask;
};

#endif
