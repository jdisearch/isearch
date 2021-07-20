#include "term_query_process.h"

TermQueryProcess::TermQueryProcess(const Json::Value& value)
    : QueryProcess(value)
{}

TermQueryProcess::~TermQueryProcess(){

}

int TermQueryProcess::ParseContent(){
    return ParseContent(ORKEY);
}

int TermQueryProcess::ParseContent(int logic_type){
    std::vector<FieldInfo> field_info_vec;
    Json::Value::Members member = parse_value_.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    std::string field_name;
    Json::Value field_value;
    if(iter != member.end()){ // 一个term下只对应一个字段
        field_name = *iter;
        field_value = parse_value_[field_name];
    } else {
        log_error("TermQueryProcess error, value is null");
        return -RT_PARSE_CONTENT_ERROR;
    }
    uint32_t segment_tag = 0;
    FieldInfo field_info;
    field_info.query_type = E_INDEX_READ_TERM;

    uint32_t uiRet = DBManager::Instance()->GetWordField(segment_tag, component_->Appid()
                    , field_name, field_info);
    if(uiRet != 0 && field_info.index_tag == 0){
        ExtraFilterKey extra_filter_key;
        extra_filter_key.field_name = field_name;
        extra_filter_key.field_value = field_value.asString();
        extra_filter_key.field_type = field_info.field_type;
        component_->AddToExtraFieldList(logic_type , extra_filter_key);
    } else if(uiRet != 0){
        field_info.word = field_value.asString();
        field_info_vec.push_back(field_info);
    } else {
        log_error("field_name:%s error, not in the app_field_define", field_name.c_str());
        return -RT_PARSE_CONTENT_ERROR;
    }
    component_->AddToFieldList(logic_type, field_info_vec);
    return 0;
}

int TermQueryProcess::GetValidDoc(){
    if (component_->GetFieldList(ORKEY).empty()){
        return -RT_GET_FIELD_ERROR;
    }
    
    return GetValidDoc(ORKEY , component_->GetFieldList(ORKEY)[FIRST_TEST_INDEX]);
}

int TermQueryProcess::GetValidDoc(int logic_type, const std::vector<FieldInfo>& keys){
    log_debug("term query GetValidDoc beginning...");
    if (0 == keys[FIRST_SPLIT_WORD_INDEX].index_tag){
        return -RT_GET_FIELD_ERROR;
    }
    
    std::vector<IndexInfo> index_info_vet;
    int iret = ValidDocFilter::Instance()->TextInvertIndexSearch(keys, index_info_vet);
    if (iret != 0) { return iret; }
    ResultContext::Instance()->SetIndexInfos(logic_type , index_info_vet);
    return 0;
}