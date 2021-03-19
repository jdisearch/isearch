/*
 * =====================================================================================
 *
 *       Filename:  split_manager.h
 *
 *    Description:  SplitManager class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  shrewdlin, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __SPLIT_MANAGER_H__
#define __SPLIT_MANAGER_H__
#include <string>
#include <sys/types.h>
#include <vector>
#include "index_conf.h"
#include "split_tool.h"
using namespace std;

#define NUMBER_ID 500000000
#define MAXNUMBER 100000000

struct table_info{
	int is_primary_key;
	int field_type;
	int index_tag;
	int snapshot_tag;
	int field_value;
	int segment_tag;
	int segment_feature;
	string index_info;
};

class SplitManager {
public:
	SplitManager();
	~SplitManager();
	static SplitManager *Instance(){
		return CSingleton<SplitManager>::Instance();
	}

	static void Destroy(){
		CSingleton<SplitManager>::Destroy();
	}

	bool Init(const SGlobalIndexConfig &global_cfg);
	void DeInit();
	vector<vector<string> > split(string str,uint32_t appid);
	vector<string> split(string str);
	bool wordValid(string word, uint32_t appid, uint32_t &id);
	bool GetWordInfo(string word, uint32_t appid, WordInfo &word_info) {
		return seg.GetWordInfoFromDictOnly(word, appid, word_info);
	}
	struct table_info *get_table_info(uint32_t appid, string filed_name);
	bool is_effective_appid(uint32_t appid);
	bool GetCharactId(string charact, uint32_t &id);
	bool GetPhoneticId(string phonetic, uint32_t &id);
	vector<string> GetPhonetic(string charact);
	bool getHanpinField(uint32_t appid, vector<uint32_t> & field_vec);
	bool getUnionKeyField(uint32_t appid, vector<string> & field_vec);

private:
	bool fetch_tbinfo_from_mysql_to_map();

	FBSegment seg;
	set<string> stop_word_set;
	map<string, u_int32_t> word_map;
	map<uint32_t,map<string,table_info> > tableDefine;
	string split_mode;
	map<string, uint32_t> charact_map;
	map<string, uint32_t> phonetic_map;
	multimap<string, string> charact_phonetic_map;
};

string trim(string& str);
string delPrefix(string& str);


#endif
