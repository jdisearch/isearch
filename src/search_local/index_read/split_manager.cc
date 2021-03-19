/*
 * =====================================================================================
 *
 *       Filename:  split_manager.h
 *
 *    Description:  split manager class definition.
 *
 *        Version:  1.0
 *        Created:  28/03/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "split_manager.h"
#include "log.h"
#include "comm.h"
#include <string>
#include <sstream>
#include <algorithm>
#include <fstream>
using namespace std;

typedef pair<string, int> PAIR;
struct CmpByValue {
	bool operator()(const PAIR& lhs, const PAIR& rhs) {
		return lhs.second > rhs.second;
	}
};

SplitManager::SplitManager() {
	en_word_map.clear();
	word_map.clear();
	letters.clear();
}

SplitManager::~SplitManager() {
}

bool SplitManager::Init(const SGlobalConfig &global_cfg) {
	bool ret = seg.Init3(global_cfg.sTrainPath, global_cfg.sWordsPath);
	if (ret == false) {
		log_error("seg init error.");
		return false;
	}
	split_mode = global_cfg.sSplitMode;

	string in_file = global_cfg.sEnWordsPath;
	string str;
	ifstream infile;
	infile.open(in_file.c_str());
	if (infile.is_open() == false) {
		printf("open file error: %s.\n", in_file.c_str());
		return false;
	}

	string word;
	int freq = 0;
	while (getline(infile, str))
	{
		vector<string> str_vec = splitEx(str, "\t");
		if (str_vec.size() == 3) {
			word = str_vec[1];
			freq = atoi(str_vec[2].c_str());
			en_word_map[word] = freq;
		}
	}
	infile.close();

	string in_letter = global_cfg.sCharacterPath;
	ifstream infile_letter;
	infile_letter.open(in_letter.c_str());
	if (infile_letter.is_open() == false) {
		printf("open file error: %s.\n", in_letter.c_str());
		return false;
	}
	while (getline(infile_letter, str))
	{
		vector<string> str_vec = splitEx(str, "\t");
		if (str_vec.size() == 2) {
			word = str_vec[1];
			letters.push_back(word);
		}
	}
	infile_letter.close();

	return true;
}


void SplitManager::DeInit() {
	en_word_map.clear();
	word_map.clear();
	letters.clear();
}

string SplitManager::split(string str, uint32_t appid) {
	iutf8string test(str);
	vector<string> res_all;
	log_debug("split_mode: %s", split_mode.c_str());
	seg.segment2(test, appid, res_all, split_mode);

	ostringstream oss;
	for (vector<string>::iterator iter = res_all.begin(); iter != res_all.end(); iter++) {
		oss << (*iter) << "|";
	}
	return oss.str();
}

set<string> SplitManager::edits1(string word) {
	set<string> word_set;
	string letters = "abcdefghijklmnopqrstuvwxyz";
	for (int i = 0; i < (int)word.length(); i++) {
		string left = word.substr(0, i);
		string right = word.substr(i, word.length() - i);

		string deletes = left + right.substr(1, right.length() - 1);
		word_set.insert(deletes);

		if (right.length() > 1) {
			string transposes = left + right[1] + right[0] + right.substr(2, right.length() - 2);
			word_set.insert(transposes);
		}
		if (right.length() > 0) {
			for (size_t j = 0; j < letters.length(); j++) {
				string replaces = left + letters[j] + right.substr(1, right.length() - 1);
				word_set.insert(replaces);
			}
		}
		for (size_t j = 0; j < letters.length(); j++) {
			string inserts = left + letters[j] + right;
			word_set.insert(inserts);
		}
	}

	return word_set;
}

set<string> SplitManager::known(const set<string> &words) {
	set<string> word_set;
	set<string>::const_iterator iter = words.begin();
	for (; iter != words.end(); iter++) {
		if (en_word_map.find(*iter) != en_word_map.end()) {
			word_set.insert(*iter);
		}
	}

	return word_set;
}

string SplitManager::correction(string word) {
	if (en_word_map.find(word) != en_word_map.end()) {
		return word;
	}
	else {
		set<string> edits11 = edits1(word);
		set<string> candidates = known(edits11);
		map<string, int> candidate_map;
		for (set<string>::iterator iter = candidates.begin(); iter != candidates.end(); iter++) {
			int freq = en_word_map[*iter];
			candidate_map[*iter] = freq;
		}
		if (candidate_map.size() > 0) {
			vector<PAIR> res_vec(candidate_map.begin(), candidate_map.end());
			sort(res_vec.begin(), res_vec.end(), CmpByValue());
			PAIR tmp = res_vec[0];
			return tmp.first;
		}
		else {
			return "";
		}
	}
}

set<string> SplitManager::ch_edits1(string word) {
	set<string> word_set;
	iutf8string utf8_str(word);
	for (int i = 0; i < utf8_str.length(); i++) {
		string left = utf8_str.substr(0, i);
		iutf8string right = utf8_str.utf8substr(i, utf8_str.length() - i);

		string deletes = left + right.substr(1, right.length() - 1);
		word_set.insert(deletes);

		if (right.length() > 1) {
			string transposes = left + right[1] + right[0] + right.substr(2, right.length() - 2);
			word_set.insert(transposes);
		}
		if (right.length() > 0) {
			for (size_t j = 0; j < letters.size(); j++) {
				string replaces = left + letters[j] + right.substr(1, right.length() - 1);
				word_set.insert(replaces);
			}
		}
		for (size_t j = 0; j < letters.size(); j++) {
			string inserts = left + letters[j] + right.stlstring();
			word_set.insert(inserts);
		}
	}

	return word_set;
}

set<string> SplitManager::ch_known(const set<string> &words) {
	set<string> word_set;
	set<string>::const_iterator iter = words.begin();
	for (; iter != words.end(); iter++) {
		if (word_map.find(*iter) != word_map.end()) {
			word_set.insert(*iter);
		}
	}

	return word_set;
}

string SplitManager::ch_correction(string word) {
	if (word_map.find(word) != word_map.end()) {
		return word;
	}
	else {
		set<string> edits11 = ch_edits1(word);
		set<string> candidates = ch_known(edits11);
		map<string, int> candidate_map;
		for (set<string>::iterator iter = candidates.begin(); iter != candidates.end(); iter++) {
			int freq = word_map[*iter];
			candidate_map[*iter] = freq;
		}
		if (candidate_map.size() > 0) {
			vector<PAIR> res_vec(candidate_map.begin(), candidate_map.end());
			sort(res_vec.begin(), res_vec.end(), CmpByValue());
			PAIR tmp = res_vec[0];
			return tmp.first;
		}
		else {
			return "";
		}
	}
}

