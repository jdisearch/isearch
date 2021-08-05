/*
 * =====================================================================================
 *
 *       Filename:  index_tbl_op.h
 *
 *    Description:  index tbl op class definition.
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
#ifndef INDEX_TBL_OP_H
#define INDEX_TBL_OP_H

#include "log.h"
#include "dtcapi.h"
#include "chash.h"

#include "comm.h"
#include "search_conf.h"
#include "search_util.h"
#include "json/value.h"
#include "result_context.h"
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <set>
using namespace std;

struct TopDocInfo {
	uint32_t appid;
	string doc_id;
	uint32_t doc_version;
	uint32_t created_time;
	uint32_t weight;
};

struct DocVersionInfo
{
	string doc_id;
	string content;
	uint32_t doc_version;
};

class CIndexTableManager
{
public:
	int InitServer(const SDTCHost &dtchost, string bindAddr);
	int InitServer2(const SDTCHost &dtchost);
	bool GetDocInfo(uint32_t appid, string word, uint32_t field_id, vector<IndexInfo> &doc_info);
	int GetDocCnt(uint32_t appid);

	bool get_snapshot_execute(int left, int right,uint32_t appid, const vector<IndexInfo>& no_filter_docs, vector<DocVersionInfo>& docVersionInfo);
	bool get_top_snapshot_execute(int left, int right, uint32_t appid, vector<TopDocInfo>& no_filter_docs, vector<DocVersionInfo>& docVersionInfo);
	bool TopDocValid(uint32_t appid, vector<TopDocInfo>& no_filter_docs, vector<TopDocInfo>& doc_info);
	bool DocValid(uint32_t appid, const vector<IndexInfo>& vecs, bool need_version, map<string, uint32_t>& valid_version, hash_string_map& doc_content_map);
	bool GetTopDocInfo(uint32_t appid, string word, vector<TopDocInfo>& doc_info);
	bool GetDocContent(uint32_t appid, const std::vector<IndexInfo>& index_infos, hash_string_map& doc_content);
	bool GetSnapshotContent(int left, int right, uint32_t appid , const std::vector<IndexInfo>& index_infos, hash_string_map& doc_content);
	bool GetSuggestDoc(uint32_t appid, int index, uint32_t len, uint32_t field, const IntelligentInfo &info, vector<IndexInfo> &doc_id_set);
	bool GetSuggestDocWithoutCharacter(uint32_t appid, int index, uint32_t len,  uint32_t field, const IntelligentInfo &info, vector<IndexInfo> &doc_id_set);
	bool GetScoreByField(uint32_t appid, string doc_id, string sort_field, uint32_t sort_type, ScoreInfo &score_info);
	bool DocValid(uint32_t appid, string doc_id, bool &is_valid);
	bool GetContentByField(uint32_t appid, string doc_id, uint32_t doc_version, const vector<string>& fields, Json::Value &value);

private:
	DTC::Server server;
};

vector<IndexInfo> vec_intersection(vector<IndexInfo> &a, vector<IndexInfo> &b);
vector<IndexInfo> vec_union(vector<IndexInfo> &a, vector<IndexInfo> &b);
vector<IndexInfo> vec_difference(vector<IndexInfo> &a, vector<IndexInfo> &b);

extern CIndexTableManager g_IndexInstance;
extern CIndexTableManager g_hanpinIndexInstance;

#endif
