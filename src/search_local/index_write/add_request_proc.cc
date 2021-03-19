/*
 * =====================================================================================
 *
 *       Filename:  add_request_proc.cc
 *
 *    Description:  AddReqProc class definition.
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

#include "add_request_proc.h"
#include "index_tbl_op.h"
#include "geohash.h"
#include "split_manager.h"
#include <sstream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iomanip>

AddReqProc::AddReqProc(){
}

AddReqProc::AddReqProc(const Json::Value& jf, InsertParam& insert_param){
	doc_version = insert_param.doc_version;
	trans_version = insert_param.trans_version;
	app_id = insert_param.appid;
	doc_id = insert_param.doc_id;
	json_field = jf;
}

AddReqProc::~AddReqProc(){
}

void AddReqProc::do_stat_word_freq(vector<vector<string> > &strss, map<string, item> &word_map, string extend) {
	string word;
	uint32_t id = 0;
	ostringstream oss;
	vector<vector<string> >::iterator iters = strss.begin();
	uint32_t index = 0;

	for(;iters != strss.end(); iters++){
		index++;
		vector<string>::iterator iter = iters->begin();

		log_debug("start do_stat_word_freq, appid = %u\n",app_id);
		for (; iter != iters->end(); iter++) {

			word = *iter;
			if (!SplitManager::Instance()->wordValid(word, app_id, id)){
				continue;
			}
			if (word_map.find(word) == word_map.end()) {
				item it;
				it.doc_id = doc_id;
				it.freq = 1;
				it.extend = extend;
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

void AddReqProc::do_stat_word_freq(vector<string> &strss, map<string, item> &word_map) {
	string word;
	vector<string>::iterator iters = strss.begin();
	uint32_t index = 0;

	for (; iters != strss.end(); iters++) {
		index++;
		word = *iters;
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
	}
}

int AddReqProc::deal_index_tag(struct table_info *tbinfo, string field_name){
	int ret =0;
	map<string, item> word_map;
	vector<vector<string> > split_content;
	switch(tbinfo->field_type){
		case FIELD_STRING:
		case FIELD_TEXT:
			if(json_field[field_name].isString()){
				if (tbinfo->segment_tag == SEGMENT_NGRAM) { // NGram split mode
					vector<string> ngram_content = SplitManager::Instance()->split(json_field[field_name].asString());
					do_stat_word_freq(ngram_content, word_map);
				}
				else if (tbinfo->segment_tag == SEGMENT_CHINESE || tbinfo->segment_tag == SEGMENT_ENGLISH) { // use intelligent_info
					string str = json_field[field_name].asString();
					// segment_tag为3对应的字段内容必须为全中文，为4对应的的字段不能包含中文
					if (tbinfo->segment_tag == SEGMENT_CHINESE && allChinese(str) == false) {
						log_error("segment_tag is 3, the content[%s] must be Chinese.", str.c_str());
						return RT_ERROR_FIELD_FORMAT;
					}
					if (tbinfo->segment_tag == SEGMENT_ENGLISH && noChinese(str) == false) {
						log_error("segment_tag is 4, the content[%s] can not contain Chinese.", str.c_str());
						return RT_ERROR_FIELD_FORMAT;
					}
					item it;
					it.doc_id = doc_id;
					it.freq = 1;
					if(tbinfo->segment_feature == SEGMENT_FEATURE_SNAPSHOT){
						Json::FastWriter ex_writer;
						it.extend = ex_writer.write(snapshot_content);
					}
					word_map.insert(make_pair(str, it));
					vector<IntelligentInfo> info;
					bool flag = false;
					get_intelligent(str, info, flag);
					if (flag) {
						stringstream ss;
						ss << app_id << "#" << tbinfo->field_value;
						ret = g_hanpinIndexInstance.do_insert_intelligent(ss.str(), doc_id, str, info, doc_version);
						if(0 != ret){
						 	roll_back();
						 	return ret;
						}
						intelligent_keys.push_back(ss.str());
					}
				}
				else {
					split_content = SplitManager::Instance()->split(json_field[field_name].asString(), app_id);
					string extend = "";
					if(tbinfo->segment_feature == SEGMENT_FEATURE_SNAPSHOT){
						Json::FastWriter ex_writer;
						extend = ex_writer.write(snapshot_content);
					}
					do_stat_word_freq(split_content, word_map, extend);
					split_content.clear();
				}
				ret = g_IndexInstance.do_insert_index(word_map, app_id, doc_version, tbinfo->field_value, docid_index_map);
				if (0 != ret) {
				 	roll_back();
					return ret;
				}
				word_map.clear();
			}else{
			 	log_error("field type error, not FIELD_STRING.");
				return RT_ERROR_FIELD_FORMAT;
			}
			break;
		case FIELD_INT:
			if(json_field[field_name].isInt()){
				int ret;
				struct item it;
				it.doc_id = doc_id;
				it.freq = 0;
				string key = "";
				if(tbinfo->segment_tag == SEGMENT_RANGE){  // 范围查的字段将key补全到20位
					stringstream ss;
					ss << setw(20) << setfill('0') << json_field[field_name].asInt();
					key = gen_dtc_key_string(app_id, "00", ss.str());
				} else {
				 	key = gen_dtc_key_string(app_id, "00", (uint32_t)json_field[field_name].asInt());
				}
				ret = g_IndexInstance.insert_index_dtc(key, it, tbinfo->field_value, doc_version, docid_index_map);
				if(ret != 0){
				 	roll_back();
					return RT_ERROR_INSERT_INDEX_DTC;
				}
			}else{
				log_error("field type error, not FIELD_INT.");
				return RT_ERROR_FIELD_FORMAT;
			}
			break;
		case FIELD_LONG:
			if(json_field[field_name].isInt64()){
				struct item it;
				it.doc_id = doc_id;
				it.freq = 0;
				string key = gen_dtc_key_string(app_id, "00", (int64_t)json_field[field_name].asInt64());
				int ret = g_IndexInstance.insert_index_dtc(key, it, tbinfo->field_value, doc_version, docid_index_map);
				if(0 != ret){
					roll_back();
					log_error("insert_index_dtc error, appid[%d], key[%s]", app_id, key.c_str());
					return RT_ERROR_INSERT_INDEX_DTC;
				}
			} else {
				log_error("field type error, not FIELD_LONG.");
				return RT_ERROR_FIELD_FORMAT;
			}
			break;
		case FIELD_DOUBLE:
			if(json_field[field_name].isDouble()){
				struct item it;
				it.doc_id = doc_id;
				it.freq = 0;
				string key = gen_dtc_key_string(app_id, "00", (double)json_field[field_name].asDouble());
				int ret = g_IndexInstance.insert_index_dtc(key, it, tbinfo->field_value, doc_version, docid_index_map);
				if(0 != ret){
					roll_back();
					log_error("insert_index_dtc error, appid[%d], key[%s]", app_id, key.c_str());
					return RT_ERROR_INSERT_INDEX_DTC;
				}
			} else {
				log_error("field type error, not FIELD_DOUBLE.");
				return RT_ERROR_FIELD_FORMAT;
			}
			break;
		case FIELD_IP:
			uint32_t s;
			int ret;
			if(json_field[field_name].isString()){
				ret = inet_pton(AF_INET, json_field[field_name].asString().c_str(), (void *)&s);
				if(ret == 0){
					log_error("ip format is error\n");
					return RT_ERROR_FIELD_FORMAT;
				}
				struct item it;
				it.doc_id = doc_id;
				it.freq = 0;
				string key = gen_dtc_key_string(app_id, "00", ntohl(s));
				ret = g_IndexInstance.insert_index_dtc(key, it, tbinfo->field_value, doc_version, docid_index_map);
				if(ret != 0){
					roll_back();
					return RT_ERROR_INSERT_INDEX_DTC;
				}
			}else{
				return RT_ERROR_FIELD_FORMAT;
			}
			break;
		case FIELD_LNG:
			if(json_field[field_name].isString()){
				lng = json_field[field_name].asString();
			}else{
				return RT_ERROR_FIELD_FORMAT;
			}
			break;
		case FIELD_LAT:
			if(json_field[field_name].isString()){
				lat = json_field[field_name].asString();
			}else{
				return RT_ERROR_FIELD_FORMAT;
			}
			break;
		case FIELD_LNG_ARRAY:
			if(json_field[field_name].isArray()){
				Json::Value lngs = json_field[field_name];
				for (uint32_t lng_idx = 0; lng_idx < lngs.size(); ++lng_idx) {
					if (lngs[lng_idx].isString()){
						lng_arr.push_back(lngs[lng_idx].asString());
					} else {
						log_error("longitude must be string");
						return RT_ERROR_FIELD_FORMAT;
					}
				}
			}else{
				log_error("FIELD_LNG_ARRAY must be array");
				return RT_ERROR_FIELD_FORMAT;
			}
			break;
		case FIELD_LAT_ARRAY:
			if(json_field[field_name].isArray()){
				Json::Value lats = json_field[field_name];
				for (uint32_t lat_idx = 0; lat_idx < lats.size(); ++lat_idx) {
					if (lats[lat_idx].isString()){
						lat_arr.push_back(lats[lat_idx].asString());
					} else {
						log_error("latitude must be string");
						return RT_ERROR_FIELD_FORMAT;
					}
				}
			}else{
				log_error("FIELD_LAT_ARRAY must be array");
				return RT_ERROR_FIELD_FORMAT;
			}
			break;
		case FIELD_WKT:
			if(json_field[field_name].isString()){
				string str = json_field[field_name].asString();
				str = delPrefix(str);
				vector<string> str_vec = splitEx(str, ",");
				for(uint32_t str_vec_idx = 0; str_vec_idx < str_vec.size(); str_vec_idx++){
					string wkt_str = trim(str_vec[str_vec_idx]);
					vector<string> wkt_vec = splitEx(wkt_str, " ");
					if(wkt_vec.size() == 2){
						lng_arr.push_back(wkt_vec[0]);
						lat_arr.push_back(wkt_vec[1]);
					}
				}
			} else {
				log_error("FIELD_WKT must be string");
				return RT_ERROR_FIELD_FORMAT;
			}
			break;
		default:
			break;
	}
	return 0;
}

int AddReqProc::do_insert_index(UserTableContent& content_fields){
	int ret = 0;
	Json::Value::Members member = json_field.getMemberNames();
	Json::Value::Members::iterator iter = member.begin();
	for(; iter != member.end(); ++iter)
	{
		string field_name = *iter;
		struct table_info *tbinfo = NULL;
		tbinfo = SplitManager::Instance()->get_table_info(app_id, field_name);
		if(tbinfo == NULL){
			continue;
		}
		if(tbinfo->snapshot_tag == 1){ //snapshot
			if(tbinfo->field_type == 1 && json_field[field_name].isInt()){
				snapshot_content[field_name] = json_field[field_name].asInt();
			}else if(tbinfo->field_type > 1 && json_field[field_name].isString()){
				snapshot_content[field_name] = json_field[field_name].asString();
			}else if(tbinfo->field_type > 1 && json_field[field_name].isDouble()){
				snapshot_content[field_name] = json_field[field_name].asDouble();
			}else if(tbinfo->field_type > 1 && json_field[field_name].isInt64()){
				snapshot_content[field_name] = json_field[field_name].asInt64();
			}else if(tbinfo->field_type > 1 && json_field[field_name].isArray()){
				snapshot_content[field_name] = json_field[field_name];
			}
		}
	}
	for(iter = member.begin(); iter != member.end(); ++iter)
	{
		string field_name = *iter;
		struct table_info *tbinfo = NULL;
		tbinfo = SplitManager::Instance()->get_table_info(app_id, field_name);
		if(tbinfo == NULL){
			continue;
		}
		if(tbinfo->index_tag == 1){
			ret = deal_index_tag(tbinfo, field_name);
			if(0 != ret){
				log_error("deal index tag process error, ret: %d", ret);
				roll_back();
				return ret;
			}
		}
	}
	if(lng.length() != 0 && lat.length() != 0){
		struct table_info *tbinfo = NULL;
		tbinfo = SplitManager::Instance()->get_table_info(app_id, "gis");
		if(tbinfo == NULL){
			roll_back();
			return RT_NO_GIS_DEFINE;
		}

		string gisid = encode(atof(lat.c_str()), atof(lng.c_str()), 6);
		log_debug("gis code = %s",gisid.c_str());
		int ret;
		uint64_t id = 0;
		struct item it;
		it.doc_id = doc_id;
		it.freq = 0;
		Json::FastWriter gis_writer;
		it.extend = gis_writer.write(snapshot_content);
		string key = gen_dtc_key_string(app_id, "00", gisid);
		ret = g_IndexInstance.insert_index_dtc(key, it, tbinfo->field_value, doc_version, docid_index_map);
		if(ret != 0){
		 	roll_back();
			return RT_ERROR_INSERT_INDEX_DTC;
		}
		log_debug("id = %llu,doc_vesion = %d,docid = %s\n",(long long unsigned int)id,doc_version,it.doc_id.c_str());
	}
	log_debug("lng_arr size: %d, lat_arr size: %d", (int)lng_arr.size(), (int)lat_arr.size());
	if(lng_arr.size() > 0 && lat_arr.size() > 0){
		if(lng_arr.size() != lat_arr.size()){
			log_error("lng_arr size not equal with lat_arr size");
			return RT_ERROR_FIELD_FORMAT;
		}
		set<string> gis_set;
		for(uint32_t arr_idx = 0; arr_idx < lng_arr.size(); arr_idx++){
			string tmp_lng = lng_arr[arr_idx];
			string tmp_lat = lat_arr[arr_idx];
			struct table_info *tbinfo = NULL;
			tbinfo = SplitManager::Instance()->get_table_info(app_id, "gis");
			if(tbinfo == NULL){
				roll_back();
				log_error("gis field not defined");
				return RT_NO_GIS_DEFINE;
			}
			string gisid = encode(atof(tmp_lat.c_str()), atof(tmp_lng.c_str()), 6);
			if(gis_set.find(gisid) != gis_set.end()){
				continue;
			}
			gis_set.insert(gisid);
			struct item it;
			it.doc_id = doc_id;
			it.freq = 0;
			Json::FastWriter gis_writer;
			it.extend = gis_writer.write(snapshot_content);
			string key = gen_dtc_key_string(app_id, "00", gisid);
			int ret = g_IndexInstance.insert_index_dtc(key, it, tbinfo->field_value, doc_version, docid_index_map);
			if(ret != 0){
				roll_back();
				return RT_ERROR_INSERT_INDEX_DTC;
			}
			log_debug("gis code = %s,doc_vesion = %d,docid = %s\n",gisid.c_str(),doc_version,it.doc_id.c_str());
		}
	}

	vector<string> union_key_vec;
	SplitManager::Instance()->getUnionKeyField(app_id, union_key_vec);
	vector<string>::iterator union_key_iter = union_key_vec.begin();
	for(; union_key_iter != union_key_vec.end(); union_key_iter++){
		string union_key = *union_key_iter;
		vector<int> union_field_vec = splitInt(union_key, ",");
		vector<int>::iterator union_field_iter = union_field_vec.begin();
		vector<vector<string> > keys_vvec;
		for(; union_field_iter != union_field_vec.end(); union_field_iter++){
			int union_field_value = *union_field_iter;
			if(union_field_value >= (int)docid_index_map.size()){
				log_error("appid[%d] field[%d] is invalid", app_id, *union_field_iter);
				break;
			}
			vector<string> key_vec;
			if(!docid_index_map[union_field_value].isArray()){
				log_debug("doc_id[%s] union_field_value[%d] has no keys", doc_id.c_str(), union_field_value);
				break;
			}
			for (int key_index = 0; key_index < (int)docid_index_map[union_field_value].size(); key_index++){
				if(docid_index_map[union_field_value][key_index].isString()){
					string union_index_key = docid_index_map[union_field_value][key_index].asString();
					if(union_index_key.size() > 9){  // 倒排key的格式为：10061#00#折扣，这里只取第二个#后面的内容
						key_vec.push_back(union_index_key.substr(9));
					}
				}
			}
			keys_vvec.push_back(key_vec);
		}
		if(keys_vvec.size() != union_field_vec.size()){
			log_debug("keys_vvec.size not equal union_field_vec.size");
			break;
		}
		vector<string> union_keys = combination(keys_vvec);
		for(int m = 0 ; m < (int)union_keys.size(); m++){
			ret = g_IndexInstance.insert_union_index_dtc(union_keys[m], doc_id, app_id, doc_version);
			if(ret != 0){
				log_error("insert union key[%s] error", union_keys[m].c_str());
			}
		}
	}

	Json::FastWriter writer;
	content_fields.content = writer.write(snapshot_content);
	Json::FastWriter doc_index_writer;
	string doc_index_map_string = doc_index_writer.write(docid_index_map);
	if(doc_version != 1){//need update
		map<uint32_t, vector<string> > index_res;
		g_IndexInstance.GetIndexData(gen_dtc_key_string(content_fields.appid, "20", doc_id), doc_version - 1, index_res);
		map<uint32_t, vector<string> >::iterator map_iter = index_res.begin();
		for(; map_iter != index_res.end(); map_iter++){
			uint32_t field = map_iter->first;
			vector<string> words = map_iter->second;
			for(int i = 0; i < (int)words.size(); i++){
				DeleteTask::GetInstance().RegisterInfo(words[i], doc_id, doc_version - 1, field);
			}
		}

		int affected_rows = 0;
		ret = g_IndexInstance.update_sanpshot_dtc(content_fields, doc_version, trans_version, affected_rows);
		if(ret != 0 || affected_rows == 0){
			log_error("update_sanpshot_dtc error, roll back, ret: %d, affected_rows: %d.", ret, affected_rows);
			roll_back();
			return RT_ERROR_UPDATE_SNAPSHOT;
		}
		g_IndexInstance.update_docid_index_dtc(doc_index_map_string, doc_id, app_id, doc_version);
	}else{
		int affected_rows = 0;
		ret = g_IndexInstance.update_sanpshot_dtc(content_fields, doc_version, trans_version, affected_rows);
		if(ret != 0 || affected_rows == 0){
			log_error("update_sanpshot_dtc error, roll back, ret: %d, affected_rows: %d.", ret, affected_rows);
			roll_back();
			return RT_ERROR_UPDATE_SNAPSHOT;
		}
		g_IndexInstance.insert_docid_index_dtc(doc_index_map_string, doc_id, app_id, doc_version);
	}
	return 0;
}

int AddReqProc::roll_back(){
	// 删除hanpin_index
	for(int i = 0; i < (int)intelligent_keys.size(); i++){
		g_hanpinIndexInstance.delete_intelligent(intelligent_keys[i], doc_id, trans_version);
	}
	
	// 删除keyword_index
	if(docid_index_map.isArray()){
		for(int i = 0;i < (int)docid_index_map.size();i++){
			Json::Value info = docid_index_map[i];
			if(info.isArray()){
				for(int j = 0;j < (int)info.size();j++){
					if(info[j].isString()){
						string key = info[j].asString();
						g_IndexInstance.delete_index(key, doc_id, trans_version, i);
					}
				}
			}
		}
	}
	// 如果trans_version=1，删除快照，否则更新快照的trans_version=trans_version-1
	Json::Value res;
	if(trans_version == 1){
		g_IndexInstance.delete_snapshot_dtc(doc_id, app_id, res);
	} else {
		g_IndexInstance.update_sanpshot_dtc(app_id, doc_id, trans_version);
	}
	return 0;
}