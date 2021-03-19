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

#include "json/reader.h"
#include "json/value.h"
#include "json/writer.h"
#include "timemanager.h"
#include "data_manager.h"
#include "remain_task.h"
#include <netinet/in.h>
#include <algorithm>
#include <functional>
#include <set>
#include <sstream>
#include <fstream>
#include "search_util.h"
#include "monitor.h"
using namespace std;


ClickInfoTask::ClickInfoTask()
{
	SGlobalConfig &global_cfg = SearchConf::Instance()->GetGlobalConfig();
	m_jdq_switch = global_cfg.iJdqSwitch;
	m_appid = 10001;
	m_user_id = "0";
	m_rank = 0;
	m_time = 0;
}

ClickInfoTask::~ClickInfoTask() {}

int ClickInfoTask::ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet)
{

	Json::Reader r(Json::Features::strictMode());
	int ret;
	ret = r.parse(sz_json, sz_json + json_len, recv_packet);
	if (0 == ret)
	{
		log_error("the err json string is : %s", sz_json);
		log_error("parse json error , errmsg : %s", r.getFormattedErrorMessages().c_str());
		return -RT_PARSE_JSON_ERR;
	}

	if (recv_packet.isMember("appid") && recv_packet["appid"].isUInt())
	{
		m_appid = recv_packet["appid"].asUInt();
	}
	else {
		m_appid = 10001;
	}

	if (recv_packet.isMember("user_id") && recv_packet["user_id"].isString())
	{
		m_user_id = recv_packet["user_id"].asString();
	}
	else {
		m_user_id = "0";
	}

	if (recv_packet.isMember("key") && recv_packet["key"].isString())
	{
		m_data = recv_packet["key"].asString();
	}
	else {
		log_error("there is no data");
		return -RT_PARSE_JSON_ERR;
	}

	if (recv_packet.isMember("url") && recv_packet["url"].isString())
	{
		m_url = recv_packet["url"].asString();
	}
	else {
		log_error("there is no data");
		return -RT_PARSE_JSON_ERR;
	}

	if (recv_packet.isMember("rank") && recv_packet["rank"].isString())
	{
		m_rank = atoi(recv_packet["rank"].asString().c_str());
	}
	else {
		m_rank = 0;
	}

	if (recv_packet.isMember("time") && recv_packet["time"].isString())
	{
		m_time = atoi(recv_packet["time"].asString().c_str());
	}
	else {
		m_time = 0;
	}

	return 0;
}

int ClickInfoTask::Process(CTaskRequest *request) {
	log_debug("ClickInfoTask::Process begin.");
	common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.clickInfoTask"));
	Json::Value recv_packet;
	string request_string = request->buildRequsetString();
	if (ParseJson(request_string.c_str(), request_string.length(), recv_packet)) {
		string resultStr = GenReplyStr(PARAMETER_ERR);
		request->setResult(resultStr);
		common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
		common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
		return -RT_PARSE_JSON_ERR;
	}
	log_debug("m_data: %s", m_data.c_str());

	string time_str = GetFormatTimeStr(m_time);
	if (m_user_id == "") {
		m_user_id = "0";
	}

	stringstream click_info;
	click_info << m_appid << "|" << m_data << "|" << m_rank << "|" << m_url << "|" << m_user_id << "|" << time_str << "|";

	Json::FastWriter writer;
	Json::Value response;
	response["code"] = 0;

	std::string outputConfig = writer.write(response);
	request->setResult(outputConfig);
	common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
	return 0;
}


ConfUpdateTask::ConfUpdateTask()
{
	m_cnt = 0;
	appid = 0;
}

ConfUpdateTask::~ConfUpdateTask() {}

int ConfUpdateTask::ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet)
{

	Json::Reader r(Json::Features::strictMode());
	int ret;
	ret = r.parse(sz_json, sz_json + json_len, recv_packet);
	if (0 == ret)
	{
		log_error("the err json string is : %s", sz_json);
		log_error("parse json error , errmsg : %s", r.getFormattedErrorMessages().c_str());
		return -RT_PARSE_JSON_ERR;
	}

	Json::Value info;
	if (recv_packet.isMember("type") && recv_packet["type"].isString())
	{
		type = recv_packet["type"].asString();
	}
	else {
		log_error("need type in json string.");
		return -RT_PARSE_JSON_ERR;
	}
	if (recv_packet.isMember("info") && recv_packet["info"].isObject())
	{
		info = recv_packet["info"];
	}
	else {
		info.resize(0);
	}
	if (recv_packet.isMember("appid") && recv_packet["appid"].isUInt()) {
		appid = recv_packet["appid"].asUInt();
	}
	log_debug("type: %s, appid: %u.", type.c_str(), appid);

	if (type == "hot") {
		if (info.isMember("data")) {
			Json::Value data = info["data"];
			string word;
			uint32_t freq = 0;
			uint32_t appid = 0;
			for (int i = 0; i < (int)data.size(); i++) {
				word = data[i]["word"].asString();
				freq = data[i]["freq"].asInt();
				appid = data[i]["appid"].asInt();
				if (hot_map[appid].find(word) == hot_map[appid].end()) {
					hot_map[appid][word] = freq;
				}
			}
		}
	}
	else if (type == "relate") {
		if (info.isMember("cnt") && info["cnt"].isInt())
		{
			m_cnt = info["cnt"].asInt();
		}
		if (info.isMember("data")) {
			Json::Value datas = info["data"];
			string word;
			for (int i = 0; i < (int)datas.size(); i++) {
				vector<string> words;
				Json::Value data = datas[i];
				for (int j = 0; j < (int)data.size(); j++) {
					word = data[j].asString();
					words.push_back(word);
				}
				relate_vec.push_back(words);
			}
		}
	}
	else if (type == "app_conf") {
		if (info.isMember("cache_switch") && info["cache_switch"].isInt()) {
			app_info.cache_switch = info["cache_switch"].asInt();
		}
		if (info.isMember("en_query_switch") && info["en_query_switch"].isInt()) {
			app_info.en_query_switch = info["en_query_switch"].asInt();
		}
	}
	else {
		log_error("type[%s] in json string is not valid.", type.c_str());
		return -RT_PARSE_JSON_ERR;
	}

	return 0;
}

int ConfUpdateTask::Process(CTaskRequest *request) {
	log_debug("ConfUpdateTask::Process begin.");
	common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.confUpdateTask"));
	Json::Value recv_packet;
	string request_string = request->buildRequsetString();
	if (ParseJson(request_string.c_str(), request_string.length(), recv_packet) != 0) {
		string str = GenReplyStr(PARAMETER_ERR);
		request->setResult(str);
		common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
		common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
		return -RT_PARSE_JSON_ERR;
	}

	int ret = 0;
	if (type == "hot") {
		ret = DataManager::Instance()->UpdateHotWord(hot_map);
	}
	else if (type == "relate") {
		ret = DataManager::Instance()->UpdateRelateWord(relate_vec);
	}
	else if (type == "app_conf") {
		ret = SearchConf::Instance()->UpdateAppInfo(appid, app_info);
	}

	Json::FastWriter writer;
	Json::Value response;
	response["code"] = ret;

	std::string outputConfig = writer.write(response);
	request->setResult(outputConfig);
	common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
	return 0;

}


HotQueryTask::HotQueryTask()
{
}

HotQueryTask::~HotQueryTask() {}

int HotQueryTask::ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet)
{

	Json::Reader r(Json::Features::strictMode());
	int ret;
	ret = r.parse(sz_json, sz_json + json_len, recv_packet);
	if (0 == ret)
	{
		log_error("the err json string is : %s", sz_json);
		log_error("parse json error , errmsg : %s", r.getFormattedErrorMessages().c_str());
		return -RT_PARSE_JSON_ERR;
	}

	if (recv_packet.isMember("hot_cnt") && recv_packet["hot_cnt"].isString())
	{
		m_cnt = atoi(recv_packet["hot_cnt"].asString().c_str());
	}
	else {
		m_cnt = 3;
	}
	if (recv_packet.isMember("appid") && recv_packet["appid"].isUInt())
	{
		appid = recv_packet["appid"].asUInt();
	}
	else {
		appid = 10001;
	}

	return 0;
}

int HotQueryTask::Process(CTaskRequest *request) {
	log_debug("HotQueryTask::Process begin.");
	common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.hotQueryTask"));
	Json::Value recv_packet;
	string request_string = request->buildRequsetString();
	if (ParseJson(request_string.c_str(), request_string.length(), recv_packet)) {
		string str = GenReplyStr(PARAMETER_ERR);
		request->setResult(str);
		common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
		common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
		return -RT_PARSE_JSON_ERR;
	}

	vector<string> word_vec;
	DataManager::Instance()->GetHotWord(appid, word_vec, m_cnt);

	Json::FastWriter writer;
	Json::Value response;
	response["code"] = 0;
	response["count"] = Json::Value((int)word_vec.size());
	response["result"].resize(0);
	for (uint32_t i = 0; i < word_vec.size(); i++)
	{
		response["result"].append(word_vec[i]);
	}

	std::string outputConfig = writer.write(response);
	request->setResult(outputConfig);
	common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
	return 0;

}


RelateTask::RelateTask()
{
}

RelateTask::~RelateTask() {}

int RelateTask::ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet)
{

	Json::Reader r(Json::Features::strictMode());
	int ret;
	ret = r.parse(sz_json, sz_json + json_len, recv_packet);
	if (0 == ret)
	{
		log_error("the err json string is : %s", sz_json);
		log_error("parse json error , errmsg : %s", r.getFormattedErrorMessages().c_str());
		return -RT_PARSE_JSON_ERR;
	}

	if (recv_packet.isMember("key") && recv_packet["key"].isString())
	{
		m_Data = recv_packet["key"].asString();
	}
	else {
		log_error("there is no data");
		return -RT_PARSE_JSON_ERR;
	}

	if (recv_packet.isMember("relate_cnt") && recv_packet["relate_cnt"].isString())
	{
		m_cnt = atoi(recv_packet["relate_cnt"].asString().c_str());
	}
	else {
		m_cnt = 3;
	}

	return 0;
}

int RelateTask::Process(CTaskRequest *request) {
	log_debug("RelateTask::Process begin.");
	common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.relateTask"));
	Json::Value recv_packet;
	string request_string = request->buildRequsetString();
	if (ParseJson(request_string.c_str(), request_string.length(), recv_packet)) {
		string str = GenReplyStr(PARAMETER_ERR);
		request->setResult(str);
		common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
		common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
		return -RT_PARSE_JSON_ERR;
	}
	ToLower(m_Data);
	log_debug("m_Data: %s", m_Data.c_str());

	vector<string> word_vec;
	DataManager::Instance()->GetRelateWord(m_Data, word_vec, m_cnt);

	Json::FastWriter writer;
	Json::Value response;
	response["code"] = 0;
	response["count"] = Json::Value((int)word_vec.size());
	response["result"].resize(0);
	for (uint32_t i = 0; i < word_vec.size(); i++)
	{
		response["result"].append(word_vec[i]);
	}

	std::string outputConfig = writer.write(response);
	request->setResult(outputConfig);
	common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
	return 0;
}


SuggestTask::SuggestTask()
{
}

SuggestTask::~SuggestTask() {}

int SuggestTask::ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet)
{

	Json::Reader r(Json::Features::strictMode());
	int ret;
	ret = r.parse(sz_json, sz_json + json_len, recv_packet);
	if (0 == ret)
	{
		log_error("the err json string is : %s", sz_json);
		log_error("parse json error , errmsg : %s", r.getFormattedErrorMessages().c_str());
		return -RT_PARSE_JSON_ERR;
	}

	if (recv_packet.isMember("key") && recv_packet["key"].isString())
	{
		m_Data = recv_packet["key"].asString();
	}
	else {
		log_error("there is no data");
		return -RT_PARSE_JSON_ERR;
	}

	if (recv_packet.isMember("suggest_cnt") && recv_packet["suggest_cnt"].isString())
	{
		m_suggest_cnt = atoi(recv_packet["suggest_cnt"].asString().c_str());
	}
	else {
		m_suggest_cnt = 5;
	}

	return 0;
}

int SuggestTask::Process(CTaskRequest *request) {
	log_debug("SuggestTask::Process begin.");
	common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.suggestTask"));
	Json::FastWriter writer;
	Json::Value response;
	Json::Value recv_packet;
	string request_string = request->buildRequsetString();
	if (ParseJson(request_string.c_str(), request_string.size(), recv_packet)) {
		string resultStr = GenReplyStr(PARAMETER_ERR);
		request->setResult(resultStr);
		common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
		common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
		return RT_PARSE_JSON_ERR;
	}
	ToLower(m_Data);
	log_debug("m_Data: %s", m_Data.c_str());

	vector<string> word_vec;
	int ret = GetSuggestWord(m_Data, word_vec, m_suggest_cnt);
	if (ret != 0) {
		string resultStr = GenReplyStr(PARAMETER_ERR);
		request->setResult(resultStr);
		return RT_GET_SUGGEST_ERR;
	}
	if (word_vec.empty()) {
		//GetEnSuggestWord(m_Data, word_vec, m_suggest_cnt);
		PriorityQueue priorityQueue(m_suggest_cnt);
		TernaryTree::Instance()->FindSimliar(m_Data, priorityQueue);
		word_vec = priorityQueue.GetElements();
	}

	response["code"] = 0;
	response["count"] = Json::Value((int)word_vec.size());
	response["result"].resize(0);
	for (uint32_t i = 0; i < word_vec.size(); i++)
	{
		response["result"].append(word_vec[i]);
	}
	std::string outputConfig = writer.write(response);
	request->setResult(outputConfig);
	common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
	return 0;
}