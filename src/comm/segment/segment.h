/*
 * =====================================================================================
 *
 *       Filename:  segment.h
 *
 *    Description:  segment class definition.
 *
 *        Version:  1.0
 *        Created:  08/06/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __SEGMENT_H__
#define __SEGMENT_H__

#include "hmm_manager.h"
#include "../utf8_str.h"
#define MAX_WORD_LEN 8
#define TOTAL 8000000

struct WordInfo {
    WordInfo() {
        word_id = 0;
        word_freq = 0;
        appid = 0;
    }
    uint32_t word_id;
    uint32_t word_freq;
    uint32_t appid;
};

class Segment{
public:
    Segment();
    virtual ~Segment();
    virtual bool Init(string word_path, string train_path);
    void CutForSearch(const string& str, uint32_t appid, vector<vector<string> >& search_res_all);
    void CutNgram(const string& str, vector<string>& search_res, uint32_t n);
    void Split(const string& str, uint32_t appid, vector<string>& vec, bool hmm_flag = false);
    bool WordValid(string word, uint32_t appid);
    bool GetWordInfo(string word, uint32_t appid, WordInfo &word_info);
    virtual void ConcreteSplit(const string& str, uint32_t appid, vector<string>& vec) = 0;

protected:
    bool isAllAlphaOrDigit(string str);
    bool isAlphaOrDigit(string str);
    void dealByHmmMgr(uint32_t appid, const vector<string>& old_vec, vector<string>& new_vec);

protected:
    map<string, map<uint32_t, WordInfo> > word_dict_;
    set<string> punct_set_;
    set<string> alpha_set_;
    HmmManager* hmm_manager_;
};

#endif