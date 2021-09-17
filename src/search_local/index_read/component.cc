/*
 * =====================================================================================
 *
 *       Filename:  component.h
 *
 *    Description:  component class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2019
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "component.h"
#include "split_manager.h"
#include "db_manager.h"
#include "utf8_str.h"
#include <sstream>
using namespace std;

Component::Component(){
	SGlobalConfig &global_cfg = SearchConf::Instance()->GetGlobalConfig();
	m_default_query = global_cfg.sDefaultQuery;
	m_jdq_switch = global_cfg.iJdqSwitch;
	m_page_index = 0;
	m_page_size = 0;
	m_return_all = 0;
	m_cache_switch = 0;
	m_top_switch = 0;
	m_snapshot_switch = 0;
	m_sort_type = SORT_RELEVANCE;
	m_appid = 10001;
	m_user_id = "0";
	m_last_id = "";
	m_last_score = "";
	m_search_after = false;
	distance = 0;
	m_terminal_tag = 0;
	m_terminal_tag_valid = false;
}

Component::~Component(){

}

int Component::ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet)
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

	if (recv_packet.isMember("userid") && recv_packet["userid"].isString())
	{
		m_user_id = recv_packet["userid"].asString();
	}
	else {
		m_user_id = "0";
	}

	if (recv_packet.isMember("key") && recv_packet["key"].isString())
	{
		m_Data = recv_packet["key"].asString();
	}
	else {
		m_Data = "";
	}

	if (recv_packet.isMember("key_and") && recv_packet["key_and"].isString())
	{
		m_Data_and = recv_packet["key_and"].asString();
	}
	else {
		m_Data_and = "";
	}

	if (recv_packet.isMember("key_invert") && recv_packet["key_invert"].isString())
	{
		m_Data_invert = recv_packet["key_invert"].asString();
	}
	else {
		m_Data_invert = "";
	}

	if (recv_packet.isMember("key_complete") && recv_packet["key_complete"].isString())
	{
		m_Data_complete = recv_packet["key_complete"].asString();
	}
	else {
		m_Data_complete = "";
	}

	if (recv_packet.isMember("page_index") && recv_packet["page_index"].isString())
	{
		m_page_index = atoi(recv_packet["page_index"].asString().c_str());
	}
	else {
		m_page_index = 1 ;
	}

	if (recv_packet.isMember("page_size") && recv_packet["page_size"].isString())
	{
		m_page_size = atoi(recv_packet["page_size"].asString().c_str());
	}
	else {
		m_page_size = 10;
	}

	if(recv_packet.isMember("sort_type") && recv_packet["sort_type"].isString())
	{
		m_sort_type = atoi(recv_packet["sort_type"].asString().c_str());
	}
	else {
		m_sort_type = SORT_RELEVANCE;
	}

	if(recv_packet.isMember("sort_field") && recv_packet["sort_field"].isString())
	{
		m_sort_field = recv_packet["sort_field"].asString();
	}
	else {
		m_sort_field = "";
	}

	if (recv_packet.isMember("return_all") && recv_packet["return_all"].isString())
	{
		m_return_all = atoi(recv_packet["return_all"].asString().c_str());
	}
	else {
		m_return_all = 0;
	}

	if(recv_packet.isMember("fields") && recv_packet["fields"].isString())
	{
		string fields = recv_packet["fields"].asString();
		m_fields = splitEx(fields, ",");
	}

	if (recv_packet.isMember("terminal_tag") && recv_packet["terminal_tag"].isString())
	{
		m_terminal_tag = atoi(recv_packet["terminal_tag"].asString().c_str());
	}
	else {
		m_terminal_tag = 0;
	}

	if(m_terminal_tag == 1){
		if(m_Data_and == "" || m_Data != "" || m_Data_invert != ""){
			log_error("terminal_tag is true, only key_and is available.");
			return -RT_PARSE_JSON_ERR;
		}
	}

	if(recv_packet.isMember("last_id") && recv_packet["last_id"].isString())
	{
		m_last_id = recv_packet["last_id"].asString();
	}
	else {
		m_last_id = "";
	}

	bool score_flag = true;
	if (recv_packet.isMember("last_score") && recv_packet["last_score"].isString())
	{
		m_last_score = recv_packet["last_score"].asString();
	}
	else {
		score_flag = false;
		m_last_score = "0";
	}
	if(m_last_id != "" && score_flag == true){
		m_search_after = true;
	}
	if(m_search_after == true && m_sort_type != SORT_FIELD_DESC && m_sort_type != SORT_FIELD_ASC){
		log_error("in search_after mode, sort_type must be SORT_FIELD_DESC or SORT_FIELD_ASC.");
		return -RT_PARSE_JSON_ERR;
	}
	if ("" == m_Data && "" == m_Data_and && "" == m_Data_complete) {
		m_Data = m_default_query;
	}
	log_debug("parse success, m_Data: %s, m_Data_and: %s, m_Data_invert: %s, m_page_index: %u, m_return_all: %u",
		m_Data.c_str(), m_Data_and.c_str(), m_Data_invert.c_str(), m_page_index, m_return_all);

	return 0;
}

void Component::InitSwitch()
{
	AppInfo app_info;
	bool res = SearchConf::Instance()->GetAppInfo(m_appid, app_info);
	if (true == res){
		m_cache_switch = app_info.cache_switch;
		m_top_switch = app_info.top_switch;
		m_snapshot_switch = app_info.snapshot_switch;
	}
}

void Component::GetQueryWord(uint32_t &m_has_gis){
	GetFieldWords(MAINKEY, m_Data, m_appid, m_has_gis);
	GetFieldWords(ANDKEY, m_Data_and, m_appid, m_has_gis);
	GetFieldWords(INVERTKEY, m_Data_invert, m_appid, m_has_gis);
}

void Component::GetFieldWords(int type, string dataStr, uint32_t appid, uint32_t &m_has_gis){
	if (dataStr == "")
		return ;
	string latitude_tmp = "";
	string longitude_tmp = "";
	string gisip_tmp = "";
	string field_Data = "";
	string primary_Data = "";
	vector<FieldInfo> joinFieldInfos;
	int i = dataStr.find(":");
	if (i == -1) {
		primary_Data = dataStr;
	} else {
		int j = dataStr.substr(0, i).rfind(" ");
		if (j == -1) {
			field_Data = dataStr;
			primary_Data = "";
		} else {
			primary_Data = dataStr.substr(0, j);
			field_Data = dataStr.substr(j+1);
		}
	}

	if (type == 0) {
		m_Query_Word = primary_Data;
	}

	if (primary_Data.length() > MAX_SEARCH_LEN) { // 超长进行截断
		primary_Data = primary_Data.substr(0, MAX_SEARCH_LEN);
	}

	string probably_key = "";
	bool is_correct = false;
	if(primary_Data != "" && primary_Data.length() <= SINGLE_WORD_LEN)   // 判断输入的词语是否正确，如果超过一定长度，则认为是多个词组成
	{
		JudgeWord(appid, primary_Data, is_correct, probably_key);
		m_probably_data = probably_key;
	}
	vector<FieldInfo> primaryInfo;
	FieldInfo pInfo;
	string split_data;
	if (is_correct == true) {
		pInfo.field = INT_MAX;
		pInfo.word = primary_Data;
		primaryInfo.push_back(pInfo);
		DataManager::Instance()->GetSynonymByKey(primary_Data, primaryInfo);
	}
	else if (probably_key != "") {
		pInfo.word = probably_key;
		primaryInfo.push_back(pInfo);
		DataManager::Instance()->GetSynonymByKey(probably_key, primaryInfo);
	}
	else if (primary_Data != ""){
		split_data = SplitManager::Instance()->split(primary_Data, appid);
		log_debug("split_data: %s", split_data.c_str());
		vector<string> split_datas = splitEx(split_data, "|");
		for(size_t i = 0; i < split_datas.size(); i++)  //是否有重复的同义词存在？
		{
			pInfo.field = INT_MAX;
			pInfo.word = split_datas[i];
			primaryInfo.push_back(pInfo);
			DataManager::Instance()->GetSynonymByKey(split_datas[i], primaryInfo);
		}
	}
	AddToFieldList(type, primaryInfo);

	vector<string> gisCode;
	vector<string> vec = splitEx(field_Data, " ");
	vector<string>::iterator iter;
	map<uint32_t, vector<FieldInfo> > field_keys_map;
	uint32_t range_query = 0;
	vector<string> lng_arr;
	vector<string> lat_arr;
	for (iter = vec.begin(); iter < vec.end(); iter++)
	{
		vector<FieldInfo> fieldInfos;
		if ((*iter)[0] == '\0')
			continue;
		vector<string> tmp = splitEx(*iter, ":");
		if (tmp.size() != 2)
			continue;
		uint32_t segment_tag = 0;
		FieldInfo fieldInfo;
		string fieldname = tmp[0];

		uint32_t field = DBManager::Instance()->GetWordField(segment_tag, appid, fieldname, fieldInfo);
		if(field != 0 && fieldInfo.index_tag == 0){
			ExtraFilterKey extra_filter_key;
			extra_filter_key.field_name = fieldname;
			extra_filter_key.field_value = tmp[1];
			extra_filter_key.field_type = fieldInfo.field_type;
			if(type == 0){
				extra_filter_keys.push_back(extra_filter_key);
			} else if (type == 1) {
				extra_filter_and_keys.push_back(extra_filter_key);
			} else if (type == 2) {
				extra_filter_invert_keys.push_back(extra_filter_key);
			}
			continue;
		}
		if (field != 0 && segment_tag == 1) 
		{
			string split_data = SplitManager::Instance()->split(tmp[1], appid);
			log_debug("split_data: %s", split_data.c_str());
			vector<string> split_datas = splitEx(split_data, "|");
			for(size_t index = 0; index < split_datas.size(); index++)
			{
				FieldInfo info;
				info.field = fieldInfo.field;
				info.field_type = fieldInfo.field_type;
				info.word = split_datas[index];
				info.segment_tag = fieldInfo.segment_tag;
				fieldInfos.push_back(info);
			}
		}
		else if (field != 0 && segment_tag == 5) {
			range_query++;
			string str = tmp[1];
			str.erase(0, str.find_first_not_of(" "));
			str.erase(str.find_last_not_of(" ") + 1);
			if (str.size() == 0) {
				log_error("field[%s] content is null", fieldname.c_str());
				continue;
			}
			if (str[0] == '[') { // 范围查询
				int l = str.find(",");
				if (l == -1 || (str[str.size() - 1] != ']' && str[str.size() - 1] != ')')) {
					log_error("field[%s] content[%s] invalid", fieldname.c_str(), str.c_str());
					continue;
				}
				istringstream iss(str.substr(1, l).c_str());
				iss >> fieldInfo.start;
				string end_str = str.substr(l + 1, str.size() - l - 2);
				end_str.erase(0, end_str.find_first_not_of(" "));
				istringstream end_iss(end_str);
				end_iss >> fieldInfo.end;

				if (str[str.size() - 1] == ']') {
					fieldInfo.range_type = RANGE_GELE;
				}
				else {
					if (end_str.size() == 0) {
						fieldInfo.range_type = RANGE_GE;
					}
					else {
						fieldInfo.range_type = RANGE_GELT;
					}
				}
				fieldInfos.push_back(fieldInfo);
			}
			else if (str[0] == '(') {
				int l = str.find(",");
                if (l == -1 || (str[str.size() - 1] != ']' && str[str.size() - 1] != ')')) {
					log_error("field[%s] content[%s] invalid", fieldname.c_str(), str.c_str());
					continue;
				}
				string start_str = str.substr(1, l).c_str();
				string end_str = str.substr(l + 1, str.size() - l - 2);
				start_str.erase(0, start_str.find_first_not_of(" "));
				end_str.erase(0, end_str.find_first_not_of(" "));
				istringstream start_iss(start_str);
				start_iss >> fieldInfo.start;
				istringstream end_iss(end_str);
				end_iss >> fieldInfo.end;

				if (str[str.size() - 1] == ']') {
					if (start_str.size() == 0) {
						fieldInfo.range_type = RANGE_LE;
					}
					else {
						fieldInfo.range_type = RANGE_GTLE;
					}
				}
				else {
					if (start_str.size() != 0 && end_str.size() != 0) {
						fieldInfo.range_type = RANGE_GTLT;
					}
					else if (start_str.size() == 0 && end_str.size() != 0) {
						fieldInfo.range_type = RANGE_LT;
					}
					else if (start_str.size() != 0 && end_str.size() == 0) {
						fieldInfo.range_type = RANGE_GT;
					}
					else {
						log_error("field[%s] content[%s] invalid", fieldname.c_str(), str.c_str());
						continue;
					}
				}
                fieldInfos.push_back(fieldInfo);
			}
			else {
				fieldInfo.word = tmp[1];
				fieldInfos.push_back(fieldInfo);
			}
            log_debug("range_type: %d, start: %u, end: %u, segment_tag: %d, word: %s", fieldInfo.range_type, fieldInfo.start, fieldInfo.end, fieldInfo.segment_tag, fieldInfo.word.c_str());
		}
		else if (field != 0) 
		{
			fieldInfo.word = tmp[1];
			fieldInfos.push_back(fieldInfo);
		} 
		else if (field == 0) 
		{
			if (fieldInfo.field_type == 5) {
				longitude_tmp = tmp[1];
				longitude = longitude_tmp;
			} else if (fieldInfo.field_type == 6) {
				latitude_tmp = tmp[1];
				latitude = latitude_tmp;
			} else if (fieldInfo.field_type == 8) {
				distance = strToDouble(tmp[1]);
			} else if (fieldInfo.field_type == 7) {
				gisip_tmp = tmp[1];
				gisip = gisip_tmp;
			} else if (fieldInfo.field_type == FIELD_WKT) {
				string str = tmp[1];
				str = delPrefix(str);
				vector<string> str_vec = splitEx(str, ",");
				for(uint32_t str_vec_idx = 0; str_vec_idx < str_vec.size(); str_vec_idx++){
					string wkt_str = trim(str_vec[str_vec_idx]);
					vector<string> wkt_vec = splitEx(wkt_str, "-");
					if(wkt_vec.size() == 2){
						lng_arr.push_back(wkt_vec[0]);
						lat_arr.push_back(wkt_vec[1]);
					}
				}
			}
		}
		if (fieldInfos.size() != 0) {
			field_keys_map.insert(make_pair(fieldInfo.field, fieldInfos));
		}
	}

	double distance_tmp = 2; // 不指定distance时最多返回2km内的数据
	if(distance > 1e-6 && distance_tmp > distance + 1e-6){ // distance大于0小于2时取distance的值
		distance_tmp = distance;
	}
	GetGisCode(longitude_tmp, latitude_tmp, gisip_tmp, distance_tmp, gisCode);
	log_debug("lng_arr size: %d, lat_arr size: %d", (int)lng_arr.size(), (int)lat_arr.size());
	if (gisCode.size() == 0 && lng_arr.size() > 0){
		GetGisCode(lng_arr, lat_arr, gisCode);
	}
	if(gisCode.size() > 0){
		vector<FieldInfo> fieldInfos;
		uint32_t segment_tag = 0;
		FieldInfo fieldInfo;
		uint32_t field = DBManager::Instance()->GetWordField(segment_tag, appid, "gis", fieldInfo);
		if (field != 0 && segment_tag == 0) {
			m_has_gis = 1; 
			for (size_t index = 0; index < gisCode.size(); index++) {
				FieldInfo info;
				info.field = fieldInfo.field;
				info.field_type = fieldInfo.field_type;
				info.segment_tag = fieldInfo.segment_tag;
				info.word = gisCode[index];
				fieldInfos.push_back(info);
			}
		}
		if (fieldInfos.size() != 0) {
			field_keys_map.insert(make_pair(fieldInfo.field, fieldInfos));
		}
	}

	//如果key_and查询的field匹配到联合索引，则将查询词拼接起来作为新的查询词
	if(type == 1){
		vector<string> union_key_vec;
		DBManager::Instance()->GetUnionKeyField(appid, union_key_vec);
		vector<string>::iterator union_key_iter = union_key_vec.begin();
		for(; union_key_iter != union_key_vec.end(); union_key_iter++){
			string union_key = *union_key_iter;
			vector<int> union_field_vec = splitInt(union_key, ",");
			vector<int>::iterator union_field_iter = union_field_vec.begin();
			bool hit_union_key = true;
			for(; union_field_iter != union_field_vec.end(); union_field_iter++){
				if(field_keys_map.find(*union_field_iter) == field_keys_map.end()){
					hit_union_key = false;
					break;
				}
			}
			if(hit_union_key == true){
				vector<vector<string> > keys_vvec;
				vector<FieldInfo> unionFieldInfos;
				for(union_field_iter = union_field_vec.begin(); union_field_iter != union_field_vec.end(); union_field_iter++){
					vector<FieldInfo> field_info_vec = field_keys_map.at(*union_field_iter);
					vector<string> key_vec;
					GetKeyFromFieldInfo(field_info_vec, key_vec);
					keys_vvec.push_back(key_vec);
					field_keys_map.erase(*union_field_iter);  // 命中union_key的需要从field_keys_map中删除
				}
				vector<string> union_keys = Combination(keys_vvec);
				for(int m = 0 ; m < (int)union_keys.size(); m++){
					FieldInfo info;
					info.field = 0;
					info.field_type = FIELD_STRING;
					info.segment_tag = 1;
					info.word = union_keys[m];
					unionFieldInfos.push_back(info);
				}
				AddToFieldList(type, unionFieldInfos);
				log_debug("hit union key index.");
				break;
			}
		}
		map<uint32_t, vector<FieldInfo> >::iterator field_key_map_iter = field_keys_map.begin();
		for(; field_key_map_iter != field_keys_map.end(); field_key_map_iter++){
			AddToFieldList(type, field_key_map_iter->second);
		}
	} else {
		map<uint32_t, vector<FieldInfo> >::iterator field_key_map_iter = field_keys_map.begin();
		for(; field_key_map_iter != field_keys_map.end(); field_key_map_iter++){
			AddToFieldList(type, field_key_map_iter->second);
		}
	}

	if(type == 1){ // terminal_tag为1时，key_and中必须只带有一个范围查询
		if(m_terminal_tag == 1 && range_query == 1 && and_keys.size() == 1){
			m_terminal_tag_valid = true;
		}
	}

	return ;
}

void Component::AddToFieldList(int type, vector<FieldInfo>& fields)
{
	if (fields.size() == 0)
		return ;
	if (type == 0) {
		keys.push_back(fields);
	} else if (type == 1) {
		and_keys.push_back(fields);
	} else if (type == 2) {
		invert_keys.push_back(fields);
	}
	return ;
}

const vector<vector<FieldInfo> >& Component::Keys(){
	return keys;
}

const vector<vector<FieldInfo> >& Component::AndKeys(){
	return and_keys;
}

const vector<vector<FieldInfo> >& Component::InvertKeys(){
	return invert_keys;
}

const vector<ExtraFilterKey>& Component::ExtraFilterKeys(){
	return extra_filter_keys;
}

const vector<ExtraFilterKey>& Component::ExtraFilterAndKeys(){
	return extra_filter_and_keys;
}

const vector<ExtraFilterKey>& Component::ExtraFilterInvertKeys(){
	return extra_filter_invert_keys;
}

string Component::QueryWord(){
	return m_Query_Word;
}

void Component::SetQueryWord(string query_word){
	m_Query_Word = query_word;
}

string Component::ProbablyData(){
	return m_probably_data;
}

void Component::SetProbablyData(string probably_data){
	m_probably_data = probably_data;
}

string Component::Latitude(){
	return latitude;
}

string Component::Longitude(){
	return longitude;
}

double Component::Distance(){
	return distance;
}

string Component::Data(){
	return m_Data;
}

uint32_t Component::JdqSwitch(){
	return m_jdq_switch;
}

uint32_t Component::Appid(){
	return m_appid;
}

string Component::DataAnd(){
	return m_Data_and;
}

string Component::DataInvert(){
	return m_Data_invert;
}

string Component::DataComplete(){
	return m_Data_complete;
}

uint32_t Component::SortType(){
	return m_sort_type;
}

uint32_t Component::PageIndex(){
	return m_page_index;
}
uint32_t Component::PageSize(){
	return m_page_size;
}

uint32_t Component::ReturnAll(){
	return m_return_all;
}

uint32_t Component::CacheSwitch(){
	return m_cache_switch;
}

uint32_t Component::TopSwitch(){
	return m_top_switch;
}

uint32_t Component::SnapshotSwitch(){
	return m_snapshot_switch;
}

string Component::SortField(){
	return m_sort_field;
}

string Component::LastId(){
	return m_last_id;
}

string Component::LastScore(){
	return m_last_score;
}

bool Component::SearchAfter(){
	return m_search_after;
}

vector<string>& Component::Fields(){
	return m_fields;
}

uint32_t Component::TerminalTag(){
	return m_terminal_tag;
}

bool Component::TerminalTagValid(){
	return m_terminal_tag_valid;
}

void Component::GetKeyFromFieldInfo(const vector<FieldInfo>& field_info_vec, vector<string>& key_vec){
	vector<FieldInfo>::const_iterator iter = field_info_vec.begin();
	for(; iter != field_info_vec.end(); iter++){
		key_vec.push_back((*iter).word);
	}
}

/*
** 通过递归求出二维vector每一维vector中取一个数的各种组合
** 输入：[[a],[b1,b2],[c1,c2,c3]]
** 输出：[a_b1_c1,a_b1_c2,a_b1_c3,a_b2_c1,a_b2_c2,a_b2_c3]
*/
vector<string> Component::Combination(vector<vector<string> > &dimensionalArr){
	int FLength = dimensionalArr.size();
	if(FLength >= 2){
		int SLength1 = dimensionalArr[0].size();
		int SLength2 = dimensionalArr[1].size();
		int DLength = SLength1 * SLength2;
		vector<string> temporary(DLength);
		int index = 0;
		for(int i = 0; i < SLength1; i++){
			for (int j = 0; j < SLength2; j++) {
				temporary[index] = dimensionalArr[0][i] +"_"+ dimensionalArr[1][j];
				index++;
			}
		}
		vector<vector<string> > new_arr;
		new_arr.push_back(temporary);
		for(int i = 2; i < (int)dimensionalArr.size(); i++){
			new_arr.push_back(dimensionalArr[i]);
		}
		return Combination(new_arr);
	} else {
		return dimensionalArr[0];
	}
}