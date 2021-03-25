/*
 * =====================================================================================
 *
 *       Filename:  logical_operate.h
 *
 *    Description:  logical operate class definition.
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

#include "logical_operate.h"
#include "search_util.h"
#include "cachelist_unit.h"
#include "data_manager.h"
#include "json/reader.h"
#include "json/writer.h"
#include "index_tbl_op.h"
#include "index_sync/sync_index_timer.h"
#include "index_sync/sequence_search_index.h"
#include "stem.h"
#include <sstream>
#include <iomanip>
using namespace std;

extern SyncIndexTimer *globalSyncIndexTimer;
extern CCacheListUnit *indexcachelist;

LogicalOperate::LogicalOperate(uint32_t a, uint32_t s, uint32_t h, uint32_t c):m_appid(a), m_sort_type(s), m_has_gis(h), m_cache_switch(c)
{

}

LogicalOperate::~LogicalOperate(){

}

void LogicalOperate::SetFunc(logical_func func){
	m_func = func;
}

int LogicalOperate::Process(const vector<vector<FieldInfo> >& keys, vector<IndexInfo>& vecs, set<string>& highlightWord, map<string, vec> &ves, map<string, uint32_t> &key_in_doc){
	for (size_t index = 0; index < keys.size(); index++)
	{
		vector<IndexInfo> doc_id_vec;
		vector<FieldInfo> fieldInfos = keys[index];
		vector<FieldInfo>::iterator it;
		for (it = fieldInfos.begin(); it != fieldInfos.end(); it++) {
			vector<IndexInfo> doc_info;
			if ((*it).segment_tag == 3) {
				int ret = GetDocByShiftWord(*it, doc_info, m_appid, highlightWord);
				if (ret != 0) {
					doc_id_vec.clear();
					return -RT_GET_DOC_ERR;
				}
				sort(doc_info.begin(), doc_info.end());
			} else if ((*it).segment_tag == 4) {
				int ret = GetDocByShiftEnWord(*it, doc_info, m_appid, highlightWord);
				if (ret != 0) {
					doc_id_vec.clear();
					return -RT_GET_DOC_ERR;
				}
				sort(doc_info.begin(), doc_info.end());
			} else if ((*it).segment_tag == 5 && (*it).word == "") { // 范围查询
				stringstream ss;
				ss << m_appid;
				InvertIndexEntry startEntry(ss.str(), (*it).field, (double)(*it).start);
				InvertIndexEntry endEntry(ss.str(), (*it).field, (double)(*it).end);
				std::vector<InvertIndexEntry> resultEntry;
				globalSyncIndexTimer->GetSearchIndex()->GetRangeIndex((*it).range_type, startEntry, endEntry, resultEntry);
				std::vector<InvertIndexEntry>::iterator iter = resultEntry.begin();
				for (; iter != resultEntry.end(); iter ++) {
					IndexInfo info;
					info.doc_id = (*iter)._InvertIndexDocId;
					info.doc_version = (*iter)._InvertIndexDocVersion;
					doc_info.push_back(info);
				}
				log_debug("appid: %s, field: %d, count: %d", startEntry._InvertIndexAppid.c_str(), (*it).field, (int)resultEntry.size());
			} else {
				int ret = GetDocIdSetByWord(*it, doc_info);
				if (ret != 0){
					return -RT_GET_DOC_ERR;
				}
				if (doc_info.size() == 0)
					continue;
				if (!m_has_gis || !isAllNumber((*it).word))
					highlightWord.insert((*it).word);
				if(!m_has_gis && (m_sort_type == SORT_RELEVANCE || m_sort_type == SORT_TIMESTAMP)){
					CalculateByWord(*it, doc_info, ves, key_in_doc);
				}
			} 
			doc_id_vec = vec_union(doc_id_vec, doc_info);
		}
		if(index == 0){ // 第一个直接赋值给vecs，后续的依次与前面的进行逻辑运算
			vecs.assign(doc_id_vec.begin(), doc_id_vec.end());
		} else {
			vecs = m_func(vecs, doc_id_vec);
		}
	}
	return 0;
}

int LogicalOperate::ProcessTerminal(const vector<vector<FieldInfo> >& and_keys, const TerminalQryCond& query_cond, vector<TerminalRes>& vecs){
	if(and_keys.size() != 1){
		return 0;
	}
	vector<FieldInfo> field_vec = and_keys[0];
	if(field_vec.size() != 1){
		return 0;
	}
	FieldInfo field_info = field_vec[0];
	if(field_info.segment_tag != SEGMENT_RANGE){
		return 0;
	}

	stringstream ss;
	ss << m_appid;
	InvertIndexEntry beginEntry(ss.str(), field_info.field, (double)field_info.start);
	InvertIndexEntry endEntry(ss.str(), field_info.field, (double)field_info.end);
	std::vector<InvertIndexEntry> resultEntry;
	globalSyncIndexTimer->GetSearchIndex()->GetRangeIndexInTerminal(field_info.range_type, beginEntry, endEntry, query_cond, resultEntry);
	std::vector<InvertIndexEntry>::iterator iter = resultEntry.begin();
	for (; iter != resultEntry.end(); iter ++) {
		TerminalRes info;
		info.doc_id = (*iter)._InvertIndexDocId;
		info.score = (*iter)._InvertIndexKey;
		vecs.push_back(info);
	}
	return 0;
}

int LogicalOperate::ProcessComplete(const vector<FieldInfo>& complete_keys, vector<IndexInfo>& complete_vecs, vector<string>& word_vec, map<string, vec> &ves, map<string, uint32_t> &key_in_doc){
	vector<FieldInfo>::const_iterator iter;
	for (iter = complete_keys.begin(); iter != complete_keys.end(); iter++) {
		vector<IndexInfo> doc_info;
		int ret = GetDocIdSetByWord(*iter, doc_info);
		if (ret != 0) {
			return -RT_GET_DOC_ERR;
		}
		
		word_vec.push_back((*iter).word);

		if(m_sort_type == SORT_RELEVANCE || m_sort_type == SORT_TIMESTAMP){
			CalculateByWord(*iter, doc_info, ves, key_in_doc);
		}

		if(iter == complete_keys.begin()){
			complete_vecs.assign(doc_info.begin(), doc_info.end());
		} else {
			complete_vecs = vec_intersection(complete_vecs, doc_info);
		}
	}
	return 0;
}

void LogicalOperate::CalculateByWord(FieldInfo fieldInfo, const vector<IndexInfo> &doc_info, map<string, vec> &ves, map<string, uint32_t> &key_in_doc) {
	string doc_id;
	uint32_t word_freq = 0;
	uint32_t field = 0;
	uint32_t created_time;
	string pos_str = "";
	for (size_t i = 0; i < doc_info.size(); i++) {
		doc_id = doc_info[i].doc_id;
		word_freq = doc_info[i].word_freq;
		field = doc_info[i].field;
		created_time = doc_info[i].created_time;
		pos_str = doc_info[i].pos;
		vector<int> pos_vec;
		if (pos_str != "" && pos_str.size() > 2) {
			pos_str = pos_str.substr(1, pos_str.size() - 2);
			pos_vec = splitInt(pos_str, ",");
		}
		KeyInfo info;
		info.word_freq = word_freq;
		info.field = field;
		info.word = fieldInfo.word;
		info.created_time = created_time;
		info.pos_vec = pos_vec;
		ves[doc_id].push_back(info);
	}
	key_in_doc[fieldInfo.word] = doc_info.size();
}


bool LogicalOperate::GetDocIndexCache(string word, uint32_t field, vector<IndexInfo> &doc_info) {
	log_debug("get doc index start");
	bool res = false;
	uint8_t value[MAX_VALUE_LEN] = { 0 };
	unsigned vsize = 0;
	string output = "";
	string indexCache = word + "|" + ToString(field);
	if (m_cache_switch == 1 && indexcachelist->in_list(indexCache.c_str(), indexCache.size(), value, vsize))
	{
		log_debug("hit index cache.");
		value[vsize] = '\0';
		output = (char *)value;
		res = true;
	}

	if (res) {
		Json::Value packet;
		Json::Reader r(Json::Features::strictMode());
		int ret;
		ret = r.parse(output.c_str(), output.c_str() + output.size(), packet);
		if (0 == ret)
		{
			log_error("the err json string is : %s, errmsg : %s", output.c_str(), r.getFormattedErrorMessages().c_str());
			res = false;
			return res;
		}

		for (uint32_t i = 0; i < packet.size(); ++i) {
			IndexInfo info;
			Json::Value& index_cache = packet[i];
			if (index_cache.isMember("appid")   && index_cache["appid"].isUInt()   && 
				index_cache.isMember("id")      && index_cache["id"].isString()    &&
				index_cache.isMember("version") && index_cache["version"].isUInt() && 
				index_cache.isMember("field")   && index_cache["field"].isUInt()   && 
				index_cache.isMember("freq")    && index_cache["freq"].isUInt()    && 
				index_cache.isMember("time")    && index_cache["time"].isUInt()    && 
				index_cache.isMember("pos")     && index_cache["pos"].isString())
			{
				info.appid = index_cache["appid"].asUInt();
				info.doc_id = index_cache["id"].asString();
				info.doc_version = index_cache["version"].asUInt();
				info.field = index_cache["field"].asUInt();
				info.word_freq = index_cache["freq"].asUInt();
				info.created_time = index_cache["time"].asUInt();
				info.pos = index_cache["pos"].asString();
				doc_info.push_back(info);
			}
			else {
				log_error("parse index_cache error, no appid");
				doc_info.clear();
				res = false;
				break;
			}
		}
	}
	return res;
}

void LogicalOperate::SetDocIndexCache(const vector<IndexInfo> &doc_info, string& indexJsonStr) {
	Json::Value indexJson;
	Json::FastWriter writer;
	for (size_t i = 0; i < doc_info.size(); i++) {
		Json::Value json_tmp;
		json_tmp["appid"] = doc_info[i].appid;
		json_tmp["id"] = doc_info[i].doc_id;
		json_tmp["version"] = doc_info[i].doc_version;
		json_tmp["field"] = doc_info[i].field;
		json_tmp["freq"] = doc_info[i].word_freq;
		json_tmp["time"] = doc_info[i].created_time;
		json_tmp["pos"] = doc_info[i].pos;
		indexJson.append(json_tmp);
	}
	indexJsonStr = writer.write(indexJson);
}


int LogicalOperate::GetDocIdSetByWord(FieldInfo fieldInfo, vector<IndexInfo> &doc_info) {
	bool bRet = false;
	if (DataManager::Instance()->IsSensitiveWord(fieldInfo.word)) {
		log_debug("%s is a sensitive word.", fieldInfo.word.c_str());
		return 0;
	}

	stringstream ss_key;
	ss_key << m_appid;
	ss_key << "#00#";
	if(fieldInfo.segment_tag == 5){
		stringstream ss;
		ss << setw(20) << setfill('0') << fieldInfo.word;
		ss_key << ss.str();
	}
	else if (fieldInfo.field_type == FIELD_INT || fieldInfo.field_type == FIELD_DOUBLE || fieldInfo.field_type == FIELD_LONG) {
		ss_key << fieldInfo.word;
	}
	else if (fieldInfo.field_type == FIELD_IP) {
		uint32_t word_id = GetIpNum(fieldInfo.word);
		if (word_id == 0)
			return 0;
		ss_key << word_id;
	}
	else if (fieldInfo.word.find("_") != string::npos) { // 联合索引
		ss_key << fieldInfo.word;
	}
	else {
		string word_new = stem(fieldInfo.word);
		ss_key << word_new;
	}

	log_debug("appid [%u], key[%s]", m_appid, ss_key.str().c_str());
	if (m_has_gis && GetDocIndexCache(ss_key.str(), fieldInfo.field, doc_info)) {
		return 0;
	}

	bRet = g_IndexInstance.GetDocInfo(m_appid, ss_key.str(), fieldInfo.field, doc_info);
	if (false == bRet) {
		log_error("GetDocInfo error.");
		return -RT_DTC_ERR;
	}

	if (m_cache_switch == 1 && m_has_gis == 1 && doc_info.size() > 0 && doc_info.size() <= 1000) {
		string index_str;
		SetDocIndexCache(doc_info, index_str);
		if (index_str != "" && index_str.size() < MAX_VALUE_LEN) {
			string indexCache = ss_key.str() + "|" + ToString(fieldInfo.field);
			unsigned data_size = indexCache.size();
			int ret = indexcachelist->add_list(indexCache.c_str(), index_str.c_str(), data_size, index_str.size());
			if (ret != 0) {
				log_error("add to index_cache_list error, ret: %d.", ret);
			}
			else {
				log_debug("add to index_cache_list: %s.", indexCache.c_str());
			}
		}
	}
	return 0;
}