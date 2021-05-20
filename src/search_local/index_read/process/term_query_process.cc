#include "term_query_process.h"
#include <sstream>
#include "../order_op.h"

TermQueryProcess::TermQueryProcess(uint32_t appid, Json::Value& value, Component* component)
:QueryProcess(appid, value, component){
    sort_type_ = component_->SortType();
    sort_field_ = component_->SortField();
    has_gis_ = false;
}

TermQueryProcess::~TermQueryProcess(){

}

int TermQueryProcess::ParseContent(){
    return ParseContent(ORKEY);
}

int TermQueryProcess::ParseContent(uint32_t type){
    vector<FieldInfo> field_info_vec;
    Json::Value::Members member = value_.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    string field_name;
    Json::Value field_value;
    if(iter != member.end()){ // 一个term下只对应一个字段
        field_name = *iter;
        field_value = value_[field_name];
    } else {
        SetErrMsg("TermQueryProcess error, value is null");
        return -RT_PARSE_CONTENT_ERROR;
    }
    uint32_t segment_tag = 0;
    FieldInfo field_info;
    uint32_t field = DBManager::Instance()->GetWordField(segment_tag, appid_, field_name, field_info);
    if(field != 0){
        field_info.word = field_value.asString();
        field_info_vec.push_back(field_info);
    } else {
        stringstream ss_msg;
        ss_msg << "field_name[" << field_name << "] error, not in the app_field_define";
        SetErrMsg(ss_msg.str());
        return -RT_PARSE_CONTENT_ERROR;
    }
    component_->AddToFieldList(type, field_info_vec);
    return 0;
}


int TermQueryProcess::GetValidDoc(){
    doc_manager_ = new DocManager(component_);
    logical_operate_ = new LogicalOperate(appid_, sort_type_, has_gis_, component_->CacheSwitch());

    for (size_t index = 0; index < component_->Keys().size(); index++)
    {
        vector<IndexInfo> doc_id_vec;
        vector<FieldInfo> field_info_vec = component_->Keys()[index];
        vector<FieldInfo>::iterator it;
        for (it = field_info_vec.begin(); it != field_info_vec.end(); it++) {
            vector<IndexInfo> doc_info;
            int ret = logical_operate_->GetDocIdSetByWord(*it, doc_info);
            if (ret != 0){
                return -RT_GET_DOC_ERR;
            }
            highlightWord_.insert((*it).word);
            if(sort_type_ == SORT_RELEVANCE){
                logical_operate_->CalculateByWord(*it, doc_info, doc_info_map_, key_in_doc_);
            }
            doc_id_vec = vec_union(doc_id_vec, doc_info);
        }
        doc_vec_ = vec_union(doc_vec_, doc_id_vec);
    }

    bool bRet = doc_manager_->GetDocContent(has_gis_, doc_vec_, valid_docs_, distances_);
    if (false == bRet) {
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }
    return 0;
}


int TermQueryProcess::GetScoreAndSort(){
    set<string>::iterator set_iter = valid_docs_.begin();
    for(; set_iter != valid_docs_.end(); set_iter++){
        string doc_id = *set_iter;

        if (sort_type_ == SORT_FIELD_ASC || sort_type_ == SORT_FIELD_DESC){
            doc_manager_->GetScoreMap(doc_id, sort_type_, sort_field_, sort_field_type_, appid_);
        } else {
            skipList_.InsertNode(1, doc_id.c_str());
        }
    }
    return 0;
}

void TermQueryProcess::TaskEnd(){
    Json::FastWriter writer;
    Json::Value response;
    response["code"] = 0;
    int sequence = -1;
    int rank = 0;
    int page_size = component_->PageSize();
    int limit_start = page_size * (component_->PageIndex()-1);
    int limit_end = page_size * (component_->PageIndex()-1) + page_size - 1;

    log_debug("search result begin.");

    if((sort_type_ == SORT_FIELD_DESC || sort_type_ == SORT_FIELD_ASC) && skipList_.GetSize() == 0){
        OrderOpCond order_op_cond;
        order_op_cond.last_id = component_->LastId();
        order_op_cond.limit_start = limit_start;
        order_op_cond.count = page_size;
        order_op_cond.has_extra_filter = false;
        if(sort_field_type_ == FIELDTYPE_INT){
            rank += doc_manager_->ScoreIntMap().size();
            COrderOp<int> orderOp(FIELDTYPE_INT, component_->SearchAfter(), sort_type_);
            orderOp.Process(doc_manager_->ScoreIntMap(), atoi(component_->LastScore().c_str()), order_op_cond, response, doc_manager_);
        } else if(sort_field_type_ == FIELDTYPE_DOUBLE) {
            rank += doc_manager_->ScoreDoubleMap().size();
            COrderOp<double> orderOp(FIELDTYPE_DOUBLE, component_->SearchAfter(), sort_type_);
            orderOp.Process(doc_manager_->ScoreDoubleMap(), atof(component_->LastScore().c_str()), order_op_cond, response, doc_manager_);
        } else {
            rank += doc_manager_->ScoreStrMap().size();
            COrderOp<string> orderOp(FIELDTYPE_STRING, component_->SearchAfter(), sort_type_);
            orderOp.Process(doc_manager_->ScoreStrMap(), component_->LastScore(), order_op_cond, response, doc_manager_);
        }
    } else {
        SkipListNode *tmp = skipList_.GetFooter()->backward;
        while(tmp->backward != NULL) {
            sequence++;
            rank++;
            if (component_->ReturnAll() == 0){
                if (sequence < limit_start || sequence > limit_end) {
                    tmp = tmp->backward;
                    continue;
                }
            }
            Json::Value doc_info;
            doc_info["doc_id"] = Json::Value(tmp->value);
            doc_info["score"] = Json::Value(tmp->key);
            response["result"].append(doc_info);
            tmp = tmp->backward;
        }
    }

    if(component_->Fields().size() > 0){
        doc_manager_->AppendFieldsToRes(response, component_->Fields());
    }

    if (rank > 0){
        AppendHighLightWord(response);
    }

    response["type"] = 0;
    response["count"] = rank;
    log_debug("search result end: %lld.", (long long int)GetSysTimeMicros());
    std::string outputConfig = writer.write(response);
    request_->setResult(outputConfig);
}

void TermQueryProcess::AppendHighLightWord(Json::Value& response){
    int count = 0;
    set<string>::iterator iter = highlightWord_.begin();
    for (; iter != highlightWord_.end(); iter++) {
        if (count >= 10){
            break;
        }
        count = count + 1;
        response["hlWord"].append((*iter).c_str());
    }
    return ;
}