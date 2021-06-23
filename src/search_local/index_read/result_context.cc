#include "result_context.h"
#include <algorithm>

ResultContext::ResultContext()
    : index_info_vet_()
    , valid_docs_set_()
    , highlight_word_set_()
    , docid_keyinfovet_map_()
    , key_doccount_map_()
{ }

ResultContext::~ResultContext(){

}

void ResultContext::Clear(){
    index_info_vet_.clear();
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

void ResultContext::SetValidDocs(const std::string& valid_docid){
    valid_docs_set_.insert(valid_docid);
}

void ResultContext::SetIndexInfos(int logic_type , std::vector<IndexInfo>& index_info_vet){
    if (ORKEY == logic_type){
        SetOrIndexInfos(index_info_vet);
    }else if(ANDKEY == logic_type){
        SetAndIndexInfos(index_info_vet);
    }else if(INVERTKEY == logic_type){
        SetInvertIndexInfos(index_info_vet);
    }
}

void ResultContext::SetOrIndexInfos(std::vector<IndexInfo>& or_index_info_vet){
    if (index_info_vet_.empty()){
        index_info_vet_ = or_index_info_vet;
    }else{
        std::vector<IndexInfo> index_info_result;
        int i_max_size = index_info_vet_.size() + or_index_info_vet.size();
        index_info_result.resize(i_max_size);

        std::sort(index_info_vet_.begin() , index_info_vet_.end());
        std::sort(or_index_info_vet.begin() , or_index_info_vet.end());

        std::set_union(index_info_vet_.begin(), index_info_vet_.end()
                    ,or_index_info_vet.begin() , or_index_info_vet.end() , index_info_result.begin());

        index_info_vet_.swap(index_info_result);
    }
}

void ResultContext::SetAndIndexInfos(std::vector<IndexInfo>& and_index_info_vet){
    if (index_info_vet_.empty()){
        index_info_vet_ = and_index_info_vet;
    }else{
        std::vector<IndexInfo> index_info_result;
        int i_min_size = (index_info_vet_.size() <= and_index_info_vet.size() ? index_info_vet_.size() : and_index_info_vet.size());
        index_info_result.resize(i_min_size);

        std::sort(index_info_vet_.begin() , index_info_vet_.end());
        std::sort(and_index_info_vet.begin() , and_index_info_vet.end());

        std::set_intersection(index_info_vet_.begin(), index_info_vet_.end()
                    ,and_index_info_vet.begin() , and_index_info_vet.end() , index_info_result.begin());

        index_info_vet_.swap(index_info_result);
    }
}

void ResultContext::SetInvertIndexInfos(std::vector<IndexInfo>& invert_index_info_vet){
    if (index_info_vet_.empty()){
        index_info_vet_ = invert_index_info_vet;
    }else{
        std::vector<IndexInfo> index_info_result;
        int i_max_size = index_info_vet_.size() + invert_index_info_vet.size();
        index_info_result.resize(i_max_size);

        std::sort(index_info_vet_.begin() , index_info_vet_.end());
        std::sort(invert_index_info_vet.begin() , invert_index_info_vet.end());

        std::set_difference(index_info_vet_.begin(), index_info_vet_.end()
                    ,invert_index_info_vet.begin() , invert_index_info_vet.end() , index_info_result.begin());

        index_info_vet_.swap(index_info_result);
    }
}
