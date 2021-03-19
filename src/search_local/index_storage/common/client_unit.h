/*
 * =====================================================================================
 *
 *       Filename:  client_unit.h
 *
 *    Description:  client uint class definition.
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
#ifndef __H_DTC_CLIENT_UNIT_H__
#define __H_DTC_CLIENT_UNIT_H__

#include "request_base_all.h"
#include "stat_dtc.h"
#include "decoder_base.h"
#include "stop_watch.h"
#include "client_resource_pool.h"

class TaskRequest;
class DTCTableDefinition;

class DTCDecoderUnit : public DecoderUnit
{
public:
	DTCDecoderUnit(PollThread *, DTCTableDefinition **, int);
	virtual ~DTCDecoderUnit();

	virtual int process_stream(int fd, int req, void *, int);
	virtual int process_dgram(int fd);

	void bind_dispatcher(TaskDispatcher<TaskRequest> *p) { output.bind_dispatcher(p); }
	void task_dispatcher(TaskRequest *task) { output.task_notify(task); }
	DTCTableDefinition *owner_table(void) const { return tableDef[0]; }
	DTCTableDefinition *admin_table(void) const { return tableDef[1]; }

	void record_request_time(int hit, int type, unsigned int usec);
	// TaskRequest *p must nonnull
	void record_request_time(TaskRequest *p);
	void record_rcv_cnt(void);
	void record_snd_cnt(void);
	int regist_resource(ClientResourceSlot **res, unsigned int &id, uint32_t &seq);
	void unregist_resource(unsigned int id, uint32_t seq);
	void clean_resource(unsigned int id);

private:
	DTCTableDefinition **tableDef;
	RequestOutput<TaskRequest> output;
	ClientResourcePool clientResourcePool;

	StatSample statRequestTime[8];
};

#endif
