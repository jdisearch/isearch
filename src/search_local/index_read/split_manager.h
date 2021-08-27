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
#include "bmm_segment.h"
#include "search_util.h"
using namespace std;


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

    bool Init(const string& word_path, const string& train_path, const string& split_mode);
    void DeInit();
    string split(string str, uint32_t appid);
    bool WordValid(string word, uint32_t appid) {
        return segment_->WordValid(word, appid);
    }
    bool GetWordInfo(string word, uint32_t appid, WordInfo &word_info) {
        return segment_->GetWordInfo(word, appid, word_info);
    }

private:
    Segment* segment_;
    string split_mode_;
};

#endif