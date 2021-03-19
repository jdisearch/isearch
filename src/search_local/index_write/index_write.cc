/*
 * =====================================================================================
 *
 *       Filename:  index_write.cc
 *
 *    Description:  IndexConf class definition.
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

#include "index_write.h"
#include <iostream>
#include <string>
#include <map>
#include "log.h"
#include "poll_thread.h"
#include "task_request.h"
#include "dtc_tools.h"
#include "index_clipping.h"
#include "monitor.h"
#include "chash.h"
#include "index_tbl_op.h"
#include "geohash.h"
#include "add_request_proc.h"

CTaskIndexGen::CTaskIndexGen(CPollThread * o) :
    CTaskDispatcher<CTaskRequest>(o),
    ownerThread(o),
    output(o),
	read_only(0)
{
}

CTaskIndexGen::~CTaskIndexGen()
{
}

int CTaskIndexGen::decode_request(const Json::Value & req, Json::Value & subreq, uint32_t & id, uint32_t & count)
{
	if (req.isMember("table_content") && req["table_content"].isArray()) {
		subreq = req["table_content"];
	}
	else {
		return RT_NO_TABLE_CONTENT;
	}

	if (req.isMember("appid") && req["appid"].isInt()) {
		id = req["appid"].asInt();
	}
	else {
		return RT_NO_APPID;
	}

	if (req.isMember("fields_count") && req["fields_count"].isInt()) {
		count = req["fields_count"].asInt();
	}
	else {
		return RT_NO_FIELD_COUNT;
	}

	return 0;
}

static int decode_fields(Json::Value table_content,Json::Value &json_fields,UserTableContent &fields){
	string cmd;
	if(table_content.isMember("cmd") && table_content["cmd"].isString()){
		cmd = table_content["cmd"].asString();
		if(cmd == "add" || cmd == "update"){
			if(table_content.isMember("fields") && table_content["fields"].isObject()){
				json_fields = table_content["fields"];

				if(json_fields.isMember("id") && (json_fields["id"].isString() || json_fields["id"].isInt())){
					fields.doc_id = json_fields["id"].asString();
				}else{
					if(json_fields.isMember("doc_id") && json_fields["doc_id"].isString()){
						fields.doc_id = json_fields["doc_id"].asString();
					}else
						return RT_NO_DOCID;
				}

				if(json_fields.isMember("weight") && json_fields["weight"].isInt()){
					fields.weight = json_fields["weight"].asInt();
				}else{
					fields.weight = 1;
				}
				return RT_CMD_ADD;
			}
			else{
				return RT_ERROR_FIELD;
			}
		}else if(cmd == "delete"){
			json_fields = table_content["fields"];
			if(json_fields.isMember("doc_id") && json_fields["doc_id"].isString()){
				fields.doc_id = json_fields["doc_id"].asString();
			}else if(json_fields.isMember("id") && (json_fields["id"].isString() || json_fields["id"].isInt())){
				fields.doc_id = json_fields["id"].asString();
			}else{
				return RT_NO_DOCID;
			}
			return RT_CMD_DELETE;
		}else{
			return RT_ERROR_FIELD_CMD;
		}
	}
	return 0;
}

int CTaskIndexGen::index_gen_process(Json::Value &req, Json::Value &res){
	int doc_version = 0, old_version = 0, trans_version = 0;
	uint32_t app_id, fields_count = 0;
	int ret = 0;
	Json::Value table_content;

	if (req.isMember("read_only") && req["read_only"].isInt()) {
		read_only = req["read_only"].asInt();
		return 0;
	}

	ret = decode_request(req, table_content, app_id, fields_count);
	if(ret != 0){
		return ret;
	}
	log_debug("table_content: %s", table_content.toStyledString().c_str());

	if(fields_count == 0 || fields_count != table_content.size()){
		return RT_ERROR_FIELD_COUNT;
	}
	if(!SplitManager::Instance()->is_effective_appid(app_id)){
		return RT_NO_APPID;
	}

	if (read_only) {
		return RT_ERROR_INDEX_READONLY;
	}

	for(int i = 0;i < (int)table_content.size();i++){
		doc_version = 0;
		old_version = 0;
		trans_version = 0;
		UserTableContent content_fields(app_id);
		Json::Value json_field;
		ret = decode_fields(table_content[i], json_field, content_fields);
		if(RT_CMD_ADD == ret){
			ret = g_IndexInstance.get_snapshot_active_doc(content_fields, old_version, res);
			if(0 == ret){
				trans_version = old_version + 1;
				doc_version = old_version + 1;
			}else if(ret == RT_NO_THIS_DOC){
				trans_version = 1;
				doc_version = 1;
			} else {
				log_error("get_snapshot_active_doc error.");
				return ret;
			}

			if(trans_version != 1){
				// 更新快照的trans_version字段
				int affected_rows = 0;
				ret = g_IndexInstance.update_snapshot_version(content_fields, trans_version, affected_rows);
				if(0 != ret){
					log_error("doc_id[%s] update snapshot version error, continue.", content_fields.doc_id.c_str());
					continue;
				}
				else if(affected_rows == 0){
					ret = RT_UPDATE_SNAPSHOT_CONFLICT;
					log_info("doc_id[%s] update snapshot conflict, continue.", content_fields.doc_id.c_str());
					continue;
				}
			} else {
				ret = g_IndexInstance.insert_snapshot_version(content_fields, trans_version);
				if(0 != ret){
					// 再查询一次快照
					ret = g_IndexInstance.get_snapshot_active_doc(content_fields, old_version, res);
					if(0 == ret){
						trans_version = old_version + 1;
						doc_version = old_version + 1;
						int affected_rows = 0;
						ret = g_IndexInstance.update_snapshot_version(content_fields, trans_version, affected_rows);
						if(0 != ret){
							log_error("doc_id[%s] update snapshot version error, continue.", content_fields.doc_id.c_str());
							continue;
						}
						else if(affected_rows == 0){
							ret = RT_UPDATE_SNAPSHOT_CONFLICT;
							log_info("doc_id[%s] update snapshot conflict, continue.", content_fields.doc_id.c_str());
							continue;
						}
					} else {
						log_error("doc_id[%s] insert error, continue.", content_fields.doc_id.c_str());
						continue;
					}
				}
			}

			InsertParam insert_param;
			insert_param.appid = app_id;
			insert_param.doc_id = content_fields.doc_id;
			insert_param.doc_version = doc_version;
			insert_param.trans_version = trans_version;
			AddReqProc add_req_proc(json_field, insert_param);
			ret = add_req_proc.do_insert_index(content_fields);
			if(0 != ret){
				return ret;
			}
		}
		else if(RT_CMD_DELETE == ret){
		    // 从hanpin_index_data中删除
			vector<uint32_t> field_vec;
			SplitManager::Instance()->getHanpinField(content_fields.appid, field_vec);
			vector<uint32_t>::iterator iter = field_vec.begin();
			for (; iter != field_vec.end(); iter++) {
				stringstream ss;
				ss << content_fields.appid << "#" << *iter;
				ret = g_IndexInstance.delete_hanpin_index(ss.str(), content_fields.doc_id);
				if (ret != 0) {
					log_error("delete error! errcode %d", ret);
					return ret;
				}
			}

			ret = g_IndexInstance.get_snapshot_active_doc(content_fields, old_version,res);
			if(ret != 0 && ret != RT_NO_THIS_DOC){
				log_error("get_snapshot_active_doc error! errcode %d", ret);
				return ret;
			}
			map<uint32_t, vector<string> > index_res;
			g_IndexInstance.GetIndexData(gen_dtc_key_string(content_fields.appid, "20", content_fields.doc_id), old_version, index_res);
			map<uint32_t, vector<string> >::iterator map_iter = index_res.begin();
			for(; map_iter != index_res.end(); map_iter++){
				uint32_t field = map_iter->first;
				vector<string> words = map_iter->second;
				for(int i = 0; i < (int)words.size(); i++){
					DeleteTask::GetInstance().RegisterInfo(words[i], content_fields.doc_id, old_version, field);
				}
			}

			ret = g_IndexInstance.delete_snapshot_dtc(content_fields.doc_id, content_fields.appid, res);//not use the doc_version curr
			g_IndexInstance.delete_docid_index_dtc(gen_dtc_key_string(content_fields.appid, "20", content_fields.doc_id), content_fields.doc_id);
		}
	}

	return ret;
}

void CTaskIndexGen::TaskNotify(CTaskRequest * curr)
{
    log_debug("CTaskIndexGen::TaskNotify start");
	common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.indexGenTask"));
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
	if(SERVICE_INDEXGEN != task->GetReqCmd()){
		res["code"] = RT_ERROR_SERVICE_TYPE;
		res["reqcmd"] = task->GetReqCmd();
		res["message"] = "service type wrong! need 106";
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
	ret = index_gen_process(value, res);
	if(0 != ret){
		res["code"] = ret;
	}

end:
	task->setResult(writer.write(res));
	task->ReplyNotify();

	common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
    return;
}
