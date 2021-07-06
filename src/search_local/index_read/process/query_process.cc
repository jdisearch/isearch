#include "query_process.h"
#include <assert.h>
#include "../valid_doc_filter.h"
#include "../order_op.h"

QueryProcess::QueryProcess(const Json::Value& value)
    : component_(NULL)
    , doc_manager_(NULL)
    , request_(NULL)
    , p_valid_docs_set_(ResultContext::Instance()->GetValidDocs())
    , parse_value_(value)
    , scoredocid_set_()
    , response_()
    , sort_field_type_()
{ }

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
            iret = CheckValidDoc();
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
    }
    return iret;
}

int QueryProcess::CheckValidDoc(){
    bool bRet = doc_manager_->GetDocContent();
    if (false == bRet){
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }
    return 0;
}

int QueryProcess::GetScore()
{
    switch (component_->SortType())
    {
    case SORT_RELEVANCE: // 按照相关度得分，并以此排序
        {
            // 范围查的时候如果不指定排序类型，需要在这里对skipList进行赋值
            const DocKeyinfosMap& docid_keyinfovet_map = ResultContext::Instance()->GetDocKeyinfosMap();
            if (docid_keyinfovet_map.empty() && scoredocid_set_.empty()) {
                std::set<std::string>::iterator iter = p_valid_docs_set_->begin();
                for(; iter != p_valid_docs_set_->end(); iter++){
                    scoredocid_set_.insert(ScoreDocIdNode(1,*iter));
                }
                break;
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
        break;
    case SORT_TIMESTAMP: // 按照时间戳得分，并以此排序
        {
            DocKeyinfosMap docid_keyinfovet_map = ResultContext::Instance()->GetDocKeyinfosMap();
            std::map<std::string, KeyInfoVet>::iterator docid_keyinfovet_iter = docid_keyinfovet_map.begin();
            for (; docid_keyinfovet_iter != docid_keyinfovet_map.end(); ++ docid_keyinfovet_iter){
                std::string doc_id = docid_keyinfovet_iter->first;
                KeyInfoVet& key_info = docid_keyinfovet_iter->second;

                if(p_valid_docs_set_->find(doc_id) == p_valid_docs_set_->end()){
                    continue;
                }

                if (key_info.empty()){
                    continue;
                }

                double score = (double)key_info[0].created_time;
                scoredocid_set_.insert(ScoreDocIdNode(score , doc_id));
            }
        }
        break;
    case DONT_SORT: // 不排序，docid有序
        {
            std::set<std::string>::iterator valid_docs_iter = p_valid_docs_set_->begin();
            for(; valid_docs_iter != p_valid_docs_set_->end(); valid_docs_iter++){
                std::string doc_id = *valid_docs_iter;
                scoredocid_set_.insert(ScoreDocIdNode(1 , doc_id));
            }
        }
        break;
    case SORT_FIELD_ASC: // 按照指定字段进行升降排序
    case SORT_FIELD_DESC:
        {
            std::set<std::string>::iterator valid_docs_iter = p_valid_docs_set_->begin();
            for(; valid_docs_iter != p_valid_docs_set_->end(); valid_docs_iter++){
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
        && scoredocid_set_.empty()){
        SortByCOrderOp(i_rank);
    }else if(SORT_FIELD_ASC == component_->SortType()){ 
        AscSort(i_sequence, i_rank);
    }else{ // 不指定情况下，默认降序，分高的在前,时间新的在前,docid大的在前（地理位置查询除外）
        DescSort(i_sequence, i_rank);
    }
}

const Json::Value& QueryProcess::SetResponse()
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
    return response_;
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

void QueryProcess::AscSort(int& i_sequence , int& i_rank)
{
    log_debug("m_has_gis, size:%d ", (uint32_t)scoredocid_set_.size());
    int i_limit_start = component_->PageSize() * (component_->PageIndex() - 1);
    int i_limit_end = component_->PageSize() * component_->PageIndex() - 1;

    std::set<ScoreDocIdNode>::iterator iter = scoredocid_set_.begin();
    for( ;iter != scoredocid_set_.end(); ++iter){
        // 通过extra_filter_keys进行额外过滤（针对区分度不高的字段）
        if(doc_manager_->CheckDocByExtraFilterKey(iter->s_docid) == false){
            log_debug("CheckDocByExtraFilterKey failed, %s", iter->s_docid.c_str());
            continue;
        }
        i_sequence ++;
        i_rank ++;
        if(component_->ReturnAll() == 0){
            if (i_sequence < i_limit_start || i_sequence > i_limit_end) {
                continue;
            }
        }
        Json::Value doc_info;
        doc_info["doc_id"] = Json::Value(iter->s_docid);
        doc_info["score"] = Json::Value(iter->d_score);
        response_["result"].append(doc_info);
    }
}

void QueryProcess::DescSort(int& i_sequence , int& i_rank)
{
    int i_limit_start = component_->PageSize() * (component_->PageIndex() - 1);
    int i_limit_end = component_->PageSize() * component_->PageIndex() - 1;

    std::set<ScoreDocIdNode>::reverse_iterator riter = scoredocid_set_.rbegin();
    for( ;riter != scoredocid_set_.rend(); ++riter){
        if(doc_manager_->CheckDocByExtraFilterKey(riter->s_docid) == false){
            continue;
        }
        i_sequence++;
        i_rank++;
        if (component_->ReturnAll() == 0){
            if (i_sequence < i_limit_start || i_sequence > i_limit_end) {
                continue;
            }
        }
        Json::Value doc_info;
        doc_info["doc_id"] = Json::Value(riter->s_docid);
        doc_info["score"] = Json::Value(riter->d_score);
        response_["result"].append(doc_info);
    }
}

void QueryProcess::AppendHighLightWord()
{
    int count = 0;
    const HighLightWordSet& highlight_word_set = ResultContext::Instance()->GetHighLightWordSet();
    std::set<std::string>::const_iterator iter = highlight_word_set.cbegin();
    for (; iter != highlight_word_set.cend(); iter++) {
        if (count >= 10)
            break;
        ++count;
        response_["hlWord"].append((*iter).c_str());
    }
}