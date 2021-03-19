/*
 * =====================================================================================
 *
 *       Filename:  multi_request.h
 *
 *    Description:  multi task requests.
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
#ifndef __H_MULTI_REQUEST_H__
#define __H_MULTI_REQUEST_H__

#include "request_base_all.h"

class TaskMultiplexer;
class TaskRequest;
union DTCValue;

class MultiRequest
{
private:
	TaskMultiplexer *owner;
	TaskRequest *wait;
	DTCValue *keyList;
	unsigned char *keyMask;
	int doneReq;
	int totalReq;
	int subReq;
	int firstPass;
	int keyFields;
	int internal;

public:
	friend class TaskMultiplexer;
	MultiRequest(TaskMultiplexer *o, TaskRequest *task);
	~MultiRequest(void);

	int decode_key_list(void);
	int split_task(void);
	int total_count(void) const { return totalReq; }
	int remain_count(void) const { return totalReq - doneReq; }
	void second_pass(int err);
	void complete_task(TaskRequest *req, int index);

public:
	DTCValue *get_key_value(int i);
	void set_key_completed(int i);
	int is_key_completed(int i);
	void complete_waiter(void);
};

class MultiTaskReply : public ReplyDispatcher<TaskRequest>
{
public:
	MultiTaskReply() {}
	virtual void reply_notify(TaskRequest *cur);
};

#endif
