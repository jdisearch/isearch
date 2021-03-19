/*
 * =====================================================================================
 *
 *       Filename:  search_process.h
 *
 *    Description:  search process class definition.
 *
 *        Version:  1.0
 *        Created:  07/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include <iostream>
#include <string>
#include <sstream>
#include "search_process.h"

#include "log.h"
#include "poll_thread.h"
#include "task_request.h"
#include "search_task.h"
#include "remain_task.h"
#include "json/json.h"
#include "stat_index.h"

using namespace std;

static CStatItemU32 statIndexSugestCount;
static CStatItemU32 statIndexSearchCount;
static CStatItemU32 statIndexClickInfoCount;
static CStatItemU32 statIndexRelateCount;
static CStatItemU32 statIndexHotQueryCount;
static CStatItemU32 statIndexUnknownCmdCount;
static CStatItemU32 statIndexReqErrorCount;

CSearchProcess::CSearchProcess(CPollThread * o) :
	CTaskDispatcher<CTaskRequest>(o),
	ownerThread(o),
	output(o)
{
	statInitFlag = false;
}

CSearchProcess::~CSearchProcess()
{
}

string CSearchProcess::GenReplyStr(RSPCODE code) {
	Json::Value m_ReplyPacket;
	m_ReplyPacket["code"] = code;
	Json::StyledWriter _sw;
	string resultStr = _sw.write(m_ReplyPacket);
	return resultStr;
}

ProcessTask *CSearchProcess::CreateTask(uint16_t CmdType)
{
	ProcessTask* task = NULL;
	switch (CmdType)
	{
	case SERVICE_SUGGEST:
		statIndexSugestCount++;
		task = new SuggestTask();
		break;
	case SERVICE_SEARCH:
		statIndexSearchCount++;
		task = new SearchTask();
		break;
	case SERVICE_CLICK:
		statIndexClickInfoCount++;
		task = new ClickInfoTask();
		break;
	case SERVICE_RELATE:
		statIndexRelateCount++;
		task = new RelateTask();
		break;
	case SERVICE_HOT:
		statIndexHotQueryCount++;
		task = new HotQueryTask();
		break;
	case SERVICE_CONF_UPD:
		task = new ConfUpdateTask();
		break;
	default:
		statIndexUnknownCmdCount++;
		log_debug("default");
		task = new ProcessTask();
		break;
	}
	return task;

}

void CSearchProcess::TaskNotify(CTaskRequest * curr)
{
	log_debug("CSearchProcess::TaskNotify start");
	if (!statInitFlag) {
		statIndexSugestCount = statmgr.GetItemU32(INDEX_SUGGEST_CNT);
		statIndexSearchCount = statmgr.GetItemU32(INDEX_SEARCH_CNT);
		statIndexClickInfoCount = statmgr.GetItemU32(INDEX_CLICK_INFO_CNT);
		statIndexRelateCount = statmgr.GetItemU32(INDEX_RELATE_CNT);
		statIndexHotQueryCount = statmgr.GetItemU32(INDEX_HOT_QUERY_CNT);
		statIndexUnknownCmdCount = statmgr.GetItemU32(INDEX_UNKNOWN_CMD_CNT);
		statIndexReqErrorCount = statmgr.GetItemU32(INDEX_REQ_ERROR_CNT);
		statInitFlag = true;
	}

	CTaskRequest * request = curr;
	if (NULL == curr) {
		return;
	}
	uint16_t cmd = request->GetReqCmd();
	ProcessTask *job = CreateTask(cmd);
	if (NULL == job) {
		log_crit("no new memory for Job");
		string resultStr = GenReplyStr(SYSTEM_ERR);
		request->setResult(resultStr);
		request->ReplyNotify();
		return;
	}
	int ret = job->Process(request);
	if (ret != 0) {
		statIndexReqErrorCount++;
	}

	request->ReplyNotify();
	if (NULL != job) {
		delete job;
	}
	return;
}