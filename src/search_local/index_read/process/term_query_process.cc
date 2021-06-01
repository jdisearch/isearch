#include "term_query_process.h"

TermQueryProcess::TermQueryProcess(Json::Value& value)
    : QueryProcess(value)
{}

TermQueryProcess::~TermQueryProcess(){

}

int TermQueryProcess::ParseContent(){
    ParseContent(ORKEY);
}

int TermQueryProcess::ParseContent(int logic_type){
    std::vector<FieldInfo> field_info_vec;
    Json::Value::Members member = parse_value_.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    std::string field_name;
    Json::Value field_value;
    if(iter != member.end()){ // 一个term下只对应一个字段
        field_name = *iter;
        field_value = value_[field_name];
    } else {
        log_error("TermQueryProcess error, value is null");
        return -RT_PARSE_CONTENT_ERROR;
    }
    uint32_t segment_tag = 0;
    FieldInfo field_info;
    uint32_t uiRet = DBManager::Instance()->GetWordField(segment_tag, component_->Appid()
                    , field_name, field_info);
    if(uiRet != 0){
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
    std::vector<IndexInfo> index_info_vet;
    int iret = ValidDocFilter::Instance()->MixTextInvertIndexSearch(component_->OrKeys()
                , index_info_vet , high_light_word_, docid_keyinfovet_map_ , key_doccount_map_);
    if (iret != 0) { return iret; }

    bool bRet = doc_manager_->GetDocContent(index_info_vet , valid_docs_);
    if (false == bRet){
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }
    return 0;
}