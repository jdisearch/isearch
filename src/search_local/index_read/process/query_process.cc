#include "query_process.h"
#include <assert.h>
#include "../valid_doc_filter.h"
#include "../order_op.h"

QueryProcess::QueryProcess(const Json::Value& value)
    : component_(NULL)
    , doc_manager_(NULL)
    , request_(NULL)
    , sort_operator_base_(NULL)
    , p_scoredocid_set_(NULL)
    , parse_value_(value)
    , response_()
{ }

QueryProcess::~QueryProcess()
{ 
    DELETE(sort_operator_base_);
    ResultContext::Instance()->Clear();
}

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
    log_debug("query base CheckValidDoc beginning...");
    bool bRet = doc_manager_->GetDocContent();
    if (false == bRet){
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }
    return 0;
}

int QueryProcess::GetScore()
{
    log_debug("query base GetScore beginning...");
    sort_operator_base_ = new SortOperatorBase(component_ , doc_manager_);
    p_scoredocid_set_ = sort_operator_base_->GetSortOperator((uint32_t)component_->SortType());
    return 0;
}

void QueryProcess::SortScore(int& i_sequence , int& i_rank)
{
    log_debug("query base sortscore beginning...");
    if ((SORT_FIELD_DESC == component_->SortType() || SORT_FIELD_ASC == component_->SortType())
        && p_scoredocid_set_->empty()){
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
    log_debug("query base SortByCOrderOp beginning...");
    OrderOpCond order_op_cond;
    order_op_cond.last_id = component_->LastId();
    order_op_cond.limit_start = component_->PageSize() * (component_->PageIndex()-1);
    order_op_cond.count = component_->PageSize();
    order_op_cond.has_extra_filter = false;
    if(component_->ExtraFilterOrKeys().size() != 0 || component_->ExtraFilterAndKeys().size() != 0 
        || component_->ExtraFilterInvertKeys().size() != 0){
        order_op_cond.has_extra_filter = true;
    }
    if(FIELDTYPE_INT == sort_operator_base_->GetSortFieldType()){
        i_rank += doc_manager_->ScoreIntMap().size();
        COrderOp<int> orderOp(FIELDTYPE_INT, component_->SearchAfter(), component_->SortType());
        orderOp.Process(doc_manager_->ScoreIntMap(), atoi(component_->LastScore().c_str()), order_op_cond, response_, doc_manager_);
    } else if(FIELDTYPE_DOUBLE == sort_operator_base_->GetSortFieldType()) {
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
    log_debug("ascsort, result size:%d ", (uint32_t)p_scoredocid_set_->size());
    int i_limit_start = component_->PageSize() * (component_->PageIndex() - 1);
    int i_limit_end = component_->PageSize() * component_->PageIndex() - 1;

    std::set<ScoreDocIdNode>::iterator iter = p_scoredocid_set_->begin();
    for( ;iter != p_scoredocid_set_->end(); ++iter){
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
    log_debug("descsort, result size:%d ", (uint32_t)p_scoredocid_set_->size());
    int i_limit_start = component_->PageSize() * (component_->PageIndex() - 1);
    int i_limit_end = component_->PageSize() * component_->PageIndex() - 1;
    log_debug("limit_start:%d , limit_end:%d", i_limit_start, i_limit_end);

    std::set<ScoreDocIdNode>::reverse_iterator riter = p_scoredocid_set_->rbegin();
    for( ;riter != p_scoredocid_set_->rend(); ++riter){
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