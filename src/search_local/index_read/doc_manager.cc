/*
 * =====================================================================================
 *
 *       Filename:  doc_manager.h
 *
 *    Description:  doc manager class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "component.h"
#include "doc_manager.h"
#include "log.h"
#include "search_util.h"
#include "db_manager.h"
#include <math.h>
#include <sstream>

DocManager::DocManager(Component *c): component(c){
}

DocManager::~DocManager(){

}

bool DocManager::CheckDocByExtraFilterKey(string doc_id){
	vector<ExtraFilterKey> extra_filter_vec = component->ExtraFilterKeys();
	vector<ExtraFilterKey> extra_filter_and_vec = component->ExtraFilterAndKeys();
	vector<ExtraFilterKey> extra_filter_invert_vec = component->ExtraFilterInvertKeys();
	if(extra_filter_vec.size() == 0 && extra_filter_and_vec.size() == 0 && extra_filter_invert_vec.size() == 0){
		return true;
	} else {
		vector<string> fields;
		for(int i = 0; i < (int)extra_filter_vec.size(); i++){
			fields.push_back(extra_filter_vec[i].field_name);
		}
		for(int i = 0; i < (int)extra_filter_and_vec.size(); i++){
			fields.push_back(extra_filter_and_vec[i].field_name);
		}
		for(int i = 0; i < (int)extra_filter_invert_vec.size(); i++){
			fields.push_back(extra_filter_invert_vec[i].field_name);
		}
		Json::Value value;
		uint32_t doc_version = 0;
		if(valid_version.find(doc_id) != valid_version.end()){
			doc_version = valid_version[doc_id];
		}
		if(doc_content_map.find(doc_id) != doc_content_map.end()){
			string extend = doc_content_map[doc_id];
			Json::Reader r(Json::Features::strictMode());
			int ret2 = r.parse(extend.c_str(), extend.c_str() + extend.length(), value);
			if (0 == ret2){
				log_error("the err json is %s, errmsg : %s", extend.c_str(), r.getFormattedErrorMessages().c_str());
				return false;
			}
		} else {
			bool bRet = g_IndexInstance.GetContentByField(component->Appid(), doc_id, doc_version, fields, value);
			if(bRet == false){
				log_error("get field content error, appid[%d] doc_id[%s].", component->Appid(), doc_id.c_str());
				return true;
			}
		}

		bool key_or_valid = false;
		CheckIfKeyValid(extra_filter_vec, value, true, key_or_valid);
		if(extra_filter_vec.size() > 0 && key_or_valid == false){
			return false;
		}

		bool key_and_valid = true;
		CheckIfKeyValid(extra_filter_and_vec, value, false, key_and_valid);
		if(key_and_valid == false){
			return false;
		}

		bool key_invert_valid = false;
		CheckIfKeyValid(extra_filter_invert_vec, value, true, key_invert_valid);
		if(key_invert_valid == true){
			return false;
		}

		return true;
	}
}

void DocManager::CheckIfKeyValid(const vector<ExtraFilterKey>& extra_filter_vec, const Json::Value &value, bool flag, bool &key_valid){
	for(int i = 0; i < (int)extra_filter_vec.size(); i++){
		bool the_same = false;
		string field_name = extra_filter_vec[i].field_name;
		if(extra_filter_vec[i].field_type == FIELD_INT){
			string query = extra_filter_vec[i].field_value;
			vector<string> query_vec = splitEx(query, "|");
			if(query_vec.size() > 1){
				for(int i = 0 ; i < (int)query_vec.size(); i++){
					if(atoi(query_vec[i].c_str()) == value[field_name.c_str()].asInt()){
						the_same = true;
						break;
					}
				}
			} else {
				the_same = (atoi(extra_filter_vec[i].field_value.c_str()) == value[field_name.c_str()].asInt());
			}
		} else if(extra_filter_vec[i].field_type == FIELD_DOUBLE){
			double d_field_value = atof(extra_filter_vec[i].field_value.c_str());
			double d_extend = value[field_name.c_str()].asDouble();
			the_same = (fabs(d_field_value - d_extend) < 1e-15);
		} else if(extra_filter_vec[i].field_type == FIELD_STRING){
			string snapshot = value[field_name.c_str()].asString();
			string query = extra_filter_vec[i].field_value;
			set<string> snapshot_set = splitStr(snapshot, "|");
			vector<string> query_vec = splitEx(query, "|");
			for(int i = 0 ; i < (int)query_vec.size(); i++){
				if(snapshot_set.find(query_vec[i]) != snapshot_set.end()){
					the_same = true;
					break;
				}
			}
		}
		if(the_same == flag){
			key_valid = flag;
			break;
		}
	}
}

bool DocManager::GetDocContent(uint32_t m_has_gis, vector<IndexInfo> &doc_id_ver_vec, set<string> &valid_docs, hash_double_map &distances){
	if (!m_has_gis && component->SnapshotSwitch() == 1 && doc_id_ver_vec.size() <= 1000) {
		bool need_version = false;
		if(component->Fields().size() > 0){
			need_version = true;
		}
		bool bRet = g_IndexInstance.DocValid(component->Appid(), doc_id_ver_vec, valid_docs, need_version, valid_version, doc_content_map);
		if (false == bRet) {
			log_error("GetDocInfo by snapshot error.");
			return false;
		}
	} else {
		for(size_t i = 0 ; i < doc_id_ver_vec.size(); i++){
			if(!m_has_gis){
				valid_docs.insert(doc_id_ver_vec[i].doc_id);
			}
			if(doc_id_ver_vec[i].extend != ""){
				doc_content_map.insert(make_pair(doc_id_ver_vec[i].doc_id, doc_id_ver_vec[i].extend));
			}
		}
	}

	log_debug("doc_id_ver_vec size: %d", (int)doc_id_ver_vec.size());
	if (m_has_gis) {
		if(doc_content_map.size() == 0){
			g_IndexInstance.GetDocContent(component->Appid(), doc_id_ver_vec, doc_content_map);
		}
		GetGisDistance(component->Appid(), component->Latitude(), component->Longitude(), distances, doc_content_map);
	}
	return true;
}

bool DocManager::AppendFieldsToRes(Json::Value &response, vector<string> &m_fields){
	for(int i = 0; i < (int)response["result"].size(); i++){
		Json::Value doc_info = response["result"][i];

		string doc_id = doc_info["doc_id"].asString();
		if(doc_content_map.find(doc_id) != doc_content_map.end()){
			string extend = doc_content_map[doc_id];
			Json::Reader r(Json::Features::strictMode());
			Json::Value recv_packet;
			int ret2 = r.parse(extend.c_str(), extend.c_str() + extend.length(), recv_packet);
			if (0 == ret2){
				log_error("parse json error [%s], errmsg : %s", extend.c_str(), r.getFormattedErrorMessages().c_str());
				return false;
			}

			Json::Value &value = response["result"][i];
			for(int i = 0; i < (int)m_fields.size(); i++){
				if (recv_packet.isMember(m_fields[i].c_str()))
				{
					if(recv_packet[m_fields[i].c_str()].isUInt()){
						value[m_fields[i].c_str()] = recv_packet[m_fields[i].c_str()].asUInt();
					} else if(recv_packet[m_fields[i].c_str()].isString()){
						value[m_fields[i].c_str()] = recv_packet[m_fields[i].c_str()].asString();
					} else if(recv_packet[m_fields[i].c_str()].isDouble()){
						value[m_fields[i].c_str()] = recv_packet[m_fields[i].c_str()].asDouble();
					} else {
						log_error("field[%s] data type error.", m_fields[i].c_str());
					}
				} else {
					log_error("appid[%u] field[%s] invalid.", component->Appid(), m_fields[i].c_str());
				}
			}
		} else {
			uint32_t doc_version = 0;
			if(valid_version.find(doc_info["doc_id"].asString()) != valid_version.end()){
				doc_version = valid_version[doc_info["doc_id"].asString()];
			}
			bool bRet = g_IndexInstance.GetContentByField(component->Appid(), doc_info["doc_id"].asString(), doc_version, m_fields, response["result"][i]);
			if(bRet == false){
				log_error("get field content error.");
				return false;
			}
		}
	}
	return true;
}

bool DocManager::GetScoreMap(string doc_id, uint32_t m_sort_type, string m_sort_field, FIELDTYPE &m_sort_field_type, uint32_t appid){
	if(doc_content_map.find(doc_id) != doc_content_map.end()){
		uint32_t field_type = 0;
		bool bRet = DBManager::Instance()->GetFieldType(appid, m_sort_field, field_type);
		if(false == bRet){
			log_error("appid[%d] field[%s] not find.", appid, m_sort_field.c_str());
			return false;
		}
		string extend = doc_content_map[doc_id];

		if(field_type == FIELD_INT){
			int len = strlen(m_sort_field.c_str()) + strlen("\":");
			size_t pos1 = extend.find(m_sort_field);
			size_t pos2 = extend.find_first_of(",", pos1);
			if(pos2 == string::npos){
				pos2 = extend.find_first_of("}", pos1);
			}
			if(pos1 != string::npos && pos2 != string::npos){
				string field_str = extend.substr(pos1+len, pos2-pos1-len);
				int field_int;
				istringstream iss(field_str);
				iss >> field_int;
				m_sort_field_type = FIELDTYPE_INT;
				score_int_map.insert(make_pair(doc_id, field_int));
			} else {
				m_sort_field_type = FIELDTYPE_INT;
				score_int_map.insert(make_pair(doc_id, 0));
			}
		} else {
			Json::Reader r(Json::Features::strictMode());
			Json::Value recv_packet;
			int ret2 = r.parse(extend.c_str(), extend.c_str() + extend.length(), recv_packet);
			if (0 == ret2){
				log_error("the err json is %s, errmsg : %s", extend.c_str(), r.getFormattedErrorMessages().c_str());
				return false;
			}

			if(recv_packet.isMember(m_sort_field.c_str()))
			{
				if(recv_packet[m_sort_field.c_str()].isUInt()){
					m_sort_field_type = FIELDTYPE_INT;
					score_int_map.insert(make_pair(doc_id, recv_packet[m_sort_field.c_str()].asUInt()));
				} else if(recv_packet[m_sort_field.c_str()].isString()){
					m_sort_field_type = FIELDTYPE_STRING;
					score_str_map.insert(make_pair(doc_id, recv_packet[m_sort_field.c_str()].asString()));
				} else if(recv_packet[m_sort_field.c_str()].isDouble()){
					m_sort_field_type = FIELDTYPE_DOUBLE;
					score_double_map.insert(make_pair(doc_id, recv_packet[m_sort_field.c_str()].asDouble()));
				} else {
					log_error("sort_field[%s] data type error.", m_sort_field.c_str());
					return false;
				}
			} else {
				log_error("appid[%u] sort_field[%s] invalid.", component->Appid(), m_sort_field.c_str());
				return false;
			}
		}
	} else {
		ScoreInfo score_info;
		bool bRet = g_IndexInstance.GetScoreByField(component->Appid(), doc_id, m_sort_field, m_sort_type, score_info);
		if(bRet == false){
			log_error("get score by field error.");
			return false;
		}
		m_sort_field_type = score_info.type;
		if(score_info.type == FIELDTYPE_INT){
			score_int_map.insert(make_pair(doc_id, score_info.i));
		} else if(score_info.type == FIELDTYPE_STRING){
			score_str_map.insert(make_pair(doc_id, score_info.str));
		} else if(score_info.type == FIELDTYPE_DOUBLE){
			score_double_map.insert(make_pair(doc_id, score_info.d));
		}
	}
	return true;
}

map<string, string>& DocManager::ScoreStrMap(){
	return score_str_map;
}

map<string, int>& DocManager::ScoreIntMap(){
	return score_int_map;
}

map<string, double>& DocManager::ScoreDoubleMap(){
	return score_double_map;
}

map<string, uint32_t>& DocManager::ValidVersion(){
	return valid_version;
}