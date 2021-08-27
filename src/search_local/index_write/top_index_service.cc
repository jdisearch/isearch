/*
 * =====================================================================================
 *
 *       Filename:  top_index_service.cc
 *
 *    Description:  class definition.
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

#include "top_index_service.h"
#include <iostream>
#include <string>
#include <map>
#include "log.h"
#include "poll_thread.h"
#include "task_request.h"
#include "dtc_tools.h"
#include "comm.h"
#include "index_clipping.h"
#include "monitor.h"
#include "chash.h"

CTaskTopIndex::CTaskTopIndex(CPollThread * o) :
    CTaskDispatcher<CTaskRequest>(o),
    ownerThread(o),
    output(o)
{
}

CTaskTopIndex::~CTaskTopIndex()
{
}

int CTaskTopIndex::insert_snapshot_dtc(const UserTableContent &fields,int &doc_version,Json::Value &res){
	int ret;
	DTC::Server* dtc_server = index_servers.GetServer();
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	DTC::InsertRequest insertReq(dtc_server);
	insertReq.SetKey(CommonHelper::Instance()->GenerateDtcKey(fields.appid, "11", fields.doc_id).c_str());
	insertReq.Set("doc_id", fields.doc_id.c_str());
	insertReq.Set("doc_version", doc_version);
	insertReq.Set("created_time", fields.publish_time);
	insertReq.Set("field", fields.top);
	insertReq.Set("word_freq", 0);
	insertReq.Set("weight", 0);
	insertReq.Set("location", "");
	insertReq.Set("start_time", 0);
	insertReq.Set("end_time", 0);
	insertReq.Set("extend", fields.content.c_str());
	DTC::Result rst;
	ret = insertReq.Execute(rst);
	if (ret != 0)
	{

		log_error("insert request error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return -1;

	}
	return 0;
}

int CTaskTopIndex::delete_snapshot_dtc(const string &doc_id, uint32_t appid, Json::Value &res) {
	int ret;
	DTC::Server* dtc_server = index_servers.GetServer();
	if (NULL == dtc_server) {
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	DTC::DeleteRequest deleteReq(dtc_server);
	ret = deleteReq.SetKey(CommonHelper::Instance()->GenerateDtcKey(appid, "11", doc_id).c_str());
	ret = deleteReq.EQ("doc_id", doc_id.c_str());

	DTC::Result rst;
	ret = deleteReq.Execute(rst);
	if (ret != 0)
	{
		log_error("delete request error! ,errno %d ,errmsg %s, errfrom %s\n", ret, rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_DELETE_SNAPSHOT;
	}
	return 0;
}

static int get_snapshot_execute(DTC::Server* dtc_server,const UserTableContent &fields,DTC::Result &rst){
	DTC::GetRequest getReq(dtc_server);
	int ret = 0;

	ret = getReq.SetKey(CommonHelper::Instance()->GenerateDtcKey(fields.appid, "11", fields.doc_id).c_str());
	ret = getReq.Need("doc_version");

	ret = getReq.Execute(rst);
	return ret;
}

int CTaskTopIndex::get_snapshot_active_doc(const UserTableContent &fields,int &doc_version,Json::Value &res){
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
				return RT_ERROR_GET_SNAPSHOT;
			}
		}
		else {
			log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
			return RT_ERROR_GET_SNAPSHOT;
		}
	}
	int cnt = rst.NumRows();
	struct index_item item;
	if (rst.NumRows() <= 0) {
		return RT_NO_THIS_DOC;
	}
	else {
		for (int i = 0; i < cnt; i++) {
			rst.FetchRow();
			doc_version = rst.IntValue("doc_version");
		}

	}

	return 0;
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

static int decode_field(Json::Value table_content,Json::Value &json_fields,UserTableContent &fields){
	string cmd;
	time_t now = time(NULL);
	if(table_content.isMember("cmd") && table_content["cmd"].isString()){
		cmd = table_content["cmd"].asString();
		if(cmd == "top_add" || cmd == "top_update"){
			fields.top = 1;
			if(table_content.isMember("fields") && table_content["fields"].isObject()){
				json_fields = table_content["fields"];

				if(json_fields.isMember("id") && json_fields["id"].isString()){
					fields.doc_id = json_fields["id"].asString();
				}else{
					if(json_fields.isMember("doc_id") && json_fields["doc_id"].isString()){
						fields.doc_id = json_fields["doc_id"].asString();
					}else
						return RT_NO_DOCID;
				}

				if(json_fields.isMember("sp_words") && json_fields["sp_words"].isString()){
					fields.sp_words = json_fields["sp_words"].asString();
					fields.description = json_fields["sp_words"].asString();//description is using as sp_words section;
				}
				if(json_fields.isMember("weight") && json_fields["weight"].isInt()){
					fields.weight = json_fields["weight"].asInt();
				}else{
					fields.weight = 1;
				}
				if(json_fields.isMember("publish_time") && json_fields["publish_time"].isInt()){
					fields.publish_time = json_fields["publish_time"].asInt();
				}else
					fields.publish_time = now;
				if(json_fields.isMember("top_start_time") && json_fields["top_start_time"].isInt()){
					fields.top_start_time = json_fields["top_start_time"].asInt();
				}else{
					fields.top_start_time = now;
				}
				if(json_fields.isMember("top_end_time") && json_fields["top_end_time"].isInt()){
					if(json_fields["top_end_time"].asInt() < fields.top_start_time)
						fields.top_end_time = fields.top_start_time;
					fields.top_end_time = json_fields["top_end_time"].asInt();
				}else{
					fields.top_end_time = fields.top_start_time + (24*60*60);
				}
				return RT_CMD_ADD;

			}
		}else if(cmd == "top_delete"){
			fields.top = 1;
			Json::Value field = table_content["fields"];
			if(field.isMember("doc_id") && field["doc_id"].isString()){
				fields.doc_id = field["doc_id"].asString();
				return RT_CMD_DELETE;
			}
		}else{
			return RT_ERROR_FIELD_CMD;
		}
	}
	return 0;
}

int CTaskTopIndex::pre_process(void){
	DTCTools *dtc_tools = DTCTools::Instance();
	dtc_tools->init_servers(index_servers, IndexConf::Instance()->GetDTCIndexConfig());

	return 0;
}

int CTaskTopIndex::do_split_sp_words(string &str, string &doc_id, uint32_t appid, set<string> &word_set,Json::Value &res) {
	string word;
	uint32_t id = 0;
	vector<string> strs = splitEx(str, "|");
	vector<string>::iterator iter = strs.begin();
	uint32_t index = 0;
	for (; iter != strs.end(); iter++) {
		index++;
		word = *iter;
		if (!SplitManager::Instance()->wordValid(word, appid, id)){
			log_error("invalued sp_word!%s",word.c_str());
			return RT_ERROR_INVALID_SP_WORD;
		}
		word_set.insert(word);
	}
	return 0;
}

static int insert_top_index_execute(DTC::Server* dtcServer,string key,const UserTableContent &fields,int doc_version,DTC::Result &rst){
	int ret = 0;

	DTC::InsertRequest insertReq(dtcServer);
	insertReq.SetKey(key.c_str());
	insertReq.Set("doc_id", fields.doc_id.c_str());
	insertReq.Set("doc_version",doc_version);
	insertReq.Set("created_time",time(NULL));
	insertReq.Set("start_time",fields.top_start_time);
	insertReq.Set("end_time",fields.top_end_time);
	insertReq.Set("weight", fields.weight);
	insertReq.Set("extend","");
	ret = insertReq.Execute(rst);
	return ret;
}

int CTaskTopIndex::insert_top_index_dtc(string key,const UserTableContent &fields,int doc_version,Json::Value &res){
	int ret = 0;

	DTC::Server* dtcServer = index_servers.GetServer();
	if(dtcServer == NULL){
		log_error("GetServer error");
		return -1;
	}
	char tmp[41] = { '0' };
	snprintf(tmp, sizeof(tmp), "%40s", fields.doc_id.c_str());

	dtcServer->SetAccessKey(tmp);

	DTC::Result rst;
	ret = insert_top_index_execute(dtcServer,key,fields,doc_version,rst);
	if (ret != 0)
	{
		log_error("insert request error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		res[MESSAGE] = rst.ErrorMessage();
		return -1;
	}
	log_debug("insert word:%s sp_word:%s doc_id:%s doc_version: %d to top index!",key.c_str(),fields.sp_words.c_str(),fields.doc_id.c_str(),doc_version);
	return 0;
}

int CTaskTopIndex::do_insert_top_index(const UserTableContent &fields,int doc_version, set<string> &word_set,Json::Value &res) {

	int ret;

	set<string>::iterator iter = word_set.begin();
	for (; iter != word_set.end(); iter++) {
		string key = CommonHelper::Instance()->GenerateDtcKey(fields.appid, "01", *iter);
		ret = insert_top_index_dtc(key,fields,doc_version,res);
		if(ret < 0)
			return RT_ERROR_INSERT_TOP_INDEX_DTC;
	}

	return 0;
}

int CTaskTopIndex::update_sanpshot_dtc(const UserTableContent &fields,int doc_version,Json::Value &res){
	int ret = 0;
	DTC::Server* dtc_server = index_servers.GetServer();
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	DTC::UpdateRequest updateReq(dtc_server);
	ret = updateReq.SetKey(CommonHelper::Instance()->GenerateDtcKey(fields.appid, "11", fields.doc_id).c_str());
	updateReq.Set("doc_version", doc_version);
	if(fields.content != "null\n")
		updateReq.Set("extend", fields.content.c_str());
	updateReq.Set("created_time",fields.publish_time);

	DTC::Result rst;
	ret = updateReq.Execute(rst);
	if (ret != 0)
	{
		log_error("updateReq  error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_UPDATE_SNAPSHOT;
	}
	return ret;
}

int CTaskTopIndex::top_index_process(Json::Value &req,Json::Value &res){
	string split_content;
	string split_title;
	int doc_version = 0,old_version = 0;
	int app_id,fields_count = 0,ret = 0;
	Json::Value table_content;

	set<string> word_set;

	ret = decode_request(req, table_content, app_id,fields_count);
	if(ret != 0){
		return ret;
	}
	if(fields_count == 0 || fields_count != (int)table_content.size()){
		return RT_ERROR_FIELD_COUNT;
	}
	if(!SplitManager::Instance()->is_effective_appid(app_id)){
		return RT_NO_APPID;
	}

	for(int i = 0;i < (int)table_content.size();i++){
		doc_version = 0; old_version = 0;
		UserTableContent content_fields(app_id);
		Json::Value json_field;
		ret = decode_field(table_content[i],json_field,content_fields);
		if(RT_CMD_ADD == ret){
			ret = get_snapshot_active_doc(content_fields,old_version,res);
			if(0 == ret){
				doc_version = ++old_version;
			}else if(ret != RT_NO_THIS_DOC) return ret;
			Json::Value::Members member = json_field.getMemberNames();
			Json::Value snapshot_content;
			string lng = "",lat = "";
			for(Json::Value::Members::iterator iter = member.begin(); iter != member.end(); ++iter)
			{
				 string field_name = *iter;
				 struct table_info *tbinfo = NULL;
				 tbinfo = SplitManager::Instance()->get_table_info(app_id,field_name);
				 if(tbinfo == NULL){
					 continue;
				 }
				 if(tbinfo->snapshot_tag == 1){//snapshot
					 if(tbinfo->field_type == 1 && json_field[field_name].isInt()){
						 snapshot_content[field_name] = json_field[field_name].asInt();
					 }else if(tbinfo->field_type > 1 && json_field[field_name].isString()){
						 snapshot_content[field_name] = json_field[field_name].asString();
					 }
				 }
			}

			log_debug("sp_words:%s\n",content_fields.sp_words.c_str());
			ret = do_split_sp_words(content_fields.sp_words,content_fields.doc_id,content_fields.appid,word_set,res);
			if(0 != ret){
				res[MESSAGE] = "do_split_sp_words error";
				return ret;
			}
			ret = do_insert_top_index(content_fields,doc_version,word_set,res);
			if( 0!= ret){
				return ret;
			}

			Json::FastWriter writer;
			content_fields.content = writer.write(snapshot_content);
			if(doc_version != 0){//need update
				update_sanpshot_dtc(content_fields,doc_version,res);
			}else{
				insert_snapshot_dtc(content_fields,doc_version,res);//insert the snapshot doc
			}
			word_set.clear();
		}
		else if(RT_CMD_DELETE == ret){
			ret = delete_snapshot_dtc(content_fields.doc_id,content_fields.appid,res);//not use the doc_version curr
		}
	}

	return ret;
}

void CTaskTopIndex::TaskNotify(CTaskRequest * curr)
{
    log_debug("CTaskTopIndex::TaskNotify start");
	common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.topIndexTask"));
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
	if(SERVICE_TOPINDEX != task->GetReqCmd()){
		res["code"] = RT_ERROR_SERVICE_TYPE;
		res["reqcmd"] = task->GetReqCmd();
		res["message"] = "service type wrong! need 107";
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
	ret = top_index_process(value,res);
	if(0 != ret){
		res["code"] = ret;
	}

end:
	task->setResult(writer.write(res));
	task->ReplyNotify();

	common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
    return;
}
