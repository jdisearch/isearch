#include "hmm_manager.h"

#include <fstream>
#include "../log.h"
#include "../utf8_str.h"

HmmManager::HmmManager(){
    train_corpus_ = new TrainCorpus();
}

HmmManager::~HmmManager(){
    if(NULL != train_corpus_){
        delete train_corpus_;
    }
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
    }
    train_infile.close();

    bool ret = train_corpus_->Init(train_path);
    if (ret == false) {
        log_error("train_corpus init error.");
        return ret;
    }
    log_info("total training words length is: %u, next_dict count: %d.", train_cnt_, (int)next_dict_.size());
    
    return true;
}

void HmmManager::HmmSplit(string str, vector<string>& vec){
    vector<char> pos_list = viterbi(str);
    string result;
    iutf8string utf8_str(str);
    for (size_t i = 0; i < pos_list.size(); i++) {
        result += utf8_str[i];
        if (pos_list[i] == 'E') {
            std::size_t found = result.find_last_of(" ");
            string new_word = result.substr(found + 1);
        }
        if (pos_list[i] == 'E' || pos_list[i] == 'S') {
            result += ' ';
        }
    }
    if (result[result.size()-1] == ' ') {
        result = result.substr(0, result.size() - 1);
    }

    vec = splitEx(result, " ");
}

vector<char> HmmManager::viterbi(string sentence) {
    iutf8string utf8_str(sentence);
    vector<map<char, double> > viterbi_vec;
    map<char, vector<char> > path;
    char states[4] = { 'B','M','E','S' };
    map<char, double> prob_map;
    for (size_t i = 0; i < sizeof(states); i++) {
        char y = states[i];
        double emit_value = train_corpus_->MinEmit();
        if (train_corpus_->emit_dict[y].find(utf8_str[0]) != train_corpus_->emit_dict[y].end()) {
            emit_value = train_corpus_->emit_dict[y].at(utf8_str[0]);
        }
        prob_map[y] = train_corpus_->start_dict[y] * emit_value;  // 在位置0，以y状态为末尾的状态序列的最大概率
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
                //cout << j << " " << y0 << " " << y << " " << V[j - 1][y0] << " " << train_corpus.trans_dict[y0][y] << " " << train_corpus.emit_dict[y].at(utf8_str[j]) << endl;
                double emit_value = train_corpus_->MinEmit();
                if (train_corpus_->emit_dict[y].find(utf8_str[j]) != train_corpus_->emit_dict[y].end()) {
                    emit_value = train_corpus_->emit_dict[y].at(utf8_str[j]);
                }
                double prob = viterbi_vec[j - 1][y0] * train_corpus_->trans_dict[y0][y] * emit_value;
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
    return path[state];
}

uint32_t HmmManager::TrainCnt(){
    return train_cnt_;
}

map<string, map<string, int> >& HmmManager::NextDict(){
    return next_dict_;
}