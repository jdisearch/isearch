/*
 * =====================================================================================
 *
 *       Filename:  image_service.cc
 *
 *    Description:  CTaskImage class definition.
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

#include "image_service.h"
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

CTaskImage::CTaskImage(CPollThread * o) :
    CTaskDispatcher<CTaskRequest>(o),
    ownerThread(o),
    output(o)
{
}

CTaskImage::~CTaskImage()
{
}

int CTaskImage::insert_snapshot_dtc(const UserTableContent &fields,int &doc_version,Json::Value &res){
	int ret;
	DTC::Server* dtc_server = index_servers.GetServer();
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	DTC::InsertRequest insertReq(dtc_server);
	insertReq.SetKey(CommonHelper::Instance()->GenerateDtcKey(fields.appid, "10", fields.doc_id).c_str());
	insertReq.Set("doc_id", fields.doc_id.c_str());
	insertReq.Set("doc_version", doc_version);
	insertReq.Set("extend", fields.content.c_str());
	insertReq.Set("field", fields.top);
	insertReq.Set("weight", fields.weight);
	insertReq.Set("created_time", fields.publish_time);
	insertReq.Set("word_freq", 0);
	insertReq.Set("location", "");
	insertReq.Set("start_time", 0);
	insertReq.Set("end_time", 0);
	DTC::Result rst;
	ret = insertReq.Execute(rst);
	if (ret != 0)
	{

		log_error("insert request error! ,errno %d ,errmsg %s, errfrom %s\n", ret, rst.ErrorMessage(), rst.ErrorFrom());
		return -1;

	}
	return 0;
}


int CTaskImage::delete_snapshot_dtc(string &doc_id,uint32_t appid,Json::Value &res){
	int ret;
	DTC::Server* dtc_server = index_servers.GetServer();
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	DTC::DeleteRequest deleteReq(dtc_server);
	ret = deleteReq.SetKey(CommonHelper::Instance()->GenerateDtcKey(appid, "10", doc_id).c_str());

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

	ret = getReq.SetKey(CommonHelper::Instance()->GenerateDtcKey(fields.appid, "10", fields.doc_id).c_str());
	ret = getReq.Need("doc_version");

	ret = getReq.Execute(rst);
	return ret;
}

int CTaskImage::get_snapshot_active_doc(const UserTableContent &fields,int &doc_version,Json::Value &res){
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

static int insert_index_execute(DTC::Server* dtcServer,string key,struct item &it,u_int8_t field_type,int doc_version,DTC::Result &rst){
	int ret = 0;

	stringstream index_sstr;
	index_sstr << "[";
	int count = 0;
	vector<uint32_t>::iterator iter = it.indexs.begin();
	for (; iter != it.indexs.end(); iter++) {
		if (count++ > 25) {
			break;
		}
		index_sstr << *iter << ",";
	}
	string index_str = index_sstr.str();
	index_str = index_str.substr(0, index_str.size()-1);
	index_str.append("]");
	DTC::InsertRequest insertReq(dtcServer);
	insertReq.SetKey(key.c_str());
	insertReq.Set("doc_id", it.doc_id.c_str());
	insertReq.Set("field", field_type);
	insertReq.Set("word_freq", it.freq);
	insertReq.Set("weight", 1);
	insertReq.Set("extend","");
	insertReq.Set("doc_version",doc_version);
	insertReq.Set("created_time",time(NULL));
	insertReq.Set("location", index_str.c_str());
	ret = insertReq.Execute(rst);
	return ret;
}

int CTaskImage::insert_index_dtc(DTC::Server* dtcServer,string key,struct item &it,u_int8_t field_type,int doc_version,Json::Value &res){
	int ret = 0;

	char tmp[41] = { '0' };
	snprintf(tmp, sizeof(tmp), "%40s", it.doc_id.c_str());

	dtcServer->SetAccessKey(tmp);

	DTC::Result rst;
	ret = insert_index_execute(dtcServer,key,it,field_type,doc_version,rst);
	if (ret != 0)
	{
		log_error("insert request error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		res[MESSAGE] = rst.ErrorMessage();
		return -1;
	}
	return 0;
}

int CTaskImage::do_insert_index(DTC::Server* dtcServer, map<string, item> &word_map, map<string, item> &title_map,uint64_t app_id,int doc_version,Json::Value &res) {

	int ret;
	map<string, item>::iterator map_iter = word_map.begin();
	for (; map_iter != word_map.end(); map_iter++) {
		string key = CommonHelper::Instance()->GenerateDtcKey(app_id, "00", map_iter->first);
		item it = map_iter->second;
		ret = insert_index_dtc(dtcServer,key,it,3,doc_version,res);
		if(ret != 0)
			return RT_ERROR_INSERT_INDEX_DTC;
	}

	map_iter = title_map.begin();
	for (; map_iter != title_map.end(); map_iter++) {
		string key = CommonHelper::Instance()->GenerateDtcKey(app_id, "00", map_iter->first);
		item it = map_iter->second;
		ret = insert_index_dtc(dtcServer,key,it,3,doc_version,res);
		if(ret != 0){
			return RT_ERROR_INSERT_INDEX_DTC;
		}
	}

	return 0;
}

int CTaskImage::pre_process(void){
	DTCTools *dtc_tools = DTCTools::Instance();
	dtc_tools->init_servers(index_servers,IndexConf::Instance()->GetDTCIndexConfig());

	return 0;
}

void CTaskImage::do_stat_word_freq(vector<vector<string> > &strss, string &doc_id, uint32_t appid, map<string, item> &word_map,Json::Value &res) {
	string word;
	uint32_t id = 0;
	ostringstream oss;
	vector<vector<string> >::iterator iters = strss.begin();
	uint32_t index = 0;

	for(;iters != strss.end(); iters++){
		index++;
		vector<string>::iterator iter = iters->begin();

		log_debug("start do_stat_word_freq, appid = %u\n",appid);
		for (; iter != iters->end(); iter++) {

			word = *iter;
			if (!SplitManager::Instance()->wordValid(word, appid, id)){
				continue;
			}
			log_debug("id == %u\n",id);
			if (word_map.find(word) == word_map.end()) {
				item it;
				it.doc_id = doc_id;
				it.freq = 1;
				it.indexs.push_back(index);
				word_map.insert(make_pair(word, it));
			}
			else {
				word_map[word].freq++;
				word_map[word].indexs.push_back(index);
			}

			oss << (*iter) << "|";
		}
	}
	log_debug("split: %s",oss.str().c_str());
}

static int decode_request(const Json::Value &req, Json::Value &subreq, uint32_t &id, uint32_t &count){
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

int CTaskImage::update_sanpshot_dtc(const UserTableContent &fields,int doc_version,Json::Value &res){
	int ret = 0;
	DTC::Server* dtc_server = index_servers.GetServer();
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	DTC::UpdateRequest updateReq(dtc_server);
	ret = updateReq.SetKey(CommonHelper::Instance()->GenerateDtcKey(fields.appid, "00", fields.doc_id).c_str());
	updateReq.Set("doc_version", doc_version);
	if (fields.content.length() > 0)
		updateReq.Set("extend", fields.content.c_str());
	updateReq.Set("weight", fields.weight);
	updateReq.Set("created_time", fields.publish_time);

	DTC::Result rst;
	ret = updateReq.Execute(rst);
	if (ret != 0)
	{
		log_error("updateReq  error! ,errno %d ,errmsg %s, errfrom %s\n", ret, rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_UPDATE_SNAPSHOT;
	}

	return ret;
}

static int decode_fields(Json::Value table_content,UserTableContent &fields){
	string cmd;
	time_t now = time(NULL);
	if(table_content.isMember("cmd") && table_content["cmd"].isString()){
		cmd = table_content["cmd"].asString();
		if(cmd == "add" || cmd == "update"){
			if(table_content.isMember("fields") && table_content["fields"].isObject()){
				Json::Value field = table_content["fields"];
				if(field.isMember("doc_id") && field["doc_id"].isString()){
					fields.doc_id = field["doc_id"].asString();
				}
				if(field.isMember("title") && field["title"].isString()){
					fields.title = field["title"].asString();
				}
				if(field.isMember("content") && field["content"].isString()){
					fields.content = field["content"].asString();
				}
				if(field.isMember("author") && field["author"].isString()){
					fields.author = field["author"].asString();
				}
				if(field.isMember("url") && field["url"].isString()){
					fields.description = field["url"].asString();
				}
				if(field.isMember("weight") && field["weight"].isInt()){
					fields.weight = field["weight"].asInt();
				}else{
					fields.weight = 1;
				}
				if(field.isMember("publish_time") && field["publish_time"].isInt()){
					fields.publish_time = field["publish_time"].asInt();
				}else{
					fields.publish_time = now;
				}
				return RT_CMD_ADD;
			}
		}else if(cmd == "delete"){
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

int CTaskImage::index_gen_process(Json::Value &req,Json::Value &res){

	vector<vector<string> > split_content;
	vector<vector<string> > split_title;
	int doc_version = 0,old_version = 0;
	uint32_t app_id,fields_count = 0;
	int ret = 0;
	Json::Value table_content;

	map<string, item> word_map;
	map<string, item> title_map;

	ret = decode_request(req, table_content, app_id,fields_count);
	if(ret != 0){
		return ret;
	}
	if(fields_count == 0 || fields_count != table_content.size()){
		return RT_ERROR_FIELD_COUNT;
	}
	DTC::Server* dtcServer = index_servers.GetServer();
	for(int i = 0;i < (int)table_content.size();i++){
		doc_version = 0; old_version = 0;
		UserTableContent fields(app_id);
		ret = decode_fields(table_content[i],fields);
		if(RT_CMD_ADD == ret){
			ret = get_snapshot_active_doc(fields,old_version,res);
			if(0 == ret){
				doc_version = ++old_version;
			}else if(ret != RT_NO_THIS_DOC) return ret;
			split_content = SplitManager::Instance()->split(fields.content,fields.appid);
			split_title = SplitManager::Instance()->split(fields.title,fields.appid);
			do_stat_word_freq(split_content, fields.doc_id,fields.appid, word_map,res);
			do_stat_word_freq(split_title, fields.doc_id, fields.appid, title_map,res);
			ret = do_insert_index(dtcServer, word_map, title_map,app_id,doc_version,res);
			if(0 != ret){
				return ret;
			}
			if(doc_version != 0){//need update
				update_sanpshot_dtc(fields,doc_version,res);
			}else{
				insert_snapshot_dtc(fields,doc_version,res);//insert the snapshot doc
			}
			word_map.clear();
			title_map.clear();
		}
		else if(RT_CMD_DELETE == ret){
			ret = delete_snapshot_dtc(fields.doc_id,fields.appid,res);//not use the doc_version curr
		}
	}

	return ret;
}


void CTaskImage::TaskNotify(CTaskRequest * curr)
{
    log_debug("CTaskImage::TaskNotify start");
	common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.imageReportTask"));
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
	if(SERVICE_PIC != task->GetReqCmd()){
		res["code"] = RT_ERROR_SERVICE_TYPE;
		res["reqcmd"] = task->GetReqCmd();
		res["message"] = "service type wrong! need 109";
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
	ret = index_gen_process(value,res);
	if(0 != ret){
		res["code"] = ret;
	}

end:
	task->setResult(writer.write(res));
	task->ReplyNotify();

	common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
    return;
}





