#include "sort_operator_base.h"

SortOperatorBase::SortOperatorBase(RequestContext* request_cnt , DocManager* doc_manager)
    : component_(request_cnt)
    , doc_manager_(doc_manager)
    , p_valid_docs_set_(ResultContext::Instance()->GetValidDocs())
    , scoredocid_set_()
    , sort_field_type_()
{

}

SortOperatorBase::~SortOperatorBase()
{

}

std::set<ScoreDocIdNode>* SortOperatorBase::GetSortOperator(uint32_t ui_sort_type)
{
    log_debug("GetSortOperator beginning...");
    switch (ui_sort_type)
    {
    case SORT_RELEVANCE:{
            RelevanceSort();
        }
        break;
    case DONT_SORT: {
            NoneSort();
        }
        break;
    case SORT_FIELD_ASC:
    case SORT_FIELD_DESC:
        {
            AssignFieldSort();
        }
        break;
    default:
        break;
    }

    return (&scoredocid_set_);
}

void SortOperatorBase::RelevanceSort()
{
    // 按照相关度得分，并以此排序
    log_debug("relevance score sort type");
    // 范围查的时候如果不指定排序类型，需要在这里对skipList进行赋值
    const DocKeyinfosMap& docid_keyinfovet_map = ResultContext::Instance()->GetDocKeyinfosMap();
    if (docid_keyinfovet_map.empty() && scoredocid_set_.empty()) {
        std::set<std::string>::iterator iter = p_valid_docs_set_->begin();
        for(; iter != p_valid_docs_set_->end(); iter++){
            scoredocid_set_.insert(ScoreDocIdNode(1,*iter));
        }
        return;
    }

    std::map<std::string, KeyInfoVet>::const_iterator docid_keyinfovet_iter = docid_keyinfovet_map.cbegin();
    for (; docid_keyinfovet_iter != docid_keyinfovet_map.cend(); ++ docid_keyinfovet_iter){
        std::string doc_id = docid_keyinfovet_iter->first;
        const KeyInfoVet& key_info = docid_keyinfovet_iter->second;

        if(p_valid_docs_set_->find(doc_id) == p_valid_docs_set_->end()){
            continue;
        }

        double score = 0.0;
        for (uint32_t i = 0; i < key_info.size(); i++) {
            std::string keyword = key_info[i].word;
            uint32_t ui_word_freq = key_info[i].word_freq;
            uint32_t ui_doc_count = ResultContext::Instance()->GetKeywordDoccountMap(keyword);
            score += log((DOC_CNT - ui_doc_count + 0.5) / (ui_doc_count + 0.5)) * ((D_BM25_K1 + 1)*ui_word_freq)  \
                    / (D_BM25_K + ui_word_freq) * (D_BM25_K2 + 1) * 1 / (D_BM25_K2 + 1);
            log_debug("loop score[%d]:%f", i , score);
        }
        scoredocid_set_.insert(ScoreDocIdNode(score , doc_id));
    }
}

void SortOperatorBase::NoneSort()
{
	log_debug("no sort type");
    std::set<std::string>::iterator valid_docs_iter = p_valid_docs_set_->begin();
    for(; valid_docs_iter != p_valid_docs_set_->end(); valid_docs_iter++){
        std::string doc_id = *valid_docs_iter;
        scoredocid_set_.insert(ScoreDocIdNode(1 , doc_id));
    }
}

void SortOperatorBase::AssignFieldSort()
{
	std::set<std::string>::iterator valid_docs_iter = p_valid_docs_set_->begin();
    for(; valid_docs_iter != p_valid_docs_set_->end(); valid_docs_iter++){
        std::string doc_id = *valid_docs_iter;
        doc_manager_->GetScoreMap(doc_id, component_->SortType()
                , component_->SortField(), sort_field_type_);
    }
    log_debug("assign field sort type , order option:%d" , (int)sort_field_type_);
}