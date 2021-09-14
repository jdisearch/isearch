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

#include <string>
#include <algorithm>
#include <sstream>  
#include <ctype.h>
#include "data_manager.h"
#include "timemanager.h"
#include "search_util.h"
#include "comm.h"
#include "split_manager.h"
#include <fstream>
using namespace std;

extern pthread_mutex_t mutex;

DataManager::DataManager() {
	rowCnt = 0;
	itable = NULL;
	charact_map.clear();
	phonetic_map.clear();
	word_id_map.clear();
	hot_map.clear();
}

DataManager::~DataManager() {
	if (itable){
		delete itable;
	}
	itable = NULL;
}

IntelligentTable* DataManager::LoadIntelligentTable(const string& file_path)
{
	string str;
	uint32_t word_id = 0;
	string word;
	
	ifstream intelligent_infile;
	IntelligentTable *tableInfo = new IntelligentTable();
	if (tableInfo == NULL) {
		log_error("malloc tableInfo error");
		return NULL;
	}
	intelligent_infile.open(file_path.c_str());
	if (intelligent_infile.is_open() == false) {
		log_error("open file error: %s.", file_path.c_str());
		return NULL;
	}
	while (getline(intelligent_infile, str)) {
		vector<string> int_vec = splitEx(str, "\t");
		if (int_vec.size() == 27) {
			int i = 0;
			word_id = atoi(int_vec[i++].c_str());
			WordIdInfo word_info;
			word_info.word = int_vec[i++];
			IntelligentInfo info;
			for(int j = 0; j < 8; j++){
				info.charact_id[j] = atoi(int_vec[i++].c_str());
			}
			for(int j = 0; j < 8; j++){
				info.phonetic_id[j] = atoi(int_vec[i++].c_str());
			}
			for(int j = 0; j < 8; j++){
				info.initial_char[j] = int_vec[i++];
			}
			word_info.word_freq = atoi(int_vec[i++].c_str());
			tableInfo->word_id_map[word_id] = word_info;
			for(int j = 0; j < 8; j++){
				if (info.charact_id[j] != 0 && info.charact_id[j] != 65535) {
					map<uint16_t, set_uint>& charact_map_ = tableInfo->charact_map[j];
					charact_map_[info.charact_id[j]].insert(word_id);
				}
			}
			for(int j = 0; j < 8; j++){
				if (info.phonetic_id[j] != 0 && info.phonetic_id[j] != 65535) {
					map<uint16_t, set_uint>& phonetic_map_ = tableInfo->phonetic_map[j];
					phonetic_map_[info.phonetic_id[j]].insert(word_id);
				}
			}
			for(int j = 0; j < 8; j++){
				if (info.initial_char[j] != "0") {
					map<string, set_uint>& initial_map_ = tableInfo->initial_map[j];
					initial_map_[info.initial_char[j]].insert(word_id);
				}
			}
		}
	}
	
	log_debug("charact_01_map: %d, phonetic_01_map: %d, initial_01_map: %d,\
	charact_02_map: %d, phonetic_02_map: %d, initial_02_map: %d,charact_03_map: %d, phonetic_03_map: %d, initial_03_map: %d",
		(int)tableInfo->charact_map[0].size(), (int)tableInfo->phonetic_map[0].size(), (int)tableInfo->initial_map[0].size(),
		(int)tableInfo->charact_map[1].size(), (int)tableInfo->phonetic_map[1].size(), (int)tableInfo->initial_map[1].size(), 
		(int)tableInfo->charact_map[2].size(), (int)tableInfo->phonetic_map[2].size(), (int)tableInfo->initial_map[2].size());
	return tableInfo;
}

void DataManager::ReplaceTableInfo(IntelligentTable *table)
{
	pthread_mutex_lock(&mutex);
	delete itable;
	itable = table;
	pthread_mutex_unlock(&mutex);
}

bool DataManager::Init(const SGlobalConfig &global_cfg) {
	
	string str;
	uint32_t phonetic_id = 0;
	string phonetic;
	uint32_t character_id = 0;
	string charact;
	uint32_t word_id = 0;
	string word;
	sAnalyzePath = global_cfg.sAnalyzePath;
	sRelatePath = global_cfg.sRelatePath;
	
	ifstream phonetic_infile;
	phonetic_infile.open(global_cfg.sPhoneticPath.c_str());
	if (phonetic_infile.is_open() == false) {
		log_error("open file error: %s.", global_cfg.sPhoneticPath.c_str());
		return false;
	}

	while (getline(phonetic_infile, str))
	{
		vector<string> str_vec = splitEx(str, "\t");
		if (str_vec.size() == 2) {
			phonetic_id = atoi(str_vec[0].c_str());
			phonetic = str_vec[1];
			phonetic_map[phonetic] = phonetic_id;
			phonetic_set.insert(phonetic);
		}
	}
	phonetic_infile.close();

	ifstream phonetic_base_infile;
	phonetic_base_infile.open(global_cfg.sPhoneticBasePath.c_str());
	if (phonetic_base_infile.is_open() == false) {
		log_error("open file error: %s.", global_cfg.sPhoneticBasePath.c_str());
		return false;
	}

	while (getline(phonetic_base_infile, str))
	{
		vector<string> str_vec = splitEx(str, "\t");
		if (str_vec.size() == 2) {
			charact = str_vec[0];
			phonetic = str_vec[1];
			charact_phonetic_map[charact] = phonetic;
		}
	}
	phonetic_base_infile.close();

	ifstream character_infile;
	character_infile.open(global_cfg.sCharacterPath.c_str());
	if (character_infile.is_open() == false) {
		log_error("open file error: %s.", global_cfg.sCharacterPath.c_str());
		return false;
	}

	while (getline(character_infile, str))
	{
		vector<string> str_vec = splitEx(str, "\t");
		if (str_vec.size() == 2) {
			character_id = atoi(str_vec[0].c_str());
			charact = str_vec[1];
			charact_map[charact] = character_id;
		}
	}
	character_infile.close();

	ifstream en_words_infile;
	en_words_infile.open(global_cfg.sEnWordsPath.c_str());
	if (en_words_infile.is_open() == false) {
		log_error("open file error: %s.", global_cfg.sEnWordsPath.c_str());
		return false;
	}
	while (getline(en_words_infile, str)) {
		vector<string> str_vec = splitEx(str, "\t");
		WordIdInfo info;
		word_id = atoi(str_vec[0].c_str());
		info.word = str_vec[1];
		ToLower(info.word);
		info.word_freq = atoi(str_vec[2].c_str());
		en_word_id_map[word_id] = info;
		en_word_map[info.word] = word_id;
	}
	en_words_infile.close();

	if (!(itable=LoadIntelligentTable(global_cfg.sIntelligentPath)))
		return false;

	ifstream en_intelligent_infile;
	en_intelligent_infile.open(global_cfg.sEnIntelligentPath.c_str());
	if (en_intelligent_infile.is_open() == false) {
		log_error("open file error: %s.", global_cfg.sEnIntelligentPath.c_str());
		return false;
	}
	while (getline(en_intelligent_infile, str)) {
		vector<string> int_vec = splitEx(str, "\t");
		if (int_vec.size() == 25) {
			int i = 0;
			word_id = atoi(int_vec[i++].c_str());
			IntelligentInfo info;
			for(int j = 0; j < 8; j++){
				info.charact_id[j] = atoi(int_vec[i++].c_str());
			}
			for(int j = 0; j < 8; j++){
				info.phonetic_id[j] = atoi(int_vec[i++].c_str());
			}
			for(int j = 0; j < 8; j++){
				info.initial_char[j] = int_vec[i++];
			}
			for(int j = 0; j < 8; j++){
				if (info.initial_char[j] != "0") {
					map<string, set_uint>& en_initial_map_ = en_initial_map[j];
					en_initial_map_[info.initial_char[j]].insert(word_id);
				}
			}
		}
	}
	en_intelligent_infile.close();
	log_debug("en_initial_01_map: %d, en_initial_02_map: %d, en_initial_03_map: %d", (int)en_initial_map[0].size(), (int)en_initial_map[1].size(), (int)en_initial_map[2].size());

	ifstream relate_infile;
	relate_infile.open(global_cfg.sRelatePath.c_str());
	if (relate_infile.is_open() == false) {
		log_error("open file error.");
		return false;
	}
	uint32_t i = 0;
	while (getline(relate_infile, str)) {
		i++;
		vector<string> int_vec = splitEx(str, "\t");
		vector<string>::iterator iter = int_vec.begin();
		for (; iter != int_vec.end(); iter++) {
			relate_id_map.insert(make_pair(i,*iter));
			relate_map[*iter] = i;
		}
	}
	relate_infile.close();

	ifstream synonym_infile;
	synonym_infile.open(global_cfg.sSynonymPath.c_str());
	if (synonym_infile.is_open() == false) {
		log_error("open file error.");
		return false;
	}
	i = 0;
	while (getline(synonym_infile, str)) {
		i++;
		vector<string> int_vec = splitEx(str, "\t");
		vector<string>::iterator iter = int_vec.begin();
		for (; iter != int_vec.end(); iter++) {
			synonym_id_map.insert(make_pair(i, *iter));
			synonym_map[*iter] = i;
		}
	}

	ifstream sensitive_infile;
	sensitive_infile.open(global_cfg.sSensitivePath.c_str());
	if (sensitive_infile.is_open() == false) {
		log_error("open file error.");
		return false;
	}
	while (getline(sensitive_infile, str)) {
		sensitive_vec.push_back(str);
	}

	ifstream hot_infile;
	hot_infile.open(global_cfg.sAnalyzePath.c_str());
	if (hot_infile.is_open() == false) {
		log_error("open file error.");
		return false;
	}
	uint32_t cnt = 0;
	uint32_t appid = 0;
	while (getline(hot_infile, str)) {
		vector<string> word_vec = splitEx(str, "\t");
		if (word_vec.size() == 3) {
			appid = atoi(word_vec[0].c_str());
			word = word_vec[1];
			cnt = atoi(word_vec[2].c_str());
			if (word == "") {
				continue;
			}
			hot_map[appid][word] = cnt;
		}
	}
	hot_infile.close();
	map<uint32_t, map<string, uint32_t> >::iterator iter = hot_map.begin();
	for (; iter != hot_map.end(); iter ++) {
		uint32_t appid = iter->first;
		map<string, uint32_t> freq_map = iter->second;
		log_debug("appid: %u, count:%d", appid, (int)freq_map.size());
		vector<PAIR_UINT> freq_vec;
		freq_vec.insert(freq_vec.begin(), freq_map.begin(), freq_map.end());
		sort(freq_vec.begin(), freq_vec.end(), CmpByUint());
		hot_vec[appid] = freq_vec;
	}
	return true;
}

bool DataManager::GetRelateWord(string word, vector<string> &word_vec, uint32_t relate_cnt) {
	if (relate_map.find(word) == relate_map.end()) {
		word_vec.clear();
		return false;
	}
	uint32_t index = relate_map[word];
	uint32_t i = 0;
	typedef std::multimap<uint32_t, string>::iterator MultiMapIterator;
	std::pair<MultiMapIterator, MultiMapIterator> iterPair = relate_id_map.equal_range(index);
	for (MultiMapIterator it = iterPair.first; it != iterPair.second; ++it)
	{
		if ((*it).second == word) {
			continue;
		}
		if (i++ >= relate_cnt) {
			break;
		}
		word_vec.push_back((*it).second);
	}

	return true;
}

int DataManager::UpdateRelateWord(const vector<vector<string> > &relate_vec) {
	vector<vector<string> >::const_iterator vec_iter = relate_vec.begin();
	for (; vec_iter != relate_vec.end(); vec_iter++) {
		vector<string> vec_data = *vec_iter;
		if (vec_data.size() == 1) {
			continue;
		}
		vector<uint32_t> id_vec;
		vector<string>::iterator iter = vec_data.begin();
		for (; iter != vec_data.end(); iter++) {
			if (relate_map.find(*iter) == relate_map.end()) {
				id_vec.push_back(0);
			}
			else {
				id_vec.push_back(relate_map.at(*iter));
			}
		}
		bool flag = false;
		uint32_t new_id = 0;
		for (size_t i = 0; i < id_vec.size(); i++) {
			if (id_vec[i] != 0) {
				flag = true;
				new_id = id_vec[i];
			}
		}
		if (false == flag) {
			if (relate_map.size() == 0) {
				new_id = 1;
			}
			else {
				multimap<uint32_t, string>::iterator end_iter = relate_id_map.end();  // find max id in relate_id_map
				new_id = end_iter->first + 1;
			}
		}
		for (iter = vec_data.begin(); iter != vec_data.end(); iter++) {
			if (relate_map.find(*iter) == relate_map.end()) {
				relate_map.insert(make_pair(*iter, new_id));
				relate_id_map.insert(make_pair(new_id, *iter));
			}
		}
	}

	ofstream relate_infile;
	relate_infile.open(sRelatePath.c_str());
	if (relate_infile.is_open() == false) {
		log_error("open file error.");
		return -RT_OPEN_FILE_ERR;
	}
	multimap<uint32_t, string>::iterator id_word_iter = relate_id_map.begin();
	while (id_word_iter != relate_id_map.end()) {
		typedef multimap<uint32_t, string>::size_type sz_type;
		sz_type cnt = relate_id_map.count(id_word_iter->first);
		for (sz_type i = 0; i != cnt; ++id_word_iter, ++i) {
			relate_infile << id_word_iter->second << "\t";
		}
		relate_infile << endl;
	}
	relate_infile.close();

	return 0;
}

bool DataManager::IsEnWord(string str) {
	return en_word_map.find(str) != en_word_map.end();
}
bool DataManager::IsChineseWord(uint32_t appid, string str) {
	WordInfo word_info;
	if (SplitManager::Instance()->GetWordInfo(str, appid, word_info) == true) {
		uint32_t word_id = word_info.word_id;
		if (word_id<100000000) {
			return true;
		}
	}
	return false;
}

string DataManager::GetPhonetic(string charact) {
	if (charact_phonetic_map.find(charact) != charact_phonetic_map.end()) {
		return charact_phonetic_map[charact];
	}
	else {
		return "";
	}
}

bool DataManager::IsSensitiveWord(string word) {
	bool result = false;
	if (find(sensitive_vec.begin(), sensitive_vec.end(), word) != sensitive_vec.end())
		result = true;
	return result;
}

void DataManager::GetSynonymByKey(const string& key, vector<FieldInfo>& keys)
{
	if (synonym_map.find(key) == synonym_map.end())
		return ;
	uint32_t id = synonym_map[key];
	multimap<uint32_t, string>::iterator it;
	it = synonym_id_map.find(id);
	for (int i = 0, len = synonym_id_map.count(id);i < len; ++i,++it) {
		if (it->second == key)
			continue ;
		FieldInfo fieldInfo;
		fieldInfo.word = it->second; 
		keys.push_back(fieldInfo);
	}
	return ;
}

int DataManager::GetWordId(string word, uint32_t appid, uint32_t &id) {
	id = 0;
	WordInfo word_info;
	if (SplitManager::Instance()->GetWordInfo(word, appid, word_info) == true) {
		id = word_info.word_id;
	}
	return 0;
}

bool DataManager::GetCharactId(string charact, uint32_t &id) {
	id = 0;
	if (charact_map.find(charact) != charact_map.end()) {
		id = charact_map[charact];
	}
	return true;
}

bool DataManager::GetPhoneticId(string phonetic, uint32_t &id) {
	id = 0;
	if (phonetic_map.find(phonetic) != phonetic_map.end()) {
		id = phonetic_map[phonetic];
	}
	return true;
}

bool DataManager::GetSuggestWord(const IntelligentInfo &info, vector<string> &word_vec, uint32_t suggest_cnt) {
	pthread_mutex_lock(&mutex);
	set<uint32_t> word_id_set;
	int count = 0;
	if (info.charact_id[0] != 0) {
		if (word_id_set.empty() && count == 0) {
			count++;
			word_id_set = itable->charact_map[0][info.charact_id[0]];
		}
	}
	if (info.phonetic_id[0] != 0) {
		if (word_id_set.empty() && count == 0) {
			count++;
			word_id_set = itable->phonetic_map[0][info.phonetic_id[0]];
		}
	}
	if (info.initial_char[0] != "") {
		if (word_id_set.empty() && count == 0) {
			count++;
			word_id_set = itable->initial_map[0][info.initial_char[0]];
		}
	}

	for(int i = 1; i < 8; i++){
		if (info.charact_id[i] != 0) {
			word_id_set = sets_intersection(word_id_set, itable->charact_map[i][info.charact_id[i]]);
		}
	}
	for(int i = 1; i < 8; i++){
		if (info.phonetic_id[i] != 0) {
			word_id_set = sets_intersection(word_id_set, itable->phonetic_map[i][info.phonetic_id[i]]);
		}
	}
	for(int i = 1; i < 8; i++){
		if (info.initial_char[i] != "") {
			word_id_set = sets_intersection(word_id_set, itable->initial_map[i][info.initial_char[i]]);
		}
	}

	map<string, uint32_t> word_freq_map;
	set<uint32_t>::iterator iter = word_id_set.begin();
	for (; iter != word_id_set.end(); iter++) {
		if (itable->word_id_map.find(*iter) != itable->word_id_map.end()) {
			WordIdInfo info = itable->word_id_map[*iter];
			word_freq_map[info.word] = info.word_freq;
		}
	}
	pthread_mutex_unlock(&mutex);

	vector<PAIR_UINT> word_freq_vec(word_freq_map.begin(), word_freq_map.end());
	sort(word_freq_vec.begin(), word_freq_vec.end(), CmpByUint());
	for (uint32_t i = 0; i < suggest_cnt; i++) {
		if (i == word_freq_vec.size()) {
			break;
		}
		word_vec.push_back(word_freq_vec[i].first);
	}

	return true;
}

bool DataManager::GetEnSuggestWord(const IntelligentInfo &info, vector<string> &word_vec, uint32_t suggest_cnt) {
	set<uint32_t> word_id_set;
	if (info.initial_char[0] != "") {
		word_id_set = en_initial_map[0][info.initial_char[0]];
	}
	for(int i = 1; i < 8; i++){
		if (info.initial_char[i] != "") {
			word_id_set = sets_intersection(word_id_set, en_initial_map[i][info.initial_char[i]]);
		}
	}

	map<string, uint32_t> word_freq_map;
	set<uint32_t>::iterator iter = word_id_set.begin();
	for (; iter != word_id_set.end(); iter++) {
		if (en_word_id_map.find(*iter) != en_word_id_map.end()) {
			WordIdInfo info = en_word_id_map[*iter];
			word_freq_map[info.word] = info.word_freq;
		}
	}

	vector<PAIR_UINT> word_freq_vec(word_freq_map.begin(), word_freq_map.end());
	sort(word_freq_vec.begin(), word_freq_vec.end(), CmpByUint());
	for (uint32_t i = 0; i < suggest_cnt; i++) {
		if (i == word_freq_vec.size()) {
			break;
		}
		word_vec.push_back(word_freq_vec[i].first);
	}

	return true;
}

bool DataManager::GetHotWord(uint32_t appid, vector<string> &word_vec, uint32_t hot_cnt) {
	uint32_t cnt = 0;
	vector<PAIR_UINT> freq_vec = hot_vec[appid];
	for (uint32_t i = 0; i < freq_vec.size(); i++) {
		pair<string, uint32_t> tmp = freq_vec[i];
		if (cnt++ >= hot_cnt) {
			break;
		}
		word_vec.push_back(tmp.first);
	}
	return true;
}

int DataManager::UpdateHotWord(const map<uint32_t, map<string, uint32_t> > &new_hot_map) {
	map<uint32_t, map<string, uint32_t> >::const_iterator iter = new_hot_map.begin();
	for (; iter != new_hot_map.end(); iter++) {
		uint32_t appid = iter->first;
		map<string, uint32_t> freq_map = iter->second;
		map<string, uint32_t>::iterator freq_iter = freq_map.begin();
		for (; freq_iter != freq_map.end(); freq_iter++) {
			if (hot_map[appid].find(freq_iter->first) == hot_map[appid].end()) {
				hot_map[appid][freq_iter->first] = freq_iter->second;
			}
			else {
				hot_map[appid][freq_iter->first] += freq_iter->second;
			}
		}
	}

	hot_vec.clear();
	map<uint32_t, map<string, uint32_t> >::iterator iter1 = hot_map.begin();
	for (; iter1 != hot_map.end(); iter1++) {
		uint32_t appid = iter1->first;
		map<string, uint32_t> freq_map = iter1->second;
		vector<PAIR_UINT> freq_vec;
		freq_vec.insert(freq_vec.begin(), freq_map.begin(), freq_map.end());
		sort(freq_vec.begin(), freq_vec.end(), CmpByUint());
		hot_vec[appid] = freq_vec;
	}

	ofstream hot_infile;
	hot_infile.open(sAnalyzePath.c_str());
	if (hot_infile.is_open() == false) {
		log_error("open file error.");
		return -RT_OPEN_FILE_ERR;
	}
	
	for (iter1 = hot_map.begin(); iter1 != hot_map.end(); iter1++) {
		uint32_t appid = iter1->first;
		map<string, uint32_t> freq_map = iter1->second;
		map<string, uint32_t>::iterator freq_iter = freq_map.begin();
		for (; freq_iter != freq_map.end(); freq_iter++) {
			if (freq_iter->first == "") {
				continue;
			}
			hot_infile << appid << "\t" << freq_iter->first << "\t" << freq_iter->second << endl;
		}
	}
	hot_infile.close();
	return 0;
}