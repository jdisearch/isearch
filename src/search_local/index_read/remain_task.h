/*
 * =====================================================================================
 *
 *       Filename:  remain_task.h
 *
 *    Description:  remain task class definition.
 *
 *        Version:  1.0
 *        Created:  22/02/2019
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef _REMAIN_TASK_H_
#define _REMAIN_TASK_H_

#include "comm.h"
#include "process_task.h"
#include "json/value.h"
#include "search_conf.h"
#include "task_request.h"
#include "priority_queue.h"
#include "ternary.h"
#include <string>
#include <vector>
using  namespace std;

#define SUGGEST_NUM 10

class ClickInfoTask : public ProcessTask
{
public:
	ClickInfoTask();
	virtual int Process(CTaskRequest *request);
	virtual ~ClickInfoTask();

private:
	string m_user_id;
	string m_data;
	string m_url;
	uint32_t m_appid;
	uint32_t m_rank;
	uint32_t m_time;
	uint32_t m_jdq_switch;

	int ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet);
};

class ConfUpdateTask : public ProcessTask
{
public:
	ConfUpdateTask();
	virtual int Process(CTaskRequest *request);
	virtual ~ConfUpdateTask();

private:
	uint32_t m_cnt;
	uint32_t appid;
	string type;
	map<uint32_t, map<string, uint32_t> > hot_map;
	vector<vector<string> > relate_vec;
	AppInfo app_info;

	int ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet);
};

class HotQueryTask : public ProcessTask
{
public:
	HotQueryTask();
	virtual int Process(CTaskRequest *request);
	virtual ~HotQueryTask();

private:
	uint32_t m_cnt;
	uint32_t appid;
	int ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet);
};

class RelateTask : public ProcessTask
{
public:
	RelateTask();
	virtual int Process(CTaskRequest *request);
	virtual ~RelateTask();

private:
	string m_Data;
	uint32_t m_cnt;

	int ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet);
};

class SuggestTask : public ProcessTask
{
public:
	SuggestTask();
	virtual int Process(CTaskRequest *request);
	virtual ~SuggestTask();

private:
	string m_Data;
	uint32_t m_suggest_cnt;

	int ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet);
};



#endif