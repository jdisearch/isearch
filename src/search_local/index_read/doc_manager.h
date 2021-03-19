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

#ifndef __DOC_MANAGER_H__
#define __DOC_MANAGER_H__
#include "comm.h"
#include "json/json.h"
#include <map>
#include <set>
using namespace std;

class Component;
class DocManager{
public:
	DocManager(Component *c);
	~DocManager();

	bool CheckDocByExtraFilterKey(string doc_id);
	bool GetDocContent(uint32_t m_has_gis, vector<IndexInfo> &doc_id_ver_vec, set<string> &valid_docs, hash_double_map &distances);
	bool AppendFieldsToRes(Json::Value &response, vector<string> &m_fields);
	bool GetScoreMap(string doc_id, uint32_t m_sort_type, string m_sort_field, FIELDTYPE &m_sort_field_type, uint32_t appid);
	map<string, string>& ScoreStrMap();
	map<string, int>& ScoreIntMap();
	map<string, double>& ScoreDoubleMap();
	map<string, uint32_t>& ValidVersion();

private:
	void CheckIfKeyValid(const vector<ExtraFilterKey>& extra_filter_vec, const Json::Value &value, bool flag, bool &key_valid);

private:
	map<string, string> score_str_map;
	map<string, int> score_int_map;
	map<string, double> score_double_map;
	map<string, uint32_t> valid_version;
	hash_string_map doc_content_map;
	Component *component;
};

#endif