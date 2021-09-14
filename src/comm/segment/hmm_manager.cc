#include "hmm_manager.h"

#include <fstream>
#include "../log.h"
#include "../utf8_str.h"

HmmManager::HmmManager(){
    min_emit_ = 1.0;
    char state[4] = { 'B','M','E','S' };
    vector<char> state_list(state, state + 4);
    for (size_t i = 0; i < state_list.size(); i++) {
        map<char, double> trans_map;
        trans_dict_.insert(make_pair(state_list[i], trans_map));
        for (size_t j = 0; j < state_list.size(); j++) {
            trans_dict_[state_list[i]].insert(make_pair(state_list[j], 0.0));
        }
    }
    for (size_t i = 0; i < state_list.size(); i++) {
        map<string, double> emit_map;
        emit_dict_.insert(make_pair(state_list[i], emit_map));
        start_dict_.insert(make_pair(state_list[i],0.0));
        count_dict_.insert(make_pair(state_list[i], 0));
    }
    line_num_ = 0;
}

HmmManager::~HmmManager(){
}

bool HmmManager::Init(string train_path, const set<string>& punct_set) {
    string str;
    ifstream train_infile;
    train_infile.open(train_path.c_str());
    if (train_infile.is_open() == false) {
        log_error("open file error: %s.\n", train_path.c_str());
        return false;
    }
    string beg_tag = "<BEG>";
    string end_tag = "<END>";
    while (getline(train_infile, str))
    {
        vector<string> str_vec = splitEx(str, " ");
        vector<string> line_list;
        vector<string>::iterator iter = str_vec.begin();
        for (; iter != str_vec.end(); iter++) {
            if (punct_set.find(*iter) == punct_set.end() && *iter != "") {
                line_list.push_back(*iter);
            }
        }
        if(line_list.size() == 0){
            continue;
        }
        train_cnt_ += line_list.size();
        for (int i = -1; i < (int)line_list.size(); i++) {
            string word1;
            string word2;
            if (i == -1) {
                word1 = beg_tag;
                word2 = line_list[i + 1];
            }
            else if (i == (int)line_list.size() - 1) {
                word1 = line_list[i];
                word2 = end_tag;
            }
            else {
                word1 = line_list[i];
                word2 = line_list[i + 1];
            }
            if (next_dict_.find(word1) == next_dict_.end()) {
                map<string, int> dict;
                next_dict_[word1] = dict;
            }
            if (next_dict_[word1].find(word2) == next_dict_[word1].end()) {
                next_dict_[word1][word2] = 1;
            }
            else {
                next_dict_[word1][word2] += 1;
            }
        }

        line_num_++;
        vector<string> word_list; // 保存除空格以外的字符
        iutf8string utf8_str(str);
        for (int i = 0; i < utf8_str.length(); i++) {
            if (utf8_str[i] != " ") {
                word_list.push_back(utf8_str[i]);
            }
        }
        vector<char> line_state;
        for (size_t i = 0; i < str_vec.size(); i++) {
            if (str_vec[i] == "") {
                continue;
            }
            iutf8string utf8_str_item(str_vec[i]);
            vector<char> item_state;
            getList(utf8_str_item.length(), item_state);
            line_state.insert(line_state.end(), item_state.begin(), item_state.end());
        }
        if (word_list.size() != line_state.size()) {
            log_error("[line = %s]\n", str.c_str());
        }
        else {
            for (size_t i = 0; i < line_state.size(); i++) {
                if (i == 0) {
                    start_dict_[line_state[0]] += 1;  // 记录句子第一个字的状态，用于计算初始状态概率
                    count_dict_[line_state[0]] ++;  // 记录每个状态的出现次数
                }
                else {
                    trans_dict_[line_state[i - 1]][line_state[i]] += 1;
                    count_dict_[line_state[i]] ++;
                    if (emit_dict_[line_state[i]].find(word_list[i]) == emit_dict_[line_state[i]].end()) {
                        emit_dict_[line_state[i]].insert(make_pair(word_list[i], 0.0));
                    }
                    else {
                        emit_dict_[line_state[i]][word_list[i]] += 1;
                    }
                }
            }
        }
    }
    train_infile.close();
    log_info("total training words length is: %u, next_dict count: %d.", train_cnt_, (int)next_dict_.size());
    map<char, double>::iterator start_iter = start_dict_.begin();
    for (; start_iter != start_dict_.end(); start_iter++) {  // 状态的初始概率
        start_dict_[start_iter->first] = start_dict_[start_iter->first] * 1.0 / line_num_;
    }

    map<char, map<char, double> >::iterator trans_iter = trans_dict_.begin();
    for (; trans_iter != trans_dict_.end(); trans_iter++) {  // 状态转移概率
        map<char, double> trans_map = trans_iter->second;
        map<char, double>::iterator trans_iter2 = trans_map.begin();
        for (; trans_iter2 != trans_map.end(); trans_iter2++) {
            trans_dict_[trans_iter->first][trans_iter2->first] = trans_dict_[trans_iter->first][trans_iter2->first] / count_dict_[trans_iter->first];
        }
    }

    map<char, map<string, double> >::iterator emit_iter = emit_dict_.begin();
    for (; emit_iter != emit_dict_.end(); emit_iter++) {  // 发射概率(状态->词语的条件概率)
        map<string, double> emit_map = emit_iter->second;
        map<string, double>::iterator emit_iter2 = emit_map.begin();
        for (; emit_iter2 != emit_map.end(); emit_iter2++) {
            double emit_value = emit_dict_[emit_iter->first][emit_iter2->first] / count_dict_[emit_iter->first];
            if (emit_value < min_emit_ && emit_value != 0.0) {
                min_emit_ = emit_value;
            }
            emit_dict_[emit_iter->first][emit_iter2->first] = emit_value;
        }
    }
    
    return true;
}

void HmmManager::HmmSplit(string str, vector<string>& vec){
    vector<char> pos_list;
    viterbi(str, pos_list);
    string result;
    iutf8string utf8_str(str);
    for (size_t i = 0; i < pos_list.size(); i++) {
        result += utf8_str[i];
        if (pos_list[i] == 'E' || pos_list[i] == 'S') {
            result += ' ';
        }
    }
    if (result[result.size()-1] == ' ') {
        result = result.substr(0, result.size() - 1);
    }

    vec = splitEx(result, " ");
}

void HmmManager::viterbi(string sentence, vector<char>& output) {
    iutf8string utf8_str(sentence);
    vector<map<char, double> > viterbi_vec;
    map<char, vector<char> > path;
    char states[4] = { 'B','M','E','S' };
    map<char, double> prob_map;
    for (size_t i = 0; i < sizeof(states); i++) {
        char y = states[i];
        double emit_value = min_emit_;
        if (emit_dict_[y].find(utf8_str[0]) != emit_dict_[y].end()) {
            emit_value = emit_dict_[y].at(utf8_str[0]);
        }
        prob_map[y] = start_dict_[y] * emit_value;  // 在位置0，以y状态为末尾的状态序列的最大概率
        path[y].push_back(y);
    }
    viterbi_vec.push_back(prob_map);
    for (int j = 1; j < utf8_str.length(); j++) {
        map<char, vector<char> > new_path;
        prob_map.clear();
        for (size_t k = 0; k < sizeof(states); k++) {
            char y = states[k];
            double max_prob = 0.0;
            char state = ' ';
            for (size_t m = 0; m < sizeof(states); m++) {
                char y0 = states[m];  // 从y0 -> y状态的递归
                //cout << j << " " << y0 << " " << y << " " << V[j - 1][y0] << " " << trans_dict_[y0][y] << " " << emit_dict_[y].at(utf8_str[j]) << endl;
                double emit_value = min_emit_;
                if (emit_dict_[y].find(utf8_str[j]) != emit_dict_[y].end()) {
                    emit_value = emit_dict_[y].at(utf8_str[j]);
                }
                double prob = viterbi_vec[j - 1][y0] * trans_dict_[y0][y] * emit_value;
                if (prob > max_prob) {
                    max_prob = prob;
                    state = y0;
                }
            }
            prob_map[y] = max_prob;
            new_path[y] = path[state];
            new_path[y].push_back(y);
        }
        viterbi_vec.push_back(prob_map);
        path = new_path;
    }
    double max_prob = 0.0;
    char state = ' ';
    for (size_t i = 0; i < sizeof(states); i++) {
        char y = states[i];
        if (viterbi_vec[utf8_str.length() - 1][y] > max_prob) {
            max_prob = viterbi_vec[utf8_str.length() - 1][y];
            state = y;
        }
    }
    output.assign(path[state].begin(), path[state].end());
}

void HmmManager::getList(uint32_t str_len, vector<char>& output) {
    if (str_len == 1) {
        output.push_back('S');
    }
    else if (str_len == 2) {
        output.push_back('B');
        output.push_back('E');
    }
    else {
        vector<char> M_list(str_len - 2, 'M');
        output.push_back('B');
        output.insert(output.end(), M_list.begin(), M_list.end());
        output.push_back('E');
    }
}

uint32_t HmmManager::TrainCnt(){
    return train_cnt_;
}

map<string, map<string, int> >& HmmManager::NextDict(){
    return next_dict_;
}