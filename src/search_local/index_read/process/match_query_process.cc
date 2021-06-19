#include "match_query_process.h"
#include "math.h"
#include "../order_op.h"
#include "../valid_doc_filter.h"

#define DOC_CNT 10000

MatchQueryProcess::MatchQueryProcess(const Json::Value& value)
    : QueryProcess(value)
{ }

MatchQueryProcess::~MatchQueryProcess()
{ }

int MatchQueryProcess::ParseContent(){
    return ParseContent(ORKEY);
}

int MatchQueryProcess::ParseContent(int logic_type){
    vector<FieldInfo> fieldInfos;
    Json::Value::Members member = parse_value_.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    std::string fieldname;
    Json::Value field_value;
    if(iter != member.end()){ // 一个match下只对应一个字段
        fieldname = *iter;
        field_value = parse_value_[fieldname];
    } else {
        log_error("MatchQueryProcess error, value is null");
        return -RT_PARSE_CONTENT_ERROR;
    }
    FieldInfo fieldInfo;
    fieldInfo.query_type = E_INDEX_READ_MATCH;

    uint32_t segment_tag = SEGMENT_NONE;
    uint32_t uiRet = DBManager::Instance()->GetWordField(segment_tag, component_->Appid()
            , fieldname, fieldInfo);
    if (uiRet != 0 && SEGMENT_DEFAULT == segment_tag)
    {
        std::string split_data = SplitManager::Instance()->split(field_value.asString(), component_->Appid());
        log_debug("split_data: %s", split_data.c_str());
        std::vector<std::string> split_datas = splitEx(split_data, "|");
        for(size_t index = 0; index < split_datas.size(); index++){
            fieldInfo.word = split_datas[index];
            fieldInfos.push_back(fieldInfo);
        }
    }
    else if (uiRet != 0)
    {
        fieldInfo.word = field_value.asString();
        fieldInfos.push_back(fieldInfo);
    }else{
        log_error("field_name:%s error, not in the app_field_define", fieldname.c_str());
        return -RT_PARSE_CONTENT_ERROR;
    }

    component_->AddToFieldList(logic_type, fieldInfos);
    return 0;
}

int MatchQueryProcess::GetValidDoc(){
    return GetValidDoc(ORKEY , component_->GetFieldList(ORKEY)[FIRST_TEST_INDEX]);
}

int MatchQueryProcess::GetValidDoc(int logic_type, const std::vector<FieldInfo>& keys){
    std::vector<IndexInfo> index_info_vet;
    int iret = 0;
    uint32_t segment_tag = keys[FIRST_SPLIT_WORD_INDEX].segment_tag;
    if (SEGMENT_DEFAULT == segment_tag 
        || SEGMENT_NONE == segment_tag){
        iret = ValidDocFilter::Instance()->TextInvertIndexSearch(keys , index_info_vet);
    }else if(SEGMENT_CHINESE == segment_tag 
        || SEGMENT_ENGLISH == segment_tag){
        iret = ValidDocFilter::Instance()->HanPinTextInvertIndexSearch(keys , index_info_vet);
    }
    if (iret != 0) { return iret; }

    ValidDocSet valid_docs;
    bool bRet = doc_manager_->GetDocContent(index_info_vet , valid_docs);
    if (false == bRet){
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }
    ResultContext::Instance()->SetValidDocs(logic_type , valid_docs);
    return 0;
}