#include "query_process.h"
#include <assert.h>
#include "../valid_doc_filter.h"
#include "../order_op.h"
#include "../result_cache.h"
#include "cachelist_unit.h"

extern CCacheListUnit* cachelist;

QueryProcess::QueryProcess(const Json::Value& value)
    : component_(NULL)
    , doc_manager_(NULL)
    , request_(NULL)
    , parse_value_(value)
    , skipList_()
    , response_()
    , valid_docs_()
    , high_light_word_()
    , docid_keyinfovet_map_()
    , key_doccount_map_()
    , sort_field_type_()
{
    skipList_.InitList();
}

QueryProcess::~QueryProcess()
{ }

int QueryProcess::StartQuery(){
    assert(component_ != NULL);
    assert(doc_manager_ != NULL);
    assert(request_ != NULL);

    int iret = ParseContent();
    if (0 == iret){
        iret = GetValidDoc();
        if (0 == iret){
            iret = GetScore();
            if (0 == iret){
                SetResponse();

                Json::FastWriter writer;
                std::string outputConfig = writer.write(response_);
                request_->setResult(outputConfig);
            }
        }
    }
    return iret;
}

void QueryProcess::SetResponse()
{
    log_debug("search result begin.");
    response_["code"] = 0;

    int sequence = -1;
    int rank = 0;
    response_["type"] = 0;
    SortScore(sequence , rank);

    if(!component_->RequiredFields().empty()){
        doc_manager_->AppendFieldsToRes(response_, component_->RequiredFields());
    }

    if (rank > 0){
        AppendHighLightWord();
    }
    response_["count"] = rank;
    log_debug("search result end: %lld.", (long long int)GetSysTimeMicros());
}

int QueryProcess::GetScore()
{
    switch (component_->SortType())
    {
    case SORT_RELEVANCE:
        {
            // 范围查的时候如果不指定排序类型，需要在这里对skipList进行赋值
            if (docid_keyinfovet_map_.empty() && skipList_.GetSize() == 0) {
                std::set<std::string>::iterator iter = valid_docs_.begin();
                for(; iter != valid_docs_.end(); iter++){
                    skipList_.InsertNode(1, (*iter).c_str());
                }
                break;
            }

            std::map<std::string, KeyInfoVet>::iterator docid_keyinfovet_iter = docid_keyinfovet_map_.begin();
            for (; docid_keyinfovet_iter != docid_keyinfovet_map_.end(); ++ docid_keyinfovet_iter){
                std::string doc_id = docid_keyinfovet_iter->first;
                KeyInfoVet& key_info = docid_keyinfovet_iter->second;

                if(valid_docs_.find(doc_id) == valid_docs_.end()){
                    continue;
                }

                double score = 0.0;
                for (uint32_t i = 0; i < key_info.size(); i++) {
                    std::string keyword = key_info[i].word;
                    uint32_t ui_word_freq = key_info[i].word_freq;
                    uint32_t ui_doc_count = key_doccount_map_[keyword];
                    score += log((DOC_CNT - ui_doc_count + 0.5) / (ui_doc_count + 0.5)) * ((D_BM25_K1 + 1)*ui_word_freq)  \
                            / (D_BM25_K + ui_word_freq) * (D_BM25_K2 + 1) * 1 / (D_BM25_K2 + 1);
                }
                skipList_.InsertNode(score, doc_id.c_str());
            }
        }
        break;
    case SORT_TIMESTAMP:
        {
            std::map<std::string, KeyInfoVet>::iterator docid_keyinfovet_iter = docid_keyinfovet_map_.begin();
            for (; docid_keyinfovet_iter != docid_keyinfovet_map_.end(); ++ docid_keyinfovet_iter){
                std::string doc_id = docid_keyinfovet_iter->first;
                KeyInfoVet& key_info = docid_keyinfovet_iter->second;

                if(valid_docs_.find(doc_id) == valid_docs_.end()){
                    continue;
                }

                if (key_info.empty()){
                    continue;
                }

                double score = (double)key_info[0].created_time;
                skipList_.InsertNode(score, doc_id.c_str());
            }
        }
        break;
    case DONT_SORT:
        {
            std::set<std::string>::iterator valid_docs_iter = valid_docs_.begin();
            for(; valid_docs_iter != valid_docs_.end(); valid_docs_iter++){
                std::string doc_id = *valid_docs_iter;
                skipList_.InsertNode(1, doc_id.c_str());
            }
        }
        break;
    case SORT_FIELD_ASC:
    case SORT_FIELD_DESC:
        {
            std::set<std::string>::iterator valid_docs_iter = valid_docs_.begin();
            for(; valid_docs_iter != valid_docs_.end(); valid_docs_iter++){
                std::string doc_id = *valid_docs_iter;
                doc_manager_->GetScoreMap(doc_id, component_->SortType()
                        , component_->SortField(), sort_field_type_);
            }
        }
        break;
    default:
        break;
    }

    return 0;
}

void QueryProcess::SortScore(int& i_sequence , int& i_rank)
{
    if ((SORT_FIELD_DESC == component_->SortType() || SORT_FIELD_ASC == component_->SortType())
        && 0 == skipList_.GetSize()){
        SortByCOrderOp(i_rank);
    }else if(SORT_FIELD_ASC == component_->SortType()){
        SortForwardBySkipList(i_sequence , i_rank);
    }else{
        SortBackwardBySkipList(i_sequence, i_rank);
    }
}

void QueryProcess::SortByCOrderOp(int& i_rank)
{
    OrderOpCond order_op_cond;
    order_op_cond.last_id = component_->LastId();
    order_op_cond.limit_start = component_->PageSize() * (component_->PageIndex()-1);
    order_op_cond.count = component_->PageSize();
    order_op_cond.has_extra_filter = false;
    if(component_->ExtraFilterOrKeys().size() != 0 || component_->ExtraFilterAndKeys().size() != 0 
        || component_->ExtraFilterInvertKeys().size() != 0){
        order_op_cond.has_extra_filter = true;
    }
    if(sort_field_type_ == FIELDTYPE_INT){
        i_rank += doc_manager_->ScoreIntMap().size();
        COrderOp<int> orderOp(FIELDTYPE_INT, component_->SearchAfter(), component_->SortType());
        orderOp.Process(doc_manager_->ScoreIntMap(), atoi(component_->LastScore().c_str()), order_op_cond, response_, doc_manager_);
    } else if(sort_field_type_ == FIELDTYPE_DOUBLE) {
        i_rank += doc_manager_->ScoreDoubleMap().size();
        COrderOp<double> orderOp(FIELDTYPE_DOUBLE, component_->SearchAfter(), component_->SortType());
        orderOp.Process(doc_manager_->ScoreDoubleMap(), atof(component_->LastScore().c_str()), order_op_cond, response_, doc_manager_);
    } else {
        i_rank += doc_manager_->ScoreStrMap().size();
        COrderOp<std::string> orderOp(FIELDTYPE_STRING, component_->SearchAfter(), component_->SortType());
        orderOp.Process(doc_manager_->ScoreStrMap(), component_->LastScore(), order_op_cond, response_, doc_manager_);
    }
}

void QueryProcess::SortForwardBySkipList(int& i_sequence , int& i_rank)
{
    log_debug("m_has_gis, size:%d ", skipList_.GetSize());
    SkipListNode* tmp = skipList_.GetHeader()->level[0].forward;

    int i_limit_start = component_->PageSize() * (component_->PageIndex() - 1);
    int i_limit_end = component_->PageSize() * component_->PageIndex() - 1;

    while (tmp->level[0].forward != NULL) {
        // 通过extra_filter_keys进行额外过滤（针对区分度不高的字段）
        if(doc_manager_->CheckDocByExtraFilterKey(tmp->value) == false){
            log_debug("CheckDocByExtraFilterKey failed, %s", tmp->value);
            tmp = tmp->level[0].forward;
            continue;
        }
        i_sequence ++;
        i_rank ++;
        if(component_->ReturnAll() == 0){
            if (i_sequence < i_limit_start || i_sequence > i_limit_end) {
                tmp = tmp->level[0].forward;
                continue;
            }
        }
        Json::Value doc_info;
        doc_info["doc_id"] = Json::Value(tmp->value);
        doc_info["score"] = Json::Value(tmp->key);
        response_["result"].append(doc_info);
        tmp = tmp->level[0].forward;
    }
}

void QueryProcess::SortBackwardBySkipList(int& i_sequence , int& i_rank)
{
    int i_limit_start = component_->PageSize() * (component_->PageIndex() - 1);
    int i_limit_end = component_->PageSize() * component_->PageIndex() - 1;

    SkipListNode *tmp = skipList_.GetFooter()->backward;
    while(tmp->backward != NULL) {
        if(doc_manager_->CheckDocByExtraFilterKey(tmp->value) == false){
            tmp = tmp->backward;
            continue;
        }
        i_sequence++;
        i_rank++;
        if (component_->ReturnAll() == 0){
            if (i_sequence < i_limit_start || i_sequence > i_limit_end) {
                tmp = tmp->backward;
                continue;
            }
        }
        Json::Value doc_info;
        doc_info["doc_id"] = Json::Value(tmp->value);
        doc_info["score"] = Json::Value(tmp->key);
        response_["result"].append(doc_info);
        tmp = tmp->backward;
    }
}

void QueryProcess::AppendHighLightWord()
{
    int count = 0;
    set<string>::iterator iter = high_light_word_.begin();
    for (; iter != high_light_word_.end(); iter++) {
        if (count >= 10)
            break;
        ++count;
        response_["hlWord"].append((*iter).c_str());
    }
}