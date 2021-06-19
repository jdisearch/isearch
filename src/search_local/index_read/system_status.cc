#include "system_status.h"
#include <algorithm>

ResultContext::ResultContext()
    : valid_docs_set_()
    , highlight_word_set_()
    , docid_keyinfovet_map_()
    , key_doccount_map_()
{ }

ResultContext::~ResultContext(){

}

void ResultContext::Clear(){
    valid_docs_set_.clear();
    highlight_word_set_.clear();
    docid_keyinfovet_map_.clear();
    key_doccount_map_.clear();
}

void ResultContext::SetHighLightWordSet(const std::string& highlight_word){
    highlight_word_set_.insert(highlight_word);
}

void ResultContext::SetDocKeyinfoMap(const std::string& s_doc_id, const KeyInfo& key_info){
    docid_keyinfovet_map_[s_doc_id].push_back(key_info);
}

void ResultContext::SetWordDoccountMap(const std::string& s_word, uint32_t ui_doc_count){
    key_doccount_map_.insert(std::make_pair(s_word , ui_doc_count));
}

void ResultContext::SetValidDocs(int logic_type , const ValidDocSet& valid_docs_set){
    if (ORKEY == logic_type){
        SetOrValidDocs(valid_docs_set);
    }else if(ANDKEY == logic_type){
        SetAndValidDocs(valid_docs_set);
    }else if(INVERTKEY == logic_type){
        SetInvertValidDocs(valid_docs_set);
    }
}

void ResultContext::SetOrValidDocs(const ValidDocSet& or_valid_docs_set){
    if (valid_docs_set_.empty()){
        valid_docs_set_ = or_valid_docs_set;
    }else{
        std::vector<std::string> valid_docs_result;
        int i_max_size = valid_docs_set_.size() + or_valid_docs_set.size();
        valid_docs_result.resize(i_max_size);
        std::set_union(valid_docs_set_.begin(), valid_docs_set_.end()
                    ,or_valid_docs_set.begin() , or_valid_docs_set.end() , valid_docs_result.begin());

        ValidDocSet valid_docs_result_set(valid_docs_result.begin() , valid_docs_result.end());
        valid_docs_set_.swap(valid_docs_result_set);
    }
}

void ResultContext::SetAndValidDocs(const ValidDocSet& and_valid_docs_set){
    if (valid_docs_set_.empty()){
        valid_docs_set_ = and_valid_docs_set;
    }else{
        std::vector<std::string> valid_docs_result;
        int i_min_size = (valid_docs_set_.size() <= and_valid_docs_set.size() ? valid_docs_set_.size() : and_valid_docs_set.size());
        valid_docs_result.resize(i_min_size);
        std::set_intersection(valid_docs_set_.begin(), valid_docs_set_.end()
                    ,and_valid_docs_set.begin() , and_valid_docs_set.end() , valid_docs_result.begin());

        ValidDocSet valid_docs_result_set(valid_docs_result.begin() , valid_docs_result.end());
        valid_docs_set_.swap(valid_docs_result_set);
    }
}

void ResultContext::SetInvertValidDocs(const ValidDocSet& invert_valid_docs_set){
    if (valid_docs_set_.empty()){
        valid_docs_set_ = invert_valid_docs_set;
    }else{
        std::vector<std::string> valid_docs_result;
        int i_max_size = valid_docs_set_.size() + invert_valid_docs_set.size();
        valid_docs_result.resize(i_max_size);
        std::set_difference(valid_docs_set_.begin(), valid_docs_set_.end()
                    ,invert_valid_docs_set.begin() , invert_valid_docs_set.end() , valid_docs_result.begin());

        ValidDocSet valid_docs_result_set(valid_docs_result.begin() , valid_docs_result.end());
        valid_docs_set_.swap(valid_docs_result_set);
    }
}
