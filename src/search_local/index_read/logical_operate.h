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

#include "component.h"
#include <map>
#include <set>
using namespace std;

typedef vector<KeyInfo> vec;
typedef vector<IndexInfo> (*logical_func)(vector<IndexInfo> &a, vector<IndexInfo> &b);

class LogicalOperate
{
public:
	LogicalOperate(uint32_t appid, uint32_t sort_type, uint32_t has_gis, uint32_t cache_switch);
	~LogicalOperate();
	
	int Process(const vector<vector<FieldInfo> >& keys, vector<IndexInfo>& vecs, set<string>& highlightWord, map<string, vec> &ves, map<string, uint32_t> &key_in_doc);
	int ProcessComplete(const vector<FieldInfo>& complete_keys, vector<IndexInfo>& complete_vecs, vector<string>& word_vec, map<string, vec> &ves, map<string, uint32_t> &key_in_doc);
	void SetFunc(logical_func func);
	int ProcessTerminal(const vector<vector<FieldInfo> >& and_keys, const TerminalQryCond& query_cond, vector<TerminalRes>& vecs);

private:
	void CalculateByWord(FieldInfo fieldInfo, const vector<IndexInfo> &doc_info, map<string, vec> &ves, map<string, uint32_t> &key_in_doc);
	void SetDocIndexCache(const vector<IndexInfo> &doc_info, string& indexJsonStr);
	bool GetDocIndexCache(string word, uint32_t field, vector<IndexInfo> &doc_info);
	int GetDocIdSetByWord(FieldInfo fieldInfo, vector<IndexInfo> &doc_info);

private:
	uint32_t m_appid;
	uint32_t m_sort_type;
	uint32_t m_has_gis;
	uint32_t m_cache_switch;
	logical_func m_func;
};

