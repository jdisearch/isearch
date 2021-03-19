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

#ifndef __SPLIT_MANAGER_H__
#define __SPLIT_MANAGER_H__
#include <string>
#include "split_tool.h"
#include "search_util.h"
#include "search_conf.h"
using namespace std;


class SplitManager {
public:
	SplitManager();
	~SplitManager();
	static SplitManager *Instance()
	{
		return CSingleton<SplitManager>::Instance();
	}

	static void Destroy()
	{
		CSingleton<SplitManager>::Destroy();
	}

	bool Init(const SGlobalConfig &global_cfg);
	void DeInit();
	string split(string str, uint32_t appid);
	string correction(string word);
	set<string> known(const set<string> &words);
	set<string> edits1(string word);
	string ch_correction(string word);
	set<string> ch_known(const set<string> &words);
	set<string> ch_edits1(string word);
	bool WordValid(string word, uint32_t appid) {
		return seg.WordValid(word, appid);
	}
	bool GetWordInfo(string word, uint32_t appid, WordInfo &word_info) {
		return seg.GetWordInfo(word, appid, word_info);
	}

private:
	FBSegment seg;
	map<string, int> en_word_map;
	map<string, int> word_map;
	vector<string> letters;
	string split_mode;
};

#endif