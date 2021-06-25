#include "ngram_segment.h"
#include <math.h>

NgramSegment::NgramSegment()
{
}

NgramSegment::~NgramSegment()
{
}

void NgramSegment::ConcreteSplit(const string& str, uint32_t appid, vector<string>& parse_list){
    vector<string> parse_list1;
    vector<string> parse_list2;
    fmm(str, appid, parse_list1);
    bmm(str, appid, parse_list2);
    parse_list1.insert(parse_list1.begin(), "<BEG>");
    parse_list1.push_back("<END>");
    parse_list2.insert(parse_list2.begin(), "<BEG>");
    parse_list2.push_back("<END>");
    
    // CalList1和CalList2分别记录两个句子词序列不同的部分
    vector<string> cal_list1;
    vector<string> cal_list2;
    // pos1和pos2记录两个句子的当前字的位置，cur1和cur2记录两个句子的第几个词
    uint32_t pos1 = 0;
    uint32_t pos2 = 0;
    uint32_t cur1 = 0;
    uint32_t cur2 = 0;
    while (1) {
        if (cur1 == parse_list1.size() && cur2 == parse_list2.size()) {
            break;
        }
        // 如果当前位置一样
        if (pos1 == pos2) {
            // 当前位置一样，并且词也一样
            if (parse_list1[cur1].size() == parse_list2[cur2].size()) {
                pos1 += parse_list1[cur1].size();
                pos2 += parse_list2[cur2].size();
                // 说明此时得到两个不同的词序列，根据bigram选择概率大的
                // 注意算不同的时候要考虑加上前面一个词和后面一个词，拼接的时候再去掉即可
                if (cal_list1.size() > 0) {
                    cal_list1.insert(cal_list1.begin(), parse_list[parse_list.size() - 1]);
                    cal_list2.insert(cal_list2.begin(), parse_list[parse_list.size() - 1]);
                    if (cur1 < parse_list1.size()-1) {
                        cal_list1.push_back(parse_list1[cur1]);
                        cal_list2.push_back(parse_list2[cur2]);
                    }
                    double p1 = calSegProbability(cal_list1);
                    double p2 = calSegProbability(cal_list2);

                    vector<string> cal_list = (p1 > p2) ? cal_list1 : cal_list2;
                    cal_list.erase(cal_list.begin());
                    if (cur1 < parse_list1.size() - 1) {
                        cal_list.pop_back();
                    }
                    parse_list.insert(parse_list.end(), cal_list.begin(), cal_list.end());
                    cal_list1.clear();
                    cal_list2.clear();
                }
                parse_list.push_back(parse_list1[cur1]);
                cur1++;
                cur2++;
            }
            // pos相同，len(ParseList1[cur1])不同，向后滑动，不同的添加到list中
            else if (parse_list1[cur1].size() > parse_list2[cur2].size()) {
                cal_list2.push_back(parse_list2[cur2]);
                pos2 += parse_list2[cur2].size();
                cur2++;
            }
            else {
                cal_list1.push_back(parse_list1[cur1]);
                pos1 += parse_list1[cur1].size();
                cur1++;
            }
        }
        else { 
            // pos不同，而结束的位置相同，两个同时向后滑动
            if (pos1 + parse_list1[cur1].size() == pos2 + parse_list2[cur2].size()) {
                cal_list1.push_back(parse_list1[cur1]);
                cal_list2.push_back(parse_list2[cur2]);
                pos1 += parse_list1[cur1].size();
                pos2 += parse_list2[cur2].size();
                cur1++;
                cur2++;
            }
            else if (pos1 + parse_list1[cur1].size() > pos2 + parse_list2[cur2].size()) {
                cal_list2.push_back(parse_list2[cur2]);
                pos2 += parse_list2[cur2].size();
                cur2++;
            }
            else {
                cal_list1.push_back(parse_list1[cur1]);
                pos1 += parse_list1[cur1].size();
                cur1++;
            }
        }
    }
    parse_list.erase(parse_list.begin());
    parse_list.pop_back();
    return;
}

void NgramSegment::fmm(const string& str, uint32_t appid, vector<string>& fmm_list) {
	iutf8string phrase(str);
    int maxlen = MAX_WORD_LEN;
    int len_phrase = phrase.length();
    int i = 0, j = 0;

    while (i < len_phrase) {
        int end = i + maxlen;
        if (end >= len_phrase)
            end = len_phrase;
        iutf8string phrase_sub = phrase.utf8substr(i, end - i);
        for (j = phrase_sub.length(); j >= 0; j--) {
            if (j == 1)
                break;
            iutf8string key = phrase_sub.utf8substr(0, j);
            if (wordValid(key.stlstring(), appid) == true) {
                fmm_list.push_back(key.stlstring());
                i += key.length() - 1;
                break;
            }
        }
        if (j == 1) {
            fmm_list.push_back(phrase_sub[0]);
        }
        i += 1;
    }
    return;
}

void NgramSegment::bmm(const string& str, uint32_t appid, vector<string>& bmm_list) {
	iutf8string phrase(str);
    int maxlen = MAX_WORD_LEN;
    int len_phrase = phrase.length();
    int i = len_phrase, j = 0;

    while (i > 0) {
        int start = i - maxlen;
        if (start < 0)
            start = 0;
        iutf8string phrase_sub = phrase.utf8substr(start, i-start);
        for (j = 0; j < phrase_sub.length(); j++) {
            if (j == phrase_sub.length() - 1)
                break;
            iutf8string key = phrase_sub.utf8substr(j, phrase_sub.length()-j);
            if (wordValid(key.stlstring(), appid) == true) {
                vector<string>::iterator iter = bmm_list.begin();
                bmm_list.insert(iter, key.stlstring());
                i -= key.length() - 1;
                break;
            }
        }
        if (j == phrase_sub.length() - 1) {
            vector<string>::iterator iter = bmm_list.begin();
            bmm_list.insert(iter, "" + phrase_sub[j]);
        }
        i -= 1;
    }
    return;
}

double NgramSegment::calSegProbability(const vector<string>& vec) {
    double p = 0;
    string word1;
    string word2;
    // 由于概率很小，对连乘做了取对数处理转化为加法
    for (int pos = 0; pos < (int)vec.size(); pos++) {
        if (pos != (int)vec.size() - 1) {
            // 乘以后面词的条件概率
            word1 = vec[pos];
            word2 = vec[pos + 1];
            if (hmm_manager_->NextDict().find(word1) == hmm_manager_->NextDict().end()) {
                // 加1平滑
                p += log(1.0 / hmm_manager_->TrainCnt());
            }
            else {
                double numerator = 1.0;
                uint32_t denominator = hmm_manager_->TrainCnt();
                map<string, int>::iterator iter = hmm_manager_->NextDict()[word1].begin();
                for (; iter != hmm_manager_->NextDict()[word1].end(); iter++) {
                    if (iter->first == word2) {
                        numerator += iter->second;
                    }
                    denominator += iter->second;
                }
                p += log(numerator / denominator);
            }
        }
        // 乘以第一个词的概率
        if ((pos == 0 && vec[0] != "<BEG>") || (pos == 1 && vec[0] == "<BEG>")) {
            uint32_t word_freq = 0;
            WordInfo word_info;
            if (getWordInfo(vec[pos], 0, word_info)) {
                word_freq = word_info.word_freq;
            }
            p += log(word_freq + 1.0 / hmm_manager_->NextDict().size() + hmm_manager_->TrainCnt());
        }
    }

    return p;
}

bool NgramSegment::getWordInfo(string word, uint32_t appid, WordInfo& word_info) {
   if (word_dict_.find(word) != word_dict_.end()) {
       map<uint32_t, WordInfo> wordInfo = word_dict_[word];
       if (wordInfo.find(0) != wordInfo.end()) {
           word_info = wordInfo[0];
           return true;
       }
       if (wordInfo.find(appid) != wordInfo.end()) {
           word_info = wordInfo[appid];
           return true;
       }
   }

   return false;
}