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
#include "../trainCorpus.h"
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
    vector<char> viterbi(string sentence);
private:
    uint32_t train_cnt_;
    TrainCorpus* train_corpus_;
    map<string, map<string, int> > next_dict_;
};


#endif
