/*
 * =====================================================================================
 *
 *       Filename:  multi_request.cc
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
#include "multi_request.h"
#include "task_request.h"
#include "task_multiplexer.h"
#include "key_list.h"
#include "mem_check.h"

static MultiTaskReply multiTaskReply;

MultiRequest::MultiRequest(TaskMultiplexer *o, TaskRequest *task) : owner(o),
																	wait(task),
																	keyList(NULL),
																	keyMask(NULL),
																	doneReq(0),
																	totalReq(0),
																	subReq(0),
																	firstPass(1),
																	keyFields(0),
																	internal(0)
{
}

MultiRequest::~MultiRequest()
{
	if (wait)
	{
		wait->reply_notify();
		wait = NULL;
	}
	if (internal == 0)
		FREE_IF(keyList);
	FREE_IF(keyMask);
}

void MultiTaskReply::reply_notify(TaskRequest *cur)
{
	MultiRequest *req = cur->OwnerInfo<MultiRequest>();
	if (req == NULL)
		delete cur;
	else
		req->complete_task(cur, cur->owner_index());
}

DTCValue *MultiRequest::get_key_value(int i)
{
	return &keyList[i * keyFields];
}

void MultiRequest::set_key_completed(int i)
{
	FD_SET(i, (fd_set *)keyMask);
	doneReq++;
}

int MultiRequest::is_key_completed(int i)
{
	return FD_ISSET(i, (fd_set *)keyMask);
}

int MultiRequest::decode_key_list(void)
{
	if (!wait->flag_multi_key_val()) // single task
		return 0;

	const DTCTableDefinition *tableDef = wait->table_definition();

	keyFields = tableDef->key_fields();
	if (wait->internal_key_val_list())
	{
		// embeded API
		totalReq = wait->internal_key_val_list()->KeyCount();
		// Q&D discard const here
		// this keyList member can't be const,
		// but actually readonly after init
		keyList = (DTCValue *)&wait->internal_key_val_list()->Value(0, 0);
		internal = 1;
	}
	else
	{
		// from network
		uint8_t fieldID[keyFields];
		Array keyNameList(*(wait->key_name_list()));
		Array keyValList(*(wait->key_val_list()));
		DTCBinary keyName;
		for (int i = 0; i < keyFields; i++)
		{
			if (keyNameList.Get(keyName) != 0)
			{
				log_error("get key name[%d] error, key field count:%d", i, tableDef->key_fields());
				return -1;
			}
			fieldID[i] = tableDef->field_id(keyName.ptr);
		}
		if (keyNameList.Get(keyName) == 0)
		{
			log_error("bogus key name: %.*s", keyName.len, keyName.ptr);
			return -1;
		}

		totalReq = wait->versionInfo.get_tag(11)->u64;
		keyList = (DTCValue *)MALLOC(totalReq * keyFields * sizeof(DTCValue));
		for (int i = 0; i < totalReq; i++)
		{
			DTCValue *keyVal = get_key_value(i);
			for (int j = 0; j < keyFields; j++)
			{
				int fid = fieldID[j];
				switch (tableDef->field_type(fid))
				{
				case DField::Signed:
				case DField::Unsigned:
					if (keyValList.Get(keyVal[fid].u64) != 0)
					{
						log_error("get key value[%d][%d] error", i, j);
						return -2;
					}
					break;

				case DField::String:
				case DField::Binary:
					if (keyValList.Get(keyVal[fid].bin) != 0)
					{
						log_error("get key value[%d][%d] error", i, j);
						return -2;
					}
					break;

				default:
					log_error("invalid key type[%d][%d]", i, j);
					return -3;
				}
			}
		}
	}

	//	keyMask = (unsigned char *)CALLOC(1, (totalReq*keyFields+7)/8);
	//	8 bytes aligned Awaste some memory. FD_SET operate memory by 8bytes
	keyMask = (unsigned char *)CALLOC(8, (((totalReq * keyFields + 7) / 8) + 7) / 8);
	return totalReq;
}

int MultiRequest::split_task(void)
{
	log_debug("split_task begin, totalReq: %d", totalReq);
	for (int i = 0; i < totalReq; i++)
	{
		if (is_key_completed(i))
			continue;

		DTCValue *keyVal = get_key_value(i);
		TaskRequest *pTask = new TaskRequest;
		if (pTask == NULL)
		{
			log_error("%s: %m", "new task error");
			return -1;
		}
		if (pTask->Copy(*wait, keyVal) < 0)
		{
			log_error("copy task error: %s", pTask->resultInfo.error_message());
			delete pTask;
			return -1;
		}

		pTask->set_owner_info(this, i, wait->OwnerAddress());
		pTask->push_reply_dispatcher(&multiTaskReply);
		owner->push_task_queue(pTask);
		subReq++;
	}

	log_debug("split_task end, subReq: %d", subReq);
	return 0;
}

void MultiRequest::complete_task(TaskRequest *req, int index)
{
	log_debug("MultiRequest::complete_task start, index: %d", index);
	if (wait)
	{
		if (wait->result_code() >= 0 && req->result_code() < 0)
		{
			wait->set_error_dup(req->resultInfo.result_code(), req->resultInfo.error_from(), req->resultInfo.error_message());
		}

		int ret;
		if ((ret = wait->merge_result(*req)) != 0)
		{
			wait->set_error(ret, "multi_request", "merge result error");
		}
	}

	delete req;

	set_key_completed(index);
	subReq--;

	// 注意，如果将CTaskMultiplexer放到cache线程执行，则会导致每split一个task，都是直接到cache_process执行完到这里；然后再split出第二个task。这会导致这一个判断逻辑有问题。
	// 目前CTaskMultiplexer是跟incoming线程绑在一起的，因此没有问题
	if (firstPass == 0 && subReq == 0)
	{
		complete_waiter();
		delete this;
	}
	log_debug("MultiRequest::complete_task end, subReq: %d", subReq);
}

void MultiRequest::complete_waiter(void)
{
	if (wait)
	{
		wait->reply_notify();
		wait = 0;
	}
}

void MultiRequest::second_pass(int err)
{
	firstPass = 0;
	if (subReq == 0)
	{
		// no sub-request present, complete whole request
		complete_waiter();
		delete this;
	}
	else if (err)
	{
		// mark all request is done except sub-requests
		doneReq = totalReq - subReq;
		complete_waiter();
	}
	return;
}

int TaskRequest::set_batch_cursor(int index)
{
	int err = 0;

	MultiRequest *mreq = get_batch_key_list();
	if (mreq == NULL)
		return -1;

	if (index < 0 || index >= mreq->total_count())
	{
		key = NULL;
		multiKey = NULL;
		return -1;
	}
	else
	{
		DTCValue *keyVal = mreq->get_key_value(index);
		int kf = table_definition()->key_fields();

		/* switch request_key() */
		key = &keyVal[0];
		if (kf > 1)
		{
			/* switch multi-fields key */
			multiKey = keyVal;
		}
		err = build_packed_key();
		if (err < 0)
		{
			log_error("build packed key error, error from: %s, error message: %s", resultInfo.error_from(), resultInfo.error_message());
			return -1;
		}
	}
	return 0;
}

void TaskRequest::done_batch_cursor(int index)
{
	MultiRequest *mreq = get_batch_key_list();
	if (mreq == NULL)
		return;

	mreq->set_key_completed(index);
}
