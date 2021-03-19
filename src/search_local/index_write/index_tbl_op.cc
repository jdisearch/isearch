/*
 * =====================================================================================
 *
 *       Filename:  index_tbl_op.cc
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

#include "index_tbl_op.h"
#include "index_clipping.h"
#include "add_request_proc.h"
#include <sstream>

CIndexTableManager g_IndexInstance;
CIndexTableManager g_delIndexInstance;
CIndexTableManager g_hanpinIndexInstance;

static char* gen_access_key(string doc_id){
        static char tmp[41] = {'0'};
        snprintf(tmp, sizeof(tmp), "%40s", doc_id.c_str());
        return tmp;
}

static int get_snapshot_execute(DTC::Server* dtc_server, const UserTableContent &fields, DTC::Result &rst){
	DTC::GetRequest getReq(dtc_server);
	int ret = 0;

	ret = getReq.SetKey(gen_dtc_key_string(fields.appid, "10", fields.doc_id).c_str());
	ret = getReq.Need("trans_version");

	ret = getReq.Execute(rst);
	return ret;
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
	if (it.indexs.size() == 0) {
		index_str = "";
	}
	DTC::InsertRequest insertReq(dtcServer);
	insertReq.SetKey(key.c_str());
	insertReq.Set("doc_id", it.doc_id.c_str());
	insertReq.Set("field", field_type);
	insertReq.Set("word_freq", it.freq);
	insertReq.Set("weight", 1);
	insertReq.Set("extend", it.extend.c_str());
	insertReq.Set("doc_version",doc_version);
	insertReq.Set("trans_version",doc_version);
	insertReq.Set("created_time",time(NULL));
	insertReq.Set("location", index_str.c_str());
	insertReq.Set("start_time", 0);
	insertReq.Set("end_time", 0);
	ret = insertReq.Execute(rst);
	return ret;
}

static int insert_intelligent_execute(DTC::Server* dtcServer, string key, string doc_id, string word, const IntelligentInfo &info, DTC::Result &rst, int doc_version) {
	int ret = 0;
	DTC::InsertRequest insertReq(dtcServer);
	insertReq.SetKey(key.c_str());
	insertReq.Set("word", word.c_str());
	insertReq.Set("doc_id", doc_id.c_str());
	insertReq.Set("doc_version", doc_version);
	insertReq.Set("charact_id_01", info.charact_id[0]);
	insertReq.Set("charact_id_02", info.charact_id[1]);
	insertReq.Set("charact_id_03", info.charact_id[2]);
	insertReq.Set("charact_id_04", info.charact_id[3]);
	insertReq.Set("charact_id_05", info.charact_id[4]);
	insertReq.Set("charact_id_06", info.charact_id[5]);
	insertReq.Set("charact_id_07", info.charact_id[6]);
	insertReq.Set("charact_id_08", info.charact_id[7]);
	insertReq.Set("phonetic_id_01", info.phonetic_id[0]);
	insertReq.Set("phonetic_id_02", info.phonetic_id[1]);
	insertReq.Set("phonetic_id_03", info.phonetic_id[2]);
	insertReq.Set("phonetic_id_04", info.phonetic_id[3]);
	insertReq.Set("phonetic_id_05", info.phonetic_id[4]);
	insertReq.Set("phonetic_id_06", info.phonetic_id[5]);
	insertReq.Set("phonetic_id_07", info.phonetic_id[6]);
	insertReq.Set("phonetic_id_08", info.phonetic_id[7]);
	insertReq.Set("initial_char_01", info.initial_char[0].c_str());
	insertReq.Set("initial_char_02", info.initial_char[1].c_str());
	insertReq.Set("initial_char_03", info.initial_char[2].c_str());
	insertReq.Set("initial_char_04", info.initial_char[3].c_str());
	insertReq.Set("initial_char_05", info.initial_char[4].c_str());
	insertReq.Set("initial_char_06", info.initial_char[5].c_str());
	insertReq.Set("initial_char_07", info.initial_char[6].c_str());
	insertReq.Set("initial_char_08", info.initial_char[7].c_str());
	insertReq.Set("initial_char_09", info.initial_char[8].c_str());
	insertReq.Set("initial_char_10", info.initial_char[9].c_str());
	insertReq.Set("initial_char_11", info.initial_char[10].c_str());
	insertReq.Set("initial_char_12", info.initial_char[11].c_str());
	insertReq.Set("initial_char_13", info.initial_char[12].c_str());
	insertReq.Set("initial_char_14", info.initial_char[13].c_str());
	insertReq.Set("initial_char_15", info.initial_char[14].c_str());
	insertReq.Set("initial_char_16", info.initial_char[15].c_str());
	ret = insertReq.Execute(rst);
	return ret;
}

int CIndexTableManager::InitServer(const SDTCHost &dtchost) {
	string _MasterAddress = "127.0.0.1";
	stringstream ss;
	uint32_t port = 0;
	if (dtchost.vecRoute.size() > 0) {
		SDTCroute route = dtchost.vecRoute[0];
		port = route.uPort;
		_MasterAddress = route.szIpadrr;
	}
	ss << ":" << port << "/tcp";
	string master_bind_port = ss.str();
	_MasterAddress.append(master_bind_port);
	
	log_info("master address is  [%s]", _MasterAddress.c_str());

	server.StringKey();
	server.SetTableName(dtchost.szTablename.c_str());
	server.SetAddress(_MasterAddress.c_str());
	server.SetMTimeout(300);

	int ret;
	if ((ret = server.Ping()) != 0 && ret != -DTC::EC_TABLE_MISMATCH) {
		log_error("ping server[%s] failed, err: %d", _MasterAddress.c_str(), ret);
		return -1;
	}

	return ret;
}

bool CIndexTableManager::DeleteIndex(std::string word, const std::string& doc_id, uint32_t doc_version, uint32_t field){
	DTC::Server* dtc_server = &server;
	if (NULL == dtc_server) {
		log_error("dtc_server is null !");
		return false;
	}
	dtc_server->SetAccessKey(gen_access_key(doc_id));

	DTC::DeleteRequest delReq(dtc_server);
	int ret = delReq.SetKey(word.c_str());
	ret |= delReq.EQ("doc_id", doc_id.c_str());
	ret |= delReq.EQ("doc_version", doc_version);
	ret |= delReq.EQ("field", field);
	DTC::Result rst;
	ret = delReq.Execute(rst);
	if(ret != 0)
	{
		log_error("delete request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
		return false;
	}

	return true;
}

int CIndexTableManager::delete_snapshot_dtc(string &doc_id, uint32_t appid, Json::Value &res){
	int ret;
	DTC::Server* dtc_server = &server;
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	dtc_server->SetAccessKey(gen_access_key(doc_id));
	DTC::DeleteRequest deleteReq(dtc_server);
	ret = deleteReq.SetKey(gen_dtc_key_string(appid, "10", doc_id).c_str());

	DTC::Result rst;
	ret = deleteReq.Execute(rst);
	if (ret != 0)
	{
		log_error("delete request error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_DELETE_SNAPSHOT;
	}
	return 0;
}

int CIndexTableManager::delete_hanpin_index(string key, string doc_id) {
	int ret = 0;
	DTC::Server* dtcServer = &server;
	if(NULL == dtcServer){
		log_error("dtc server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	dtcServer->SetAccessKey(gen_access_key(doc_id));
	DTC::DeleteRequest deleteReq(dtcServer);
	ret = deleteReq.SetKey(key.c_str());
	ret = deleteReq.EQ("doc_id", doc_id.c_str());

	DTC::Result rst;
	ret = deleteReq.Execute(rst);
	if (ret != 0){
		log_error("delete request error! ,errno %d ,errmsg %s, errfrom %s\n", ret, rst.ErrorMessage(), rst.ErrorFrom());
	}
	else {
		log_debug("delete key = %s doc_id = %s", key.c_str(), doc_id.c_str());
	}

	return ret;
}

int CIndexTableManager::get_snapshot_active_doc(const UserTableContent &fields, int &doc_version, Json::Value &res){
	int ret;
	DTC::Server* dtc_server = &server;
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
	if (rst.NumRows() <= 0) {
		return RT_NO_THIS_DOC;
	}
	else {
		for (int i = 0; i < cnt; i++) {
			rst.FetchRow();
			doc_version = rst.IntValue("trans_version");
		}

	}

	return 0;
}

int CIndexTableManager::do_insert_index(map<string, item> &word_map, uint64_t app_id,int doc_version,int field,Json::Value &res) {
	int ret;
	map<string, item>::iterator map_iter = word_map.begin();
	for (; map_iter != word_map.end(); map_iter++) {
		string key = gen_dtc_key_string(app_id, "00", map_iter->first);
		item it = map_iter->second;
		ret = insert_index_dtc(key,it,field,doc_version,res);
		log_debug("key = %s,doc_vesion = %d,docid = %s\n",key.c_str(),doc_version,it.doc_id.c_str());
		if(ret != 0)
			return RT_ERROR_INSERT_INDEX_DTC;
	}

	return 0;
}

int CIndexTableManager::insert_index_dtc(string key, struct item &it, u_int8_t field_type, int doc_version, Json::Value &res){
	int ret = 0;

	DTC::Server* dtcServer = &server;
	res[field_type].append(key);

	char tmp[41] = { '0' };
	snprintf(tmp, sizeof(tmp), "%40s", it.doc_id.c_str());

	dtcServer->SetAccessKey(tmp);

	DTC::Result rst;
	ret = insert_index_execute(dtcServer, key, it, field_type, doc_version, rst);
	if (ret != 0)
	{
		log_error("insert request error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return -1;
	}
	return 0;
}

int CIndexTableManager::do_insert_intelligent(string key, string doc_id, string word, const vector<IntelligentInfo> & info_vec, int doc_version) {
	int ret = 0;

	DTC::Server* dtcServer = &server;


	char tmp[41] = { '0' };
	snprintf(tmp, sizeof(tmp), "%40s", doc_id.c_str());

	dtcServer->SetAccessKey(tmp);

	vector<IntelligentInfo>::const_iterator iter = info_vec.begin();
	for (; iter != info_vec.end(); iter++) {
		IntelligentInfo info = *iter;
		DTC::Result rst;
		ret = insert_intelligent_execute(dtcServer, key, doc_id, word, info, rst, doc_version);
		if (ret != 0)
		{
			log_error("insert request error! ,errno %d ,errmsg %s, errfrom %s\n", ret, rst.ErrorMessage(), rst.ErrorFrom());
			return -1;
		}
	}
	return 0;
}

int CIndexTableManager::update_sanpshot_dtc(const UserTableContent &fields,int doc_version,int trans_version,int &affected_rows){
	int ret = 0;
	DTC::Server* dtc_server = &server;
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	dtc_server->SetAccessKey(gen_access_key(fields.doc_id));
	DTC::UpdateRequest updateReq(dtc_server);
	ret = updateReq.SetKey(gen_dtc_key_string(fields.appid, "10", fields.doc_id).c_str());
	updateReq.Set("doc_version", doc_version);
	if(fields.content.length() > 0)
		updateReq.Set("extend", fields.content.c_str());
	updateReq.Set("weight",fields.weight);
	updateReq.Set("created_time",fields.publish_time);
	updateReq.EQ("trans_version", trans_version);

	DTC::Result rst;
	ret = updateReq.Execute(rst);
	if (ret != 0)
	{
		log_error("updateReq  error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_UPDATE_SNAPSHOT;
	}
	affected_rows = rst.AffectedRows();
	return ret;
}

int CIndexTableManager::update_sanpshot_dtc(uint32_t appid, string doc_id, int trans_version){
	DTC::Server* dtc_server = &server;
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	dtc_server->SetAccessKey(gen_access_key(doc_id));
	DTC::UpdateRequest updateReq(dtc_server);
	int ret = updateReq.SetKey(gen_dtc_key_string(appid, "10", doc_id).c_str());
	updateReq.Set("trans_version", trans_version - 1);
	updateReq.EQ("trans_version", trans_version);
	DTC::Result rst;
	ret = updateReq.Execute(rst);
	if (ret != 0)
	{
		log_error("delete request error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_DELETE_SNAPSHOT;
	}
	return 0;
}

int CIndexTableManager::update_snapshot_version(const UserTableContent &fields,int doc_version,int &affected_rows){
	int ret = 0;
	DTC::Server* dtc_server = &server;
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	dtc_server->SetAccessKey(gen_access_key(fields.doc_id));
	DTC::UpdateRequest updateReq(dtc_server);
	ret = updateReq.SetKey(gen_dtc_key_string(fields.appid, "10", fields.doc_id).c_str());
	updateReq.Set("trans_version", doc_version);
	updateReq.EQ("trans_version", doc_version - 1);
	DTC::Result rst;
	ret = updateReq.Execute(rst);
	if (ret != 0)
	{
		log_error("updateReq  error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_UPDATE_SNAPSHOT;
	}
	affected_rows = rst.AffectedRows();
	return ret;
}

int CIndexTableManager::insert_snapshot_version(const UserTableContent &fields,int doc_version){
	int ret = 0;
	DTC::Server* dtc_server = &server;
	if(NULL == dtc_server){
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	dtc_server->SetAccessKey(gen_access_key(fields.doc_id));
	DTC::InsertRequest insertReq(dtc_server);
	ret = insertReq.SetKey(gen_dtc_key_string(fields.appid, "10", fields.doc_id).c_str());
	insertReq.Set("doc_version", 0);
	insertReq.Set("trans_version", doc_version);
	insertReq.Set("doc_id", fields.doc_id.c_str());
	DTC::Result rst;
	ret = insertReq.Execute(rst);
	if (ret != 0)
	{
		log_error("insertReq  error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_INSERT_SNAPSHOT;
	}
	return ret;
}


int CIndexTableManager::update_docid_index_dtc(const string & invert_keys, const string & doc_id, uint32_t appid, int doc_version)
{
	int ret = 0;
	DTC::Server* dtc_server = &server;
	if (NULL == dtc_server) {
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	dtc_server->SetAccessKey(gen_access_key(doc_id));
	DTC::UpdateRequest updateReq(dtc_server);
	ret = updateReq.SetKey(gen_dtc_key_string(appid, "20", doc_id).c_str());
	updateReq.Set("doc_version", doc_version);
	if (invert_keys.length() > 0)
		updateReq.Set("extend", invert_keys.c_str());
	
	DTC::Result rst;
	ret = updateReq.Execute(rst);
	if (ret != 0)
	{
		log_error("updateReq  error! ,errno %d ,errmsg %s, errfrom %s\n", ret, rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_UPDATE_SNAPSHOT;
	}
	return ret;
}

int CIndexTableManager::insert_docid_index_dtc(const string & invert_keys, const string & doc_id, uint32_t appid, int doc_version)
{
	int ret = 0;
	DTC::Server* dtc_server = &server;
	if (NULL == dtc_server) {
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	dtc_server->SetAccessKey(gen_access_key(doc_id));
	DTC::InsertRequest insertReq(dtc_server);
	ret = insertReq.SetKey(gen_dtc_key_string(appid, "20", doc_id).c_str());
	insertReq.Set("doc_id", doc_id.c_str());
	insertReq.Set("doc_version", doc_version);
	if (invert_keys.length() > 0)
		insertReq.Set("extend", invert_keys.c_str());

	DTC::Result rst;
	ret = insertReq.Execute(rst);
	if (ret != 0)
	{
		log_error("updateReq  error! ,errno %d ,errmsg %s, errfrom %s\n", ret, rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_INSERT_INDEX_DTC;
	}
	return ret;
}

int CIndexTableManager::insert_union_index_dtc(const string & union_key, const string & doc_id, uint32_t appid, int doc_version)
{
	int ret = 0;
	DTC::Server* dtc_server = &server;
	if (NULL == dtc_server) {
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	dtc_server->SetAccessKey(gen_access_key(doc_id));
	DTC::InsertRequest insertReq(dtc_server);
	ret = insertReq.SetKey(gen_dtc_key_string(appid, "00", union_key).c_str());
	insertReq.Set("doc_id", doc_id.c_str());
	insertReq.Set("doc_version", doc_version);
	insertReq.Set("trans_version", doc_version);
	insertReq.Set("created_time", time(NULL));
	insertReq.Set("word_freq", 1);
	insertReq.Set("weight", 1);

	DTC::Result rst;
	ret = insertReq.Execute(rst);
	if (ret != 0)
	{
		log_error("updateReq  error! ,errno %d ,errmsg %s, errfrom %s\n", ret, rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_INSERT_INDEX_DTC;
	}
	return ret;
}

int CIndexTableManager::delete_docid_index_dtc(const string & key, const string & doc_id){
	int ret = 0;
	DTC::Server* dtc_server = &server;
	if (NULL == dtc_server) {
		log_error("snapshot server connect error!");
		return RT_ERROR_GET_SNAPSHOT;
	}
	dtc_server->SetAccessKey(gen_access_key(doc_id));
	DTC::DeleteRequest deleteReq(dtc_server);
	ret = deleteReq.SetKey(key.c_str());
	DTC::Result rst;
	ret = deleteReq.Execute(rst);
	if(ret != 0)
	{
		log_error("deleteReq  error! ,errno %d ,errmsg %s, errfrom %s\n", ret, rst.ErrorMessage(), rst.ErrorFrom());
		return RT_ERROR_DELETE_SNAPSHOT;
	}
	return ret;
}


bool CIndexTableManager::GetIndexData(const std::string& doc_id, uint32_t doc_version, map<uint32_t, vector<string> > &res){
	DTC::Server* dtc_server = &server;
	if (NULL == dtc_server) {
		log_error("dtc_server is null !");
		return false;
	}

	DTC::GetRequest getReq(dtc_server);
	int ret = getReq.SetKey(doc_id.c_str());
	ret |= getReq.EQ("doc_version", doc_version);
	ret |= getReq.Need("extend");

	DTC::Result rst;
	ret = getReq.Execute(rst);
	if(ret != 0)
	{
		log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
		return false;
 	}
	int cnt = rst.NumRows();
	if(cnt <= 0)
	{
		log_debug("can not find any result. key:%s", doc_id.c_str());
		return false;
	}
	rst.FetchRow();
	string extend = rst.StringValue("extend");
	Json::Reader reader;
	Json::Value value;
	if(!reader.parse(extend, value, false))
	{
		log_error("parse json error!\ndata:%s errors:%s\n",extend.c_str(),reader.getFormattedErrorMessages().c_str());
		return false;
	}
	if(value.isArray()){
		for(int i = 0;i < (int)value.size();i++){
			Json::Value info = value[i];
			if(info.isArray()){
				for(int j = 0;j < (int)info.size();j++){
					if(info[j].isString()){
						res[i].push_back(info[j].asString());
					}
				}
			}
		}
	}

	return true;
}

bool CIndexTableManager::delete_index(std::string word, const std::string& doc_id, uint32_t doc_version, uint32_t field){
	DTC::Server* dtc_server = &server;
	if (NULL == dtc_server) {
		log_error("dtc_server is null !");
		return false;
	}
	dtc_server->SetAccessKey(gen_access_key(doc_id));

	DTC::DeleteRequest delReq(dtc_server);
	int ret = delReq.SetKey(word.c_str());
	ret |= delReq.EQ("doc_id", doc_id.c_str());
	ret |= delReq.EQ("doc_version", doc_version);
	ret |= delReq.EQ("field", field);
	DTC::Result rst;
	ret = delReq.Execute(rst);
	if(ret != 0)
	{
		log_error("delete request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
		return false;
 	}

 	return true;
}

bool CIndexTableManager::delete_intelligent(std::string key, std::string doc_id, uint32_t trans_version){
	DTC::Server* intelligent_server = &server;
	if(NULL == intelligent_server){
		log_error("GetServer error!");
		return false;
	}
	intelligent_server->SetAccessKey(gen_access_key(doc_id));
	DTC::DeleteRequest deleteReq(intelligent_server);
	int ret = deleteReq.SetKey(key.c_str());
	deleteReq.EQ("doc_id", doc_id.c_str());
	deleteReq.EQ("doc_version", trans_version);

	DTC::Result rst;
	ret = deleteReq.Execute(rst);
	if (ret != 0)
	{
		log_error("delete request error! ,errno %d ,errmsg %s, errfrom %s\n", ret,rst.ErrorMessage(), rst.ErrorFrom());
		return false;
	}
	return true;
}


void * DeleteTask::ProcessCycle(void * arg)
{
	int statistic_period = 5;
	pthread_mutex_lock(&DeleteTask::GetInstance()._Mutex);
	std::vector<DeleteItem> temp_result;
	int last_append_time = time(NULL);
	while (!DeleteTask::GetInstance()._StopFlag) {
		if (DeleteTask::GetInstance()._InfoHead == NULL) {
			if(temp_result.size() != 0){
				for (std::vector<DeleteItem>::iterator it = temp_result.begin(); it != temp_result.end(); it++) {
					DeleteItem item = *it;
					g_delIndexInstance.DeleteIndex(item.word, item.doc_id, item.doc_version, item.field);
				}
				temp_result.clear();
			}
			pthread_cond_wait(&DeleteTask::GetInstance()._NotEmpty, &DeleteTask::GetInstance()._Mutex);
			continue;
		}
		DeleteItem *head = DeleteTask::GetInstance()._InfoHead;
		DeleteTask::GetInstance()._InfoHead = DeleteTask::GetInstance()._InfoTail = NULL;
		pthread_mutex_unlock(&DeleteTask::GetInstance()._Mutex);
		
		DeleteTask::GetInstance().Coalesce(head, temp_result);
		int now_time = time(NULL);
		if (now_time - last_append_time >= statistic_period) {
			last_append_time = now_time;
			for (std::vector<DeleteItem>::iterator it = temp_result.begin(); it != temp_result.end(); it++) {
				DeleteItem item = *it;
				g_delIndexInstance.DeleteIndex(item.word, item.doc_id, item.doc_version, item.field);
			}
			temp_result.clear();
		}
		
		pthread_mutex_lock(&DeleteTask::GetInstance()._Mutex);
	}
	return NULL;
}

void  DeleteTask::Coalesce(DeleteItem * head, std::vector<DeleteItem>& temp_result)
{
	DeleteItem *p = head;
	DeleteItem *q;
	while (p != NULL) {

		std::vector<DeleteItem>::iterator it = temp_result.begin();	
		for ( ; it != temp_result.end(); it++) {
			if (*it == *p) {
				break;
			}
		}
		if (temp_result.size() == 0  || it == temp_result.end()) {
			temp_result.push_back(*p);
		}

		q = p; 
		p = p->_Next;
		delete q;		
	}

}

bool DeleteTask::Initialize(){
	_InfoHead = _InfoTail = NULL;
	return true;
}

void DeleteTask::RegisterInfo(const std::string& word, const std::string& doc_id, uint32_t doc_version, uint32_t field) {
	DeleteItem *item =  new DeleteItem();
	 if (item != NULL) {
		item->word = word;
		item->doc_id = doc_id;
		item->doc_version = doc_version;
		item->field = field;
		PushReportItem(item);
	 }
}
