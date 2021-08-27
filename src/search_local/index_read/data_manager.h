/*
 * =====================================================================================
 *
 *       Filename:  data_manager.h
 *
 *    Description:  data manager class definition.
 *
 *        Version:  1.0
 *        Created:  09/05/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __DATA_MANAGER_H__
#define __DATA_MANAGER_H__

#include "log.h"
#include "search_conf.h"
#include "protocal.h"
#include "comm.h"
#include <string>
#include <set>
#include <algorithm>
using namespace std;

typedef set<uint32_t> set_uint;
typedef pair<string, uint32_t> PAIR_UINT;

struct CmpByUint {
	bool operator()(const PAIR_UINT& lhs, const PAIR_UINT& rhs) {
		return lhs.second > rhs.second;
	}
};

struct WordIdInfo {
	string word;
	uint32_t word_freq;
};

struct IntelligentTable
{	
	map<uint32_t, WordIdInfo> word_id_map; // 中文词语id
	map<uint16_t, set_uint> charact_map[8];
	map<uint16_t, set_uint> phonetic_map[8];
	map<string, set_uint> initial_map[8];
};

class DataManager
{
public:

	DataManager();
	~DataManager();
	static DataManager *Instance()
	{
		return CSingleton<DataManager>::Instance();
	}

	static void Destroy()
	{
		CSingleton<DataManager>::Destroy();
	}

	bool Init(const SGlobalConfig &global_cfg);
	IntelligentTable* LoadIntelligentTable(const string& file_path);
	int GetWordId(string word, uint32_t appid, uint32_t &id);
	bool GetCharactId(string charact, uint32_t &id);
	bool GetPhoneticId(string phonetic, uint32_t &id);
	bool GetSuggestWord(const IntelligentInfo &info, vector<string> &word_vec, uint32_t suggest_cnt);
	bool GetEnSuggestWord(const IntelligentInfo &info, vector<string> &word_vec, uint32_t suggest_cnt);
	void GetSynonymByKey(const string& key, vector<FieldInfo>& keys);
	bool GetRelateWord(string word, vector<string> &word_vec, uint32_t relate_cnt);
	bool GetHotWord(uint32_t appid, vector<string> &word_vec, uint32_t hot_cnt);
	int UpdateHotWord(const map<uint32_t, map<string, uint32_t> > &hot_map);
	int UpdateRelateWord(const vector<vector<string> > &relate_vec);
	bool IsEnWord(string str);
	bool IsChineseWord(uint32_t appid, string str);
	string GetPhonetic(string charact);
	bool IsSensitiveWord(string word);
	bool IsContainKey(vector<FieldInfo>& keys, string key);
	bool IsPhonetic(string phonetic) {
		bool result = false;
		if (phonetic_set.find(phonetic) != phonetic_set.end())
			result = true;
		return result;
	}
	void ReplaceTableInfo(IntelligentTable *table);
	uint32_t GetRowCnt() {return rowCnt;}
	void SetRowCnt(uint32_t cnt) {rowCnt = cnt;}

private:
	
	IntelligentTable *itable;
	map<string, uint32_t> charact_map; // 汉字到id的映射
	map<string, uint32_t> phonetic_map; // 拼音到id的映射
	set<string> phonetic_set;
	map<string, string> charact_phonetic_map; // 汉字到拼音的映射
	map<string, uint32_t> en_word_map; // 英文词语到id的映射
	map<uint32_t, WordIdInfo> word_id_map; // 中文词语id
	map<uint32_t, WordIdInfo> en_word_id_map; // 英文词语id
	map<string, set_uint> en_initial_map[8];

	map<string, uint32_t> relate_map;
	multimap<uint32_t, string> relate_id_map;
	map<string, uint32_t> synonym_map;
	multimap<uint32_t, string> synonym_id_map;
	map<uint32_t, map<string, uint32_t> > hot_map;
	map<uint32_t, vector<PAIR_UINT> > hot_vec;
	vector<string> sensitive_vec;
	string sAnalyzePath;
	string sRelatePath;

	uint32_t rowCnt; //DB中智能推荐词语的行数

};

#endif