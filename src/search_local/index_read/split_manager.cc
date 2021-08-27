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

#include <string>
#include <sstream>
#include <algorithm>
#include <fstream>

#include "log.h"
#include "comm.h"
#include "fmm_segment.h"
#include "dag_segment.h"
#include "ngram_segment.h"
#include "custom_segment.h"
using namespace std;

SplitManager::SplitManager() {
}

SplitManager::~SplitManager() {
    if(segment_ != NULL){
        delete segment_;
    }
}

bool SplitManager::Init(const string& word_path, const string& train_path, const string& split_mode) {
    split_mode_ = split_mode;
    if(split_mode_ == "Post"){
        segment_ = new BmmSegment();
    } else if(split_mode_ == "Pre"){
        segment_ = new FmmSegment();
    } else if(split_mode_ == "DAG"){
        segment_ = new DagSegment();
    } else if(split_mode_ == "Ngram"){
        segment_ = new NgramSegment();
    } else if(split_mode_ == "Custom"){
        segment_ = new CustomSegment();
    } else {
        log_debug("split_mode_ not supported, change to default: Post");
        segment_ = new BmmSegment();
    }

    bool ret = segment_->Init(word_path, train_path);
    if(false == ret){
        log_error("segment_ Init failed.");
        return false;
    }

    return true;
}

void SplitManager::DeInit() {
}

string SplitManager::split(string str, uint32_t appid) {
    iutf8string test(str);
    vector<string> res_all;
    log_debug("split_mode: %s", split_mode_.c_str());
    segment_->Split(str, appid, res_all);

    ostringstream oss;
    for (vector<string>::iterator iter = res_all.begin(); iter != res_all.end(); iter++) {
        oss << (*iter) << "|";
    }
    return oss.str();
}


