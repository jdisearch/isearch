#include "correction.h"
#include <fstream>
#include <algorithm>
#include "../split_manager.h"

typedef pair<string, int> PAIR;
struct CmpByValue {
    bool operator()(const PAIR& lhs, const PAIR& rhs) {
        return lhs.second > rhs.second;
    }
};

Correction::Correction(){

}

Correction::~Correction(){

}

bool Correction::Init(const string& en_word_path, const string& character_path){
    string str;
    ifstream infile;
    infile.open(en_word_path.c_str());
    if (infile.is_open() == false) {
        printf("open file error: %s.\n", en_word_path.c_str());
        return false;
    }

    string word;
    int freq = 0;
    while (getline(infile, str)){
        vector<string> str_vec = splitEx(str, "\t");
        if (str_vec.size() == 3) {
            word = str_vec[1];
            freq = atoi(str_vec[2].c_str());
            en_word_map_[word] = freq;
        }
    }
    infile.close();

    ifstream infile_letter;
    infile_letter.open(character_path.c_str());
    if (infile_letter.is_open() == false) {
        printf("open file error: %s.\n", character_path.c_str());
        return false;
    }
    while (getline(infile_letter, str))
    {
        vector<string> str_vec = splitEx(str, "\t");
        if (str_vec.size() == 2) {
            word = str_vec[1];
            characters_.push_back(word);
        }
    }
    infile_letter.close();

    return true;
}

set<string> Correction::Edits1(const string& word) {
    set<string> word_set;
    string letters = "abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < (int)word.length(); i++) {
        string left = word.substr(0, i);
        string right = word.substr(i, word.length() - i);

        string deletes = left + right.substr(1, right.length() - 1);
        word_set.insert(deletes);

        if (right.length() > 1) {
            string transposes = left + right[1] + right[0] + right.substr(2, right.length() - 2);
            word_set.insert(transposes);
        }
        if (right.length() > 0) {
            for (size_t j = 0; j < letters.length(); j++) {
                string replaces = left + letters[j] + right.substr(1, right.length() - 1);
                word_set.insert(replaces);
            }
        }
        for (size_t j = 0; j < letters.length(); j++) {
            string inserts = left + letters[j] + right;
            word_set.insert(inserts);
        }
    }

    return word_set;
}

set<string> Correction::Known(const set<string>& words) {
    set<string> word_set;
    set<string>::const_iterator iter = words.begin();
    for (; iter != words.end(); iter++) {
        if (en_word_map_.find(*iter) != en_word_map_.end()) {
            word_set.insert(*iter);
        }
    }

    return word_set;
}

string Correction::Correct(const string& word) {
    if (en_word_map_.find(word) != en_word_map_.end()) {
        return word;
    }
    else {
        set<string> edits11 = Edits1(word);
        set<string> candidates = Known(edits11);
        map<string, int> candidate_map;
        for (set<string>::iterator iter = candidates.begin(); iter != candidates.end(); iter++) {
            int freq = en_word_map_[*iter];
            candidate_map[*iter] = freq;
        }
        if (candidate_map.size() > 0) {
            vector<PAIR> res_vec(candidate_map.begin(), candidate_map.end());
            sort(res_vec.begin(), res_vec.end(), CmpByValue());
            PAIR tmp = res_vec[0];
            return tmp.first;
        }
        else {
            return "";
        }
    }
}

set<string> Correction::ChEdits1(const string& word) {
    set<string> word_set;
    iutf8string utf8_str(word);
    for (int i = 0; i < utf8_str.length(); i++) {
        string left = utf8_str.substr(0, i);
        iutf8string right = utf8_str.utf8substr(i, utf8_str.length() - i);

        string deletes = left + right.substr(1, right.length() - 1);
        word_set.insert(deletes);

        if (right.length() > 1) {
            string transposes = left + right[1] + right[0] + right.substr(2, right.length() - 2);
            word_set.insert(transposes);
        }
        if (right.length() > 0) {
            for (size_t j = 0; j < characters_.size(); j++) {
                string replaces = left + characters_[j] + right.substr(1, right.length() - 1);
                word_set.insert(replaces);
            }
        }
        for (size_t j = 0; j < characters_.size(); j++) {
            string inserts = left + characters_[j] + right.stlstring();
            word_set.insert(inserts);
        }
    }

    return word_set;
}

set<string> Correction::ChKnown(const set<string>& words) {
    set<string> word_set;
    set<string>::const_iterator iter = words.begin();
    for (; iter != words.end(); iter++) {
        if (SplitManager::Instance()->WordValid(*iter, 0)) {
            word_set.insert(*iter);
        }
    }

    return word_set;
}

string Correction::ChCorrect(const string& word) {
    if (SplitManager::Instance()->WordValid(word, 0)) {
        return word;
    }
    else {
        set<string> edits11 = ChEdits1(word);
        set<string> candidates = ChKnown(edits11);
        map<string, int> candidate_map;
        for (set<string>::iterator iter = candidates.begin(); iter != candidates.end(); iter++) {
            WordInfo word_info;
            SplitManager::Instance()->GetWordInfo(*iter, 0, word_info);
            int freq = word_info.word_freq;
            candidate_map[*iter] = freq;
        }
        if (candidate_map.size() > 0) {
            vector<PAIR> res_vec(candidate_map.begin(), candidate_map.end());
            sort(res_vec.begin(), res_vec.end(), CmpByValue());
            PAIR tmp = res_vec[0];
            return tmp.first;
        }
        else {
            return "";
        }
    }
}

bool Correction::IsChineseWord(uint32_t appid, const string& str) {
    WordInfo word_info;
    if (SplitManager::Instance()->GetWordInfo(str, appid, word_info) == true) {
        uint32_t word_id = word_info.word_id;
        if (word_id < 100000000) {
            return true;
        }
    }
    return false;
}

bool Correction::IsEnWord(const string& str) {
    return en_word_map_.find(str) != en_word_map_.end();
}

int Correction::JudgeWord(uint32_t appid, const string& word, bool& is_correct, string& probably_word){
    is_correct = true;
    probably_word = "";
    if (IsChineseWord(appid, word)) {
        log_debug("correct Chinese word");
        return 0;
    }
    if (IsEnWord(word)) {
        log_debug("correct English word");
        return 0;
    }
    int data_type = judgeDataType(word);
    log_debug("data_type: %d.", data_type);
    is_correct = false;
    vector<string> word_str_vec;
    int ret = GetSuggestWord(word, word_str_vec, 1);
    if (ret != 0) {
        log_error("GetSuggestWord error.");
        return -RT_DB_ERR;
    }

    if (word_str_vec.size() > 0) {
        probably_word = word_str_vec[0];
    }
    else {
        if (data_type == DATA_PHONETIC) {
            ret = GetEnSuggestWord(word, word_str_vec, 1);
            if (ret != 0) {
                log_error("GetEnSuggestWord error.");
                return -RT_GET_SUGGEST_ERR;
            }
            if (word_str_vec.size() > 0)
                probably_word = word_str_vec[0];
            else {
                probably_word = Correct(word);
            }
        }
        else if (data_type == DATA_CHINESE) {
            vector<string> keys;
            string split_data = SplitManager::Instance()->split(word, 0);
            log_debug("split_data: %s", split_data.c_str());
            keys = splitEx(split_data, "|");
            size_t single = 0;
            vector<string>::const_iterator iter = keys.begin();
            for (; iter != keys.end(); iter++) {
                if ((*iter).length() == 3) {
                    single++;
                }
            }
            if (single > keys.size() - 1) { // if all are single Chinese characters
                string new_word = "";
                Convert2Phonetic(word, new_word);
                word_str_vec.clear();
                ret = GetSuggestWord(new_word, word_str_vec, 1);
                if (ret == 0) {
                    if (word_str_vec.size() > 0) {
                        probably_word = word_str_vec[0];
                    }
                    else {
                        probably_word = ChCorrect(word);
                    }
                }
                else {
                    log_error("GetSuggestWord error.");
                    return -RT_GET_SUGGEST_ERR;
                }
            }
        }
    }
    log_debug("is_correct: %d, probably_word: %s", is_correct, probably_word.c_str());
    return 0;
}