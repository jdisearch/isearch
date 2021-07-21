#include "range_query_process.h"
#include "../valid_doc_filter.h"

RangeQueryProcess::RangeQueryProcess(const Json::Value& value, uint32_t ui_query_type)
    : QueryProcess(value)
    , ui_query_type_(ui_query_type)
{ }

RangeQueryProcess::~RangeQueryProcess()
{ }

int RangeQueryProcess::ParseContent(){
    return ParseContent(ORKEY);
}

int RangeQueryProcess::ParseContent(int logic_type)
{
    std::vector<FieldInfo> fieldInfos;
    Json::Value::Members member = parse_value_.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    if(iter != member.end()){ // 一个range下只对应一个字段
        std::string fieldname = *iter;
        uint32_t segment_tag = 0;
        FieldInfo fieldInfo;
        fieldInfo.query_type = ui_query_type_;

        uint32_t uiRet = DBManager::Instance()->GetWordField(segment_tag, component_->Appid()
                        , fieldname, fieldInfo);
        if (0 == uiRet){
            return -RT_GET_FIELD_ERROR;
        }
        
        Json::Value field_value = parse_value_[fieldname];
        if(field_value.isObject()){
            Json::Value start;
            Json::Value end;
            RANGTYPE ui_range_type;
            if(field_value.isMember(GTE)){
                start = field_value[GTE];
                if(field_value.isMember(LTE)){
                    end = field_value[LTE];
                    ui_range_type = RANGE_GELE;
                } else if(field_value.isMember(LT)){
                    end = field_value[LT];
                    ui_range_type = RANGE_GELT;
                } else {
                    ui_range_type = RANGE_GE;
                }
            } else if(field_value.isMember(GT)){
                start = field_value[GT];
                if(field_value.isMember(LTE)){
                    end = field_value[LTE];
                    ui_range_type = RANGE_GTLE;
                } else if(field_value.isMember(LT)){
                    end = field_value[LT];
                    ui_range_type = RANGE_GTLT;
                } else {
                    ui_range_type = RANGE_GT;
                }
            } else if(field_value.isMember(LTE)){
                end = field_value[LTE];
                ui_range_type = RANGE_LE;
            } else if(field_value.isMember(LT)){
                end = field_value[LT];
                ui_range_type = RANGE_LT;
            }
            fieldInfo.range_type = ui_range_type;

            log_debug("range_type:%d", ui_range_type);
            if(start.isInt()){
                fieldInfo.start = start.asInt();
            } else if (start.isDouble()){
                fieldInfo.start = start.asDouble();
            } else {
                log_error("range query lower value only support int/double");
            }
            
            if (end.isInt()){
                fieldInfo.end = end.asInt();
            }else if (end.isDouble()){
                fieldInfo.end = end.asDouble();
            } else {
                log_error("range query upper limit value only support int/double");
            }
            
            log_debug("start:%f , end:%f" , fieldInfo.start , fieldInfo.end);
            fieldInfos.push_back(fieldInfo);
        }
        if (!fieldInfos.empty()) {
            component_->AddToFieldList(logic_type, fieldInfos);
        }
    } else {
        log_error("RangeQueryParser error, value is null");
        return -RT_PARSE_CONTENT_ERROR;
    }
    return 0;
}

int RangeQueryProcess::GetValidDoc(){
    return GetValidDoc(ORKEY , component_->GetFieldList(ORKEY)[FIRST_TEST_INDEX]);
}

int RangeQueryProcess::GetValidDoc(int logic_type, const std::vector<FieldInfo>& keys)
{
    std::vector<IndexInfo> index_info_vet;
    int iret = ValidDocFilter::Instance()->RangeQueryInvertIndexSearch(keys, index_info_vet);
    if (iret != 0) { return iret;}
    ResultContext::Instance()->SetIndexInfos(logic_type , index_info_vet);
    return iret;
}






PreTerminal::PreTerminal(const Json::Value& value, uint32_t ui_query_type)
    : RangeQueryProcess(value , ui_query_type)
    , candidate_doc_()
{}

PreTerminal::~PreTerminal()
{}

int PreTerminal::GetValidDoc(int logic_type, const std::vector<FieldInfo>& keys){
    return 0;
}

int PreTerminal::GetValidDoc(){
    uint32_t count = 0;
    uint32_t N = 2;
    uint32_t limit_start = 0;
    int try_times = 0;
    while(count < component_->PageSize()){
        if(try_times++ > 10){
            log_debug("ProcessTerminal try_times is the max, return");
            break;
        }
        vector<TerminalRes> and_vecs;
        TerminalQryCond query_cond;
        query_cond.sort_type = component_->SortType();
        query_cond.sort_field = component_->SortField();
        query_cond.last_id = component_->LastId();
        query_cond.last_score = component_->LastScore();
        query_cond.limit_start = limit_start;
        query_cond.page_size = component_->PageSize() * N;
        int ret = ValidDocFilter::Instance()->ProcessTerminal(component_->AndKeys(), query_cond, and_vecs);
        if(0 != ret){
            log_error("ProcessTerminal error.");
            return -RT_GET_DOC_ERR;
        }
        for(int i = 0; i < (int)and_vecs.size(); i++){
            std::string doc_id = and_vecs[i].doc_id;
            std::stringstream ss;
            ss << (int)and_vecs[i].score;
            std::string ss_key = ss.str();
            log_debug("last_score: %s, ss_key: %s, score: %lf", query_cond.last_score.c_str(), ss_key.c_str(), and_vecs[i].score);
            if(component_->LastId() != "" && ss_key == query_cond.last_score){ // 翻页时过滤掉已经返回过的文档编号
                if(component_->SortType() == SORT_FIELD_DESC && doc_id >= component_->LastId()){
                    continue;
                }
                if(component_->SortType() == SORT_FIELD_ASC && doc_id <= component_->LastId()){
                    continue;
                }
            }
            if(doc_manager_->CheckDocByExtraFilterKey(doc_id) == true){
                count++;
                candidate_doc_.push_back(and_vecs[i]);
            }
        }
        limit_start += component_->PageSize() * N;
        N *= 2;
    }
    return 0;
}

int PreTerminal::GetScore(){
    log_info("RangeQueryPreTerminal do not need get score");
    return 0;
}

const Json::Value& PreTerminal::SetResponse(){
    response_["code"] = 0;
    int sequence = -1;
    int rank = 0;
    for (uint32_t i = 0; i < candidate_doc_.size(); i++) {
        if(rank >= (int)component_->PageSize()){
            break;
        }
        sequence++;
        rank++;
        TerminalRes tmp = candidate_doc_[i];
        Json::Value doc_info;
        doc_info["doc_id"] = Json::Value(tmp.doc_id.c_str());
        doc_info["score"] = Json::Value(tmp.score);
        response_["result"].append(doc_info);
    }
    response_["type"] = 0;
    response_["count"] = rank;  // TODO 这里的count并不是实际的总数
    return response_;
}