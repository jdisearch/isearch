/*
 * =====================================================================================
 *
 *       Filename:  search_task.h
 *
 *    Description:  search task class definition.
 *
 *        Version:  1.0
 *        Created:  19/03/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef _SEARCH_TASK_H_
#define _SEARCH_TASK_H_

#include "comm.h"
#include "process_task.h"
#include "json/value.h"
#include "search_conf.h"
#include "index_tbl_op.h"
#include "task_request.h"
#include "skiplist.h"
#include "component.h"
#include "logical_operate.h"
#include "doc_manager.h"
#include <string>
#include <map>
#include <vector>
using  namespace std;

typedef vector<KeyInfo> vec;

class SearchTask : public ProcessTask
{
public:
	SearchTask();
	virtual int Process(CTaskRequest *request);
	virtual ~SearchTask();

private:
	int DoJob(CTaskRequest *request);
	int GetTopDocIdSetByWord(FieldInfo fieldInfo, vector<TopDocInfo>& doc_info);
	int GetTopDocScore(map<string, double>& top_doc_score);
	int GetDocScore(map<string, double>& top_doc_score);
	int GetValidDoc(map<string, vec> &ves, vector<string> &word_vec, map<string, uint32_t> &key_in_doc, hash_double_map &distances, set<string> &valid_docs);
	void AppendHighLightWord(Json::Value& response);

private:
	Component *component;
	LogicalOperate *logical_operate;
	DocManager *doc_manager;
	vector<FieldInfo> complete_keys;

	string m_Primary_Data;
	FIELDTYPE m_sort_field_type;
	uint32_t m_index_set_cnt;
	uint32_t m_appid;
	uint32_t m_sort_type;
	string m_sort_field;
	vector<string> m_fields;

	uint32_t m_has_gis; //该appid是否包含有地理位置gis信息的查询
	set<string> highlightWord;
	SkipList skipList;
};





#endif
