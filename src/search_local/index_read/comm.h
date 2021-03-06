/*
 * =====================================================================================
 *
 *       Filename:  comm.h
 *
 *    Description:  common enum definition.
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

#ifndef __COMM_H__
#define __COMM_H__
#include <string>
#include <stdint.h>
#include <vector>
#include <set>
#include <unordered_map>
#include <limits.h>
#include <map>
#include <cmath>
#include "test/timer_counter.h"
#include "comm_enum.h"

#define DOC_CNT 10000
#define MAX_DOCID_LENGTH 32

const double D_BM25_K = 1.65;
const double D_BM25_K1 = 1.2;
const double D_BM25_K2 = 200;

const uint32_t MAX_SEARCH_LEN = 60;
const uint32_t SINGLE_WORD_LEN = 18;
const uint32_t MAX_VALUE_LEN = 51200;

const double DOUBLE_EPS_3 = 1e-3;

typedef std::unordered_map<std::string, double> hash_double_map;
typedef std::unordered_map<std::string, std::string> hash_string_map;

enum RetCode{
	RT_PARSE_JSON_ERR = 10001,
	RT_INIT_ERR = 10002,
	RT_PARSE_CONF_ERR,
	RT_PRE_RUN_ERR,
	RT_BIND_ERR,
	RT_DB_ERR,
	RT_ATTACH_POLLER_ERR,
	RT_PARA_ERR,
	ER_SET_ADDRESS_ERR,
	RT_MEM_ERR,
	RT_DTC_ERR,
	RT_GET_SUGGEST_ERR,
	RT_OPEN_FILE_ERR,
	RT_GET_RELATE_ERR,
	RT_GET_HOT_ERR,
	RT_GET_DOC_ERR,
	RT_ADD_DICT_ERR,
	RT_GET_FIELD_ERROR,
	RT_PARSE_CONTENT_ERROR,
	RT_QUERY_TYPE_ERROR,
};

typedef enum CMD {
	SEARCH = 101,
	SUGGEST = 102,
	CLICK_INFO = 103,
	RELATE = 104,
	HOT_QUERY = 105,
}Cmd;

enum QUERYTYPE {
	TYPE_CONTENT = 1,
	TYPE_IMAGE = 2,
};

enum KEYLOCATE{
	LOCATE_ANY = 1,
	LOCATE_TITLE = 2,
	LOCATE_IMAGE = 3,
};

typedef enum FIELDTYPE {
    FIELDTYPE_INT = 1,
    FIELDTYPE_DOUBLE = 2,
    FIELDTYPE_STRING = 3,
}FIELDTYPE;

typedef enum RSPCODE {
	SUCCESS = 0,
	SYSTEM_ERR = 1,
	NETWORK_ERR = 2,
	PARAMETER_ERR = 3,
	SIGN_ERR = 4,
	DATA_ERR = 0x1001
}RSPCODE;

enum CHARACTERTYPE {
	CHINESE = 1,  // 汉字
	INITIAL = 2,  // 声母
	WHOLE_SPELL = 3,  // 全拼
};

enum DATATYPE {
	DATA_CHINESE = 1,  // 全中文
	DATA_HYBRID = 2,  // 中文+拼音
	DATA_PHONETIC = 3,  // 全拼音
	DATA_ENGLISH = 4,  // 英文
	DATA_OTHER = 5,  // 其它
};

enum RANGTYPE {
	RANGE_INIT = 0,
	RANGE_GELE = 1,  // 大于等于小于等于
	RANGE_GELT = 2,  // 大于等于小于
	RANGE_GTLE = 3,  // 大于小于等于
	RANGE_GTLT = 4,  // 大于小于
	RANGE_LT   = 5,  // 小于
	RANGE_GT   = 6,  // 大于
	RANGE_LE   = 7,  // 小于等于
	RANGE_GE   = 8,  // 大于等于
};

struct Content {
	uint32_t type;
	std::string str;
};

struct Info {
	std::string title;
	std::string content;
	std::string classify;
	std::string keywords;
	std::string url;
};

struct KeyInfo {
	uint32_t doc_count;
	uint32_t word_freq;
};
 
struct FieldInfo
{
	std::string word;
	uint32_t field;
	uint32_t field_type;
	uint32_t segment_tag;
	uint32_t segment_feature;
	double start;
	double end;
	uint32_t index_tag;
	RANGTYPE range_type;
	uint32_t query_type;
	FieldInfo() {
		field = 1;
		field_type = 0;
		segment_tag = 0;
		segment_feature = 0;
		start = 0;
		end = 0;
		range_type = RANGE_INIT;
		index_tag = 0;
		query_type = -1;
	}
};

struct AppFieldInfo {
	uint16_t is_primary_key;
	uint16_t field_type;
	uint16_t index_tag;
	uint16_t segment_tag;
	uint16_t field_value;
	uint16_t segment_feature;
	std::string index_info;
};

struct ScoreInfo
{
	double score;
	FIELDTYPE type;
	std::string str;
	int i;
	double d;
	ScoreInfo(){
		score = 0;
		type = FIELDTYPE_INT;
		i = 0;
		d = 0;
	}
};

struct CacheQueryInfo
{
	uint32_t appid;
	uint32_t sort_field;
	uint32_t sort_type;
	uint32_t page_index;
	uint32_t page_size;
	std::string last_score;
	std::string last_id;
	CacheQueryInfo(){
		appid = 0;
		sort_field = 0;
		sort_type = 0;
		page_index = 0;
		page_size = 0;
	}
};

enum KeyType
{
	ORKEY,
	ANDKEY,
	INVERTKEY,
	KEYTOTALNUM
};

struct ScoreDocIdNode{
	double d_score;
	std::string s_docid;

	ScoreDocIdNode(double score , const std::string& docid)
		: d_score(score)
		, s_docid(docid)
	{ }
	
	bool operator<(const ScoreDocIdNode& score_docid_node) const {
		if (fabs(d_score - score_docid_node.d_score) < DOUBLE_EPS_3){
			return s_docid.compare(score_docid_node.s_docid) < 0;
		}
		return (d_score + DOUBLE_EPS_3) < score_docid_node.d_score;
	}
};

struct IndexInfo {
	uint32_t appid;
	std::string doc_id;
	uint32_t doc_version;
	uint32_t field;
	uint32_t word_freq;
	uint32_t created_time;
	std::string pos;
	std::string extend;
	double distance;

	IndexInfo(){
		appid = 0;
		doc_version = 0;
		field = 0;
		word_freq = 0;
		created_time = 0;
		distance = 0.0;
	}

	bool operator<(const IndexInfo& src) const  {
		if(this->doc_id == src.doc_id){
			return this->doc_version < src.doc_version;
		}
		return this->doc_id.compare(src.doc_id) < 0;
	}
};

struct ExtraFilterKey
{
	std::string field_name;
	std::string field_value;
	uint16_t field_type;
};

struct TerminalQryCond{
	uint32_t sort_type;
	std::string sort_field;
	std::string last_id;
	std::string last_score;
	uint32_t limit_start;
	uint32_t page_size;
};

struct OrderOpCond{
	std::string last_id;
	uint32_t limit_start;
	uint32_t count;
	bool has_extra_filter;
};

struct TerminalRes{
	std::string doc_id;
	double score;
};

enum QUERYRTPE{
	QUERY_TYPE_BOOL,
	QUERY_TYPE_MATCH,
	QUERY_TYPE_RANGE,
	QUERY_TYPE_TERM,
	QUERY_TYPE_GEO_DISTANCE,
	QUERY_TYPE_GEO_SHAPE,
};


typedef std::set<std::string> ValidDocSet;
typedef std::set<std::string> HighLightWordSet;

typedef std::vector<KeyInfo> KeyInfoVet;
typedef std::map<std::string, KeyInfoVet> DocKeyinfosMap;

typedef std::map<std::string, uint32_t> KeywordDoccountMap;

#endif