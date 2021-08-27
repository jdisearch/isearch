/*
 * =====================================================================================
 *
 *       Filename:  snapshot_service.cc
 *
 *    Description:  CTaskSnapShot class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  shrewdlin, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include <iostream>
#include <string>
#include "log.h"
#include "poll_thread.h"
#include "task_request.h"
#include "dtc_tools.h"
#include "comm.h"
#include "snapshot_service.h"
#include "monitor.h"
#include "chash.h"


CTaskSnapShot::CTaskSnapShot(CPollThread * o) :
    CTaskDispatcher<CTaskRequest>(o),
    ownerThread(o),
    output(o)
{
}

CTaskSnapShot::~CTaskSnapShot()
{
}

static int decode_request(const Json::Value &req, Json::Value &subreq, int &id, int &count){
	if(req.isMember("table_content") && req["table_content"].isArray()){
		subreq = req["table_content"];
	}else{
		return RT_NO_TABLE_CONTENT;
	}

	if(req.isMember("appid") && req["appid"].isInt()){
		id = req["appid"].asInt();
	}else{
		return RT_NO_APPID;
	}

	if(req.isMember("fields_count") && req["fields_count"].isInt()){
		count = req["fields_count"].asInt();
	}else{
		return RT_NO_FIELD_COUNT;
	}

	return 0;
}

static int decode_field(Json::Value table_content,UserTableContent &fields){
	string cmd;
	if(table_content.isMember("cmd") && table_content["cmd"].isString()){
		cmd = table_content["cmd"].asString();
		if(cmd == "snapshot"){
			if(table_content.isMember("fields") && table_content["fields"].isObject()){
				Json::Value field = table_content["fields"];
				if(field.isMember("doc_id") && field["doc_id"].isString()){
					fields.doc_id = field["doc_id"].asString();
				}
				if (field.isMember("top") && field["top"].isInt()) {
					fields.top = field["top"].asInt();
				}
			}
			return RT_CMD_GET;
		}else if(cmd == "update_snapshot"){
			if(table_content.isMember("fields") && table_content["fields"].isObject()){
				Json::Value field = table_content["fields"];
				if(field.isMember("doc_id") && field["doc_id"].isString()){
					fields.doc_id = field["doc_id"].asString();
				}
				if (field.isMember("top") && field["top"].isInt()) {
					fields.top = field["top"].asInt();
				}
				if(field.isMember("weight") && field["weight"].isInt()){
					fields.weight = field["weight"].asInt();
				}
			}
			return RT_CMD_UPDATE;
		}else{
			return RT_ERROR_FIELD_CMD;
		}
	}
	return 0;
}

static int get_snapshot_execute(DTC::Server* dtc_server,const UserTableContent &fields,DTC::Result &rst){
	DTC::GetRequest getReq(dtc_server);
	int ret = 0;

	string top_tag = "10";
	if (fields.top == 1) {
		top_tag = "11";
	}
	ret = getReq.SetKey(CommonHelper::Instance()->GenerateDtcKey(fields.appid, top_tag, fields.doc_id).c_str());
	ret = getReq.Need("extend");
	ret = getReq.Need("created_time");

	ret = getReq.Execute(rst);
	return ret;
}

int CTaskSnapShot::get_snapshot_dtc(UserTableContent &fields,Json::Value &res){
	int ret;
	DTC::Server* dtc_server = index_servers.GetServer();
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	DTC::Result rst;
	ret = get_snapshot_execute(dtc_server,fields,rst);
	if (ret != 0) {
		if (ret == -110) {
			rst.Reset();
			ret = get_snapshot_execute(dtc_server,fields,rst);
			if (ret != 0) {
				log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
				res[MESSAGE] = rst.ErrorMessage();
				return RT_ERROR_GET_SNAPSHOT;
			}
		}
		else {
			log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
			res[MESSAGE] = rst.ErrorMessage();
			return RT_ERROR_GET_SNAPSHOT;
		}
	}
	int cnt = rst.NumRows();
	if (rst.NumRows() <= 0) {
		res["doc_id"] = fields.doc_id;
		res[MESSAGE] = "no this doc";
		return RT_NO_THIS_DOC;
	}
	else {
		for (int i = 0; i < cnt; i++) {
			rst.FetchRow();
			fields.title = "";
			fields.content = rst.StringValue("extend");
			fields.publish_time = rst.IntValue("created_time");
			fields.author = "";
			if(fields.title.length() > 0 && fields.content.length() > 0)
				break;
		}

	}

	return 0;
}

int CTaskSnapShot::pre_process(void){
	DTCTools *dtc_tools = DTCTools::Instance();
	dtc_tools->init_servers(index_servers, IndexConf::Instance()->GetDTCIndexConfig());

	return 0;
}

int CTaskSnapShot::update_sanpshot_dtc(const UserTableContent &fields,Json::Value &res){
	int ret = 0;
	DTC::Server* dtc_server = index_servers.GetServer();
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	string top_tag = "10";
	if (fields.top == 1) {
		top_tag = "11";
	}
	DTC::UpdateRequest updateReq(dtc_server);
	ret = updateReq.SetKey(CommonHelper::Instance()->GenerateDtcKey(fields.appid, top_tag, fields.doc_id).c_str());
	updateReq.Set("weight",fields.weight);

	DTC::Result rst;
	ret = updateReq.Execute(rst);
	if (ret != 0)
	{
		log_error("updateReq  error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_UPDATE_SNAPSHOT;
	}
	return ret;
}

int CTaskSnapShot::snapshot_process(Json::Value &req,Json::Value &res){
	int app_id,fields_count = 0,ret = 0;
	Json::Value table_content;

	ret = decode_request(req, table_content, app_id,fields_count);
	if(ret != 0){
		return ret;
	}
	if(fields_count != 1 || fields_count != (int)table_content.size()){
		res["message"] = "fields_count and table size must be 1";
		return RT_ERROR_FIELD_COUNT;
	}
	UserTableContent content_fields(app_id);
	ret = decode_field(table_content[0],content_fields);
	if(RT_CMD_GET == ret && content_fields.doc_id.length() > 0){
		ret = get_snapshot_dtc(content_fields,res);
		if(0 == ret){
			res["doc_id"] = content_fields.doc_id;
			res["title"] = content_fields.title;
			res["content"] = content_fields.content;
			res["author"] = content_fields.author;
			res["publish_time"] = content_fields.publish_time;
		}
	}else if(RT_CMD_UPDATE == ret && content_fields.doc_id.length() > 0){
		ret = update_sanpshot_dtc(content_fields,res);
	}
	return ret;
}

void CTaskSnapShot::TaskNotify(CTaskRequest * curr)
{
    log_debug("CTaskSnapShot::TaskNotify start");
	common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.snapshotTask"));
    //there is a race condition here:
    //curr may be deleted during process (in task->ReplyNotify())
    int ret;
	Json::Reader reader;
	Json::FastWriter writer;
	Json::Value value, res;
	std::string req;
	res["code"] = 0;

	CTaskRequest * task = curr;
	if(NULL == curr){
		common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
		common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
		return;
	}
	if(SERVICE_SNAPSHOT != task->GetReqCmd()){
		res["code"] = RT_ERROR_SERVICE_TYPE;
		res["reqcmd"] = task->GetReqCmd();
		res["message"] = "service type wrong! need 108";
		goto end;
	}
	req = task->buildRequsetString();
	log_debug("recv:%s\n",req.c_str());
	if(!reader.parse(req,value,false))
	{
		log_error("parse json error!\ndata:%s errors:%s\n",req.c_str(),reader.getFormattedErrorMessages().c_str());
		res["code"] = RT_PARSE_JSON_ERR;
		res["message"] = reader.getFormattedErrorMessages();
		res["data"] = req;
		goto end;

	}
	if(!value.isObject()){
		log_error("parse json error!\ndata:%s errors:%s\n",req.c_str(),reader.getFormattedErrorMessages().c_str());
		res["code"] = RT_PARSE_JSON_ERR;
		res["message"] = "it's not a json";
		res["data"] = req;
		goto end;
	}
	ret = snapshot_process(value,res);
	if(0 != ret){
		res["code"] = ret;
	}

end:
	task->setResult(writer.write(res));
	task->ReplyNotify();

	common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
    return;
}
