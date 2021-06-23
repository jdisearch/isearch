/*
 * =====================================================================================
 *
 *       Filename:  hmm_manager.h
 *
 *    Description:  hmm manager class definition.
 *
 *        Version:  1.0
 *        Created:  09/06/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __HMM_MANAGER_H__
#define __HMM_MANAGER_H__

#include <string>
#include <set>
#include <vector>
#include <map>
#include <stdint.h>
using namespace std;

class HmmManager{
public:
    HmmManager();
    ~HmmManager();
    bool Init(string train_path, const set<string>& punct_set);
    void HmmSplit(string str, vector<string>& vec);
    map<string, map<string, int> >& NextDict();
    uint32_t TrainCnt();
private:
    void viterbi(string sentence, vector<char>& output);
    void getList(uint32_t str_len, vector<char>& output);
private:
    uint32_t train_cnt_;
    map<string, map<string, int> > next_dict_;

    map<char, map<char, double> > trans_dict_;
    map<char, map<string, double> > emit_dict_;
    map<char, double> start_dict_;
    map<char, uint32_t> count_dict_;
    uint32_t line_num_;
    double min_emit_;
};


#endif
