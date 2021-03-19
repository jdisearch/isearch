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

#ifndef __COMPONENT_H__
#define __COMPONENT_H__
#include "comm.h"
#include "json/json.h"
#include <string>
#include <vector>
using namespace std;

class Component
{
public:
	Component();
	~Component();

	void GetQueryWord(uint32_t &m_has_gis);
	const vector<vector<FieldInfo> >& Keys();
	const vector<vector<FieldInfo> >& AndKeys();
	const vector<vector<FieldInfo> >& InvertKeys();
	const vector<ExtraFilterKey>& ExtraFilterKeys();
	const vector<ExtraFilterKey>& ExtraFilterAndKeys();
	const vector<ExtraFilterKey>& ExtraFilterInvertKeys();
	int ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet);
	void InitSwitch();
	string QueryWord();
	void SetQueryWord(string query_word);
	string ProbablyData();
	void SetProbablyData(string probably_data);
	string Latitude();
	string Longitude();
	double Distance();
	string Data();
	string DataAnd();
	string DataInvert();
	string DataComplete();
	uint32_t JdqSwitch();
	uint32_t Appid();
	uint32_t SortType();
	uint32_t PageIndex();
	uint32_t PageSize();
	uint32_t ReturnAll();
	uint32_t CacheSwitch();
	uint32_t TopSwitch();
	uint32_t SnapshotSwitch();
	string SortField();
	string LastId();
	string LastScore();
	bool SearchAfter();
	vector<string>& Fields();
	uint32_t TerminalTag();
	bool TerminalTagValid();

private:
	void GetFieldWords(int type, string dataStr, uint32_t appid, uint32_t &m_has_gis);
	void AddToFieldList(int type, vector<FieldInfo>& fields);
	void GetKeyFromFieldInfo(const vector<FieldInfo>& field_info_vec, vector<string>& key_vec);
	vector<string> Combination(vector<vector<string> > &dimensionalArr);

private:
	vector<vector<FieldInfo> > keys;
	vector<vector<FieldInfo> > and_keys;
	vector<vector<FieldInfo> > invert_keys;
	vector<ExtraFilterKey> extra_filter_keys;
	vector<ExtraFilterKey> extra_filter_and_keys;
	vector<ExtraFilterKey> extra_filter_invert_keys;

	string m_Query_Word;
	string m_probably_data;
	string latitude;
	string longitude;
	string gisip;
	double distance;

	string m_Data; 	//查询词
	string m_Data_and; 		// 包含该查询词
	string m_Data_invert;   // 不包含该查询词
	string m_Data_complete; // 完整关键词
	uint32_t m_page_index;
	uint32_t m_page_size;
	uint32_t m_return_all;
	uint32_t m_cache_switch;
	uint32_t m_top_switch;
	uint32_t m_snapshot_switch;
	uint32_t m_sort_type;
	uint32_t m_appid;
	string m_user_id;
	string m_sort_field;
	string m_last_id;
	string m_last_score;
	bool m_search_after;
	vector<string> m_fields;
	string m_default_query;
	uint32_t m_jdq_switch;
	uint32_t m_terminal_tag;
	bool m_terminal_tag_valid;
};
#endif