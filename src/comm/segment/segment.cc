#include "segment.h"

#include <fstream>
#include <stdlib.h>

#include "../log.h"

#define ALPHA_DIGIT "0123456789１２３４５６７８９０\
abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZａｂｃｄｅｆｇｈｉｇｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＵＶＷＸＹＺ"

Segment::Segment(){
    hmm_manager_ = new HmmManager();
}

Segment::~Segment(){
    if(NULL != hmm_manager_){
        delete hmm_manager_;
    }
}

bool Segment::isAlphaOrDigit(string str) {
    if (alpha_set_.find(str) != alpha_set_.end()){
        return true;
    }
    return false;
}

bool Segment::wordValid(string word, uint32_t appid) {
    if(punct_set_.find(word) != punct_set_.end()){
        return false;
    }
    if (word_dict_.find(word) != word_dict_.end()) {
        map<uint32_t, WordInfo> wordInfo = word_dict_[word];
        if (wordInfo.find(0) != wordInfo.end() || wordInfo.find(appid) != wordInfo.end()) {
            return true;
        }
    }

    return false;
}

bool Segment::Init(string word_path, string train_path){
    string en_punct = ",.!?/'\"<>\\:;\n";
    string punct = "，。！？、；：“”‘’（）《》 ";
    punct = punct.append(en_punct);
    iutf8string utf8_punct(punct);
    for (int i = 0; i < utf8_punct.length(); i++) {
        punct_set_.insert(utf8_punct[i]);
    }

    string alphadigit = ALPHA_DIGIT;
    iutf8string utf8_alpha(alphadigit);
    for (int i = 0; i < utf8_alpha.length(); i++) {
        alpha_set_.insert(utf8_alpha[i]);
    }

    string str;
    ifstream word_infile;
    word_infile.open(word_path.c_str());
    if (word_infile.is_open() == false) {
        log_error("open file error: %s.\n", word_path.c_str());
        return false;
    }

    uint32_t word_id = 0;
    uint32_t appid = 0;
    string word;
    uint32_t word_freq = 0;
    while (getline(word_infile, str))
    {
        vector<string> str_vec = splitEx(str, "\t");
        word_id = atoi(str_vec[0].c_str());
        word = str_vec[1];
        appid = atoi(str_vec[2].c_str());
        word_freq = atoi(str_vec[3].c_str());
        WordInfo word_info;
        word_info.appid = appid;
        word_info.word_freq = word_freq;
        word_info.word_id = word_id;
        word_dict_[word][appid] = word_info;
    }
    log_info("word_dict count: %d", (int)word_dict_.size());

    bool ret = hmm_manager_->Init(train_path, punct_set_);
    if(false == ret){
        log_error("hmm_manager_ init error: %s.\n", train_path.c_str());
        return false;
    }
    return true;
}

void Segment::Split(iutf8string& phrase, uint32_t appid, vector<string>& new_res_all, bool hmm_flag){
    vector<string> sen_list;
    set<string> special_set;  // 记录英文和数字字符串
    string tmp_words = "";
    bool flag = false; // 记录是否有英文或者数字的flag
    for (int i = 0; i < phrase.length(); i++) {  // 对句子进行预处理：去掉标点、提取出英文或数字，只对连续的汉字进行中文分词
        if (isAlphaOrDigit(phrase[i])) {
            if (tmp_words != "" && flag == false) {
                sen_list.push_back(tmp_words);
                tmp_words = "";
            }
            flag = true;
            tmp_words += phrase[i];
        }
        else if(punct_set_.find(phrase[i]) != punct_set_.end()){
            if (tmp_words != "") {
                sen_list.push_back(tmp_words);
                sen_list.push_back(phrase[i]);
                if (flag == true) {
                    special_set.insert(tmp_words);
                    flag = false;
                }
                tmp_words = "";
            }
        }
        else {
            if (flag == true) {
                sen_list.push_back(tmp_words);
                special_set.insert(tmp_words);
                flag = false;
                tmp_words = phrase[i];
            }
            else {
                tmp_words += phrase[i];
            }
        }
    }
    if (tmp_words != "") {
        sen_list.push_back(tmp_words);
        if (flag == true) {
            special_set.insert(tmp_words);
        }
    }
    tmp_words = "";
    vector<string> res_all;
    for (int i = 0; i < (int)sen_list.size(); i++) {
        // special_set中保存了连续的字母数字串，不需要进行分词
        if (special_set.find(sen_list[i]) == special_set.end() && punct_set_.find(sen_list[i]) == punct_set_.end()) {
            iutf8string utf8_str(sen_list[i]);
            vector<string> parse_list;
            ConcreteSplit(utf8_str, appid, parse_list);
            res_all.insert(res_all.end(), parse_list.begin(), parse_list.end());
        }else { // 英文或数字需要放入到res_all，标点符号不需要
            if(punct_set_.find(sen_list[i]) == punct_set_.end()){
                res_all.push_back(sen_list[i]);
            }
        }
    }

    if (hmm_flag == false) {
        new_res_all.assign(res_all.begin(), res_all.end());
    } else {
        // 使用HMM发现新词
        dealByHmmMgr(appid, res_all, new_res_all);
    }

    return;
}

void Segment::dealByHmmMgr(uint32_t appid, const vector<string>& res_all, vector<string>& new_res_all){
    string buf = "";
    for (size_t i = 0; i < res_all.size(); i++) {
        iutf8string utf8_str(res_all[i]);
        if (utf8_str.length() == 1 && punct_set_.find(res_all[i]) == punct_set_.end() && res_all[i].length() > 1) { // 确保res_all[i]是汉字
            buf += res_all[i];
        }
        else {
            if (buf.length() > 0) {
                iutf8string utf8_buf(buf);
                if (utf8_buf.length() == 1) {
                    new_res_all.push_back(buf);
                }
                else if (wordValid(buf, appid) == false) { // 连续的单字组合起来，使用HMM算法进行分词
                    vector<string> vec;
                    hmm_manager_->HmmSplit(buf, vec);
                    new_res_all.insert(new_res_all.end(), vec.begin(), vec.end());
                }
            }
            buf = "";
            new_res_all.push_back(res_all[i]);
        }
    }

    if (buf.length() > 0) {
        iutf8string utf8_buf(buf);
        if (utf8_buf.length() == 1) {
            new_res_all.push_back(buf);
        }
        else if (wordValid(buf, appid) == false) { // 连续的单字组合起来，使用HMM算法进行分词
            vector<string> vec;
            hmm_manager_->HmmSplit(buf, vec);
            new_res_all.insert(new_res_all.end(), vec.begin(), vec.end());
        }
        buf = "";
    }
}

void Segment::CutForSearch(iutf8string& phrase, uint32_t appid, vector<vector<string> >& search_res_all) {
    // 搜索引擎模式
    vector<string> new_res_all;
    Split(phrase, appid, new_res_all);
    for (size_t i = 0; i < new_res_all.size(); i++) {
        vector<string> vec;
        iutf8string utf8_str(new_res_all[i]);
        if (utf8_str.length() > 2 && isAllAlphaOrDigit(new_res_all[i]) == false) {
            for (int j = 0; j < utf8_str.length() - 1; j++) {
                string tmp_str = utf8_str.substr(j, 2);
                if (wordValid(tmp_str, appid) == true) {
                    vec.push_back(tmp_str);
                }
            }
        }
        if (utf8_str.length() > 3 && isAllAlphaOrDigit(new_res_all[i]) == false) {
            for (int j = 0; j < utf8_str.length() - 2; j++) {
                string tmp_str = utf8_str.substr(j, 3);
                if (wordValid(tmp_str, appid) == true) {
                    vec.push_back(tmp_str);
                }
            }
        }
        vec.push_back(new_res_all[i]);
        search_res_all.push_back(vec);
    }

    return;
}

bool Segment::isAllAlphaOrDigit(string str) {
    bool flag = true;
    size_t i = 0;
    for (; i < str.size(); i++) {
        if (!isupper(str[i]) && !islower(str[i]) && !isdigit(str[i])) {
            flag = false;
            break;
        }
    }
    return flag;
}

void Segment::CutNgram(iutf8string& phrase, vector<string>& search_res, uint32_t n) {
    uint32_t N = (n > (uint32_t)phrase.length()) ? (uint32_t)phrase.length() : n;
    for (size_t i = 1; i <= N; i++) {
        for (size_t j = 0; j < (size_t)phrase.length() - i + 1; j++) {
            string tmp_str = phrase.substr(j, i);
            search_res.push_back(tmp_str);
        }
    }
}