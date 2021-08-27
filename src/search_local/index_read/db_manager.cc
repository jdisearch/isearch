/*
 * =====================================================================================
 *
 *       Filename:  db_manager.h
 *
 *    Description:  db manager class definition.
 *
 *        Version:  1.0
 *        Created:  13/12/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include <string>
#include <sstream>
#include <sys/time.h>
#include <fstream>
#include "db_manager.h"
#include "utf8_str.h"
using namespace std;

extern pthread_mutex_t mutex;

DBManager::DBManager() {
	aTable = new AppFieldTable();
}

DBManager::~DBManager() {
	if (aTable){
		delete aTable;
	}
	aTable = NULL;
}

uint32_t DBManager::ToInt(const char* str) {
	if (NULL != str)
		return atoi(str);
	else
		return 0;
}

string DBManager::ToString(const char* str) {
	if (NULL != str)
		return str;
	else
		return "";
}

bool DBManager::Init(const SGlobalConfig &global_cfg) {
	ifstream app_filed_infile;
	app_filed_infile.open(global_cfg.sAppFieldPath.c_str());
	if (app_filed_infile.is_open() == false) {
		log_error("open file error: %s.", global_cfg.sPhoneticPath.c_str());
		return false;
	}
	string str;
	while (getline(app_filed_infile, str))
	{
		vector<string> str_vec = splitEx(str, "\t");
		if (str_vec.size() >= 11) {
			AppFieldInfo info;
			int32_t row_index = 1;
			uint32_t appid = ToInt(str_vec[row_index++].c_str());
			string field_name = ToString(str_vec[row_index++].c_str());
			info.is_primary_key = ToInt(str_vec[row_index++].c_str());
			info.field_type = ToInt(str_vec[row_index++].c_str());
			info.index_tag = ToInt(str_vec[row_index++].c_str());
			row_index++;
			info.segment_tag = ToInt(str_vec[row_index++].c_str());
			info.field_value = ToInt(str_vec[row_index++].c_str());
			row_index++;
			info.segment_feature = ToInt(str_vec[row_index++].c_str());
			if (str_vec.size() >= 12){
				info.index_info = ToString(str_vec[row_index++].c_str());
				log_debug("union index[%s]", info.index_info.c_str());
			}
			aTable->appFieldInfo[appid][field_name] = info;
		}
		else {
			log_error("data format error: %s.", global_cfg.sPhoneticPath.c_str());
			app_filed_infile.close();
			return false;
		}
	}
	app_filed_infile.close();
	return true;
}

void DBManager::ReplaceAppFieldInfo(string appFieldFile){
	ifstream app_filed_infile;
	app_filed_infile.open(appFieldFile.c_str());
	if (app_filed_infile.is_open() == false) {
		log_error("open file error: %s.", appFieldFile.c_str());
		return;
	}
	AppFieldTable *table = new AppFieldTable();
	string str;
	while (getline(app_filed_infile, str))
	{
		vector<string> str_vec = splitEx(str, "\t");
		if (str_vec.size() >= 11) {
			AppFieldInfo info;
			int32_t row_index = 1;
			uint32_t appid = ToInt(str_vec[row_index++].c_str());
			string field_name = ToString(str_vec[row_index++].c_str());
			info.is_primary_key = ToInt(str_vec[row_index++].c_str());
			info.field_type = ToInt(str_vec[row_index++].c_str());
			info.index_tag = ToInt(str_vec[row_index++].c_str());
			row_index++;
			info.segment_tag = ToInt(str_vec[row_index++].c_str());
			info.field_value = ToInt(str_vec[row_index++].c_str());
			row_index++;
			info.segment_feature = ToInt(str_vec[row_index++].c_str());
			if (str_vec.size() >= 12){
				info.index_info = ToString(str_vec[row_index++].c_str());
				log_debug("union index[%s]", info.index_info.c_str());
			}
			table->appFieldInfo[appid][field_name] = info;
		}
		else {
			log_error("data format error: %s.", appFieldFile.c_str());
			app_filed_infile.close();
			delete table;
			return;
		}
	}
	app_filed_infile.close();
	pthread_mutex_lock(&mutex);
	delete aTable;
	aTable = table;
	pthread_mutex_unlock(&mutex);
	log_debug("ReplaceAppFieldInfo success, appFieldInfo size: %d", (int)aTable->appFieldInfo.size());
	return;
}

uint32_t DBManager::GetWordField(uint32_t& segment_tag, uint32_t appid, string field_name, FieldInfo &fieldInfo)
{
	uint32_t field_value = 0;
	if (aTable->appFieldInfo.find(appid) != aTable->appFieldInfo.end()) {
		map<string, AppFieldInfo>& field_map = aTable->appFieldInfo[appid];
		if (field_map.find(field_name) != field_map.end()) {
			AppFieldInfo& info = field_map[field_name];
			segment_tag = info.segment_tag;
			fieldInfo.field = info.field_value;
			fieldInfo.field_type = info.field_type;
			fieldInfo.segment_tag = info.segment_tag;
			fieldInfo.segment_feature = info.segment_feature;
			fieldInfo.index_tag = info.index_tag;
			return info.field_value;
		}
	}

	return field_value;
}

bool DBManager::IsFieldSupportRange(uint32_t appid, uint32_t field_value) {
	std::map<uint32_t, std::map<std::string, AppFieldInfo> >::iterator iter = aTable->appFieldInfo.find(appid);
	if (iter != aTable->appFieldInfo.end()) {
		std::map<std::string, AppFieldInfo> map_field = iter->second;
		std::map<std::string, AppFieldInfo>::iterator field_iter = map_field.begin();
		for (; field_iter != map_field.end(); field_iter++) {
			AppFieldInfo field_info = field_iter->second;
			if (field_info.field_value == field_value) {
				if (field_info.segment_tag == 5) {
					return true;
				}
				break;
			}
		}
	}

	return false;
}

bool DBManager::GetFieldValue(uint32_t appid, string field, uint32_t &field_value){
	std::map<uint32_t, std::map<std::string, AppFieldInfo> >::iterator iter = aTable->appFieldInfo.find(appid);
	if (iter != aTable->appFieldInfo.end()) {
		std::map<std::string, AppFieldInfo> map_field = iter->second;
		if(map_field.find(field) != map_field.end()){
			AppFieldInfo appFieldInfo = map_field[field];
			field_value = appFieldInfo.field_value;
			return true;
		}
	}
	return false;
}

bool DBManager::GetFieldType(uint32_t appid, string field, uint32_t &field_type){
	std::map<uint32_t, std::map<std::string, AppFieldInfo> >::iterator iter = aTable->appFieldInfo.find(appid);
	if (iter != aTable->appFieldInfo.end()) {
		std::map<std::string, AppFieldInfo> map_field = iter->second;
		if(map_field.find(field) != map_field.end()){
			AppFieldInfo appFieldInfo = map_field[field];
			field_type = appFieldInfo.field_type;
			return true;
		}
	}
	return false;
}

bool DBManager::GetUnionKeyField(uint32_t appid, vector<string> & field_vec){
	if (aTable->appFieldInfo.find(appid) != aTable->appFieldInfo.end()) {
		map<string, AppFieldInfo> stMap = aTable->appFieldInfo[appid];
		map<string, AppFieldInfo>::iterator iter = stMap.begin();
		for (; iter != stMap.end(); iter++) {
			AppFieldInfo tInfo = iter->second;
			if(tInfo.field_type == FIELD_INDEX){
				field_vec.push_back(tInfo.index_info);
			}
		}
		return true;
	}
	return false;
}
