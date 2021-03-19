/*
 * =====================================================================================
 *
 *       Filename:  client_unit.cc
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
#include <errno.h>
#include <unistd.h>

#include "client_unit.h"
#include "client_sync.h"
#include "client_async.h"
#include "client_dgram.h"
#include "poll_thread.h"
#include "task_request.h"
#include "log.h"

DTCDecoderUnit::DTCDecoderUnit(PollThread *o, DTCTableDefinition **tdef, int it) : DecoderUnit(o, it),
																				   tableDef(tdef),
																				   output(o)
{

	statRequestTime[0] = statmgr.get_sample(REQ_USEC_ALL);
	statRequestTime[1] = statmgr.get_sample(REQ_USEC_GET);
	statRequestTime[2] = statmgr.get_sample(REQ_USEC_INS);
	statRequestTime[3] = statmgr.get_sample(REQ_USEC_UPD);
	statRequestTime[4] = statmgr.get_sample(REQ_USEC_DEL);
	statRequestTime[5] = statmgr.get_sample(REQ_USEC_FLUSH);
	statRequestTime[6] = statmgr.get_sample(REQ_USEC_HIT);
	statRequestTime[7] = statmgr.get_sample(REQ_USEC_REPLACE);

	if (clientResourcePool.Init() < 0)
		throw(int) - ENOMEM;
}

DTCDecoderUnit::~DTCDecoderUnit()
{
}

void DTCDecoderUnit::record_request_time(int hit, int type, unsigned int usec)
{
	static const unsigned char cmd2type[] =
		{
			/*Nop*/ 0,
			/*result_code*/ 0,
			/*DTCResultSet*/ 0,
			/*HelperAdmin*/ 0,
			/*Get*/ 1,
			/*Purge*/ 5,
			/*Insert*/ 2,
			/*Update*/ 3,
			/*Delete*/ 4,
			/*Other*/ 0,
			/*Other*/ 0,
			/*Other*/ 0,
			/*Replace*/ 7,
			/*Flush*/ 5,
			/*Other*/ 0,
			/*Other*/ 0,
		};
	statRequestTime[0].push(usec);
	unsigned int t = hit ? 6 : cmd2type[type];
	if (t)
		statRequestTime[t].push(usec);
}

void DTCDecoderUnit::record_request_time(TaskRequest *req)
{
	record_request_time(req->flag_is_hit(), req->request_code(), req->responseTimer.live());
}

int DTCDecoderUnit::process_stream(int newfd, int req, void *peer, int peerSize)
{
	if (req <= 1)
	{
		ClientSync *cli = NULL;
		try
		{
			cli = new ClientSync(this, newfd, peer, peerSize);
		}
		catch (int err)
		{
			DELETE(cli);
			return -1;
		}

		if (0 == cli)
		{
			log_error("create CClient object failed, errno[%d], msg[%m]", errno);
			return -1;
		}

		if (cli->Attach() == -1)
		{
			log_error("Invoke CClient::Attach() failed");
			delete cli;
			return -1;
		}

		/* accept唤醒后立即recv */
		cli->input_notify();
	}
	else
	{
		ClientAsync *cli = new ClientAsync(this, newfd, req, peer, peerSize);

		if (0 == cli)
		{
			log_error("create CClient object failed, errno[%d], msg[%m]", errno);
			return -1;
		}

		if (cli->Attach() == -1)
		{
			log_error("Invoke CClient::Attach() failed");
			delete cli;
			return -1;
		}

		/* accept唤醒后立即recv */
		cli->input_notify();
	}
	return 0;
}

int DTCDecoderUnit::process_dgram(int newfd)
{
	ClientDgram *cli = new ClientDgram(this, newfd);

	if (0 == cli)
	{
		log_error("create CClient object failed, errno[%d], msg[%m]", errno);
		return -1;
	}

	if (cli->Attach() == -1)
	{
		log_error("Invoke CClient::Attach() failed");
		delete cli;
		return -1;
	}
	return 0;
}

int DTCDecoderUnit::regist_resource(ClientResourceSlot **res, unsigned int &id, uint32_t &seq)
{
	if (clientResourcePool.Alloc(id, seq) < 0)
	{
		id = 0;
		*res = NULL;
		return -1;
	}

	*res = clientResourcePool.Slot(id);
	return 0;
}

void DTCDecoderUnit::unregist_resource(unsigned int id, uint32_t seq)
{
	clientResourcePool.Free(id, seq);
}

void DTCDecoderUnit::clean_resource(unsigned int id)
{
	clientResourcePool.Clean(id);
}
