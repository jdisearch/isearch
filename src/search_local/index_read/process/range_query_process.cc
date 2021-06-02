#include "range_query_process.h"
#include "../valid_doc_filter.h"

RangeQueryProcess::RangeQueryProcess(const Json::Value& value)
    : QueryProcess(value)
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
        uint32_t uiRet = DBManager::Instance()->GetWordField(segment_tag, component_->Appid()
                        , fieldname, fieldInfo);
        if (0 == uiRet){
            return -RT_GET_FIELD_ERROR;
        }
        
        Json::Value field_value = parse_value_[fieldname];
        if(field_value.isObject()){
            FieldInfo info;
            Json::Value start;
            Json::Value end;
            if(field_value.isMember(GTE)){
                start = field_value[GTE];
                if(field_value.isMember(LTE)){
                    end = field_value[LTE];
                    info.range_type = RANGE_GELE;
                } else if(field_value.isMember(LT)){
                    end = field_value[LT];
                    info.range_type = RANGE_GELT;
                } else {
                    info.range_type = RANGE_GE;
                }
            } else if(field_value.isMember(GT)){
                start = field_value[GT];
                if(field_value.isMember(LTE)){
                    end = field_value[LTE];
                    info.range_type = RANGE_GTLE;
                } else if(field_value.isMember(LT)){
                    end = field_value[LT];
                    info.range_type = RANGE_GTLT;
                } else {
                    info.range_type = RANGE_GT;
                }
            } else if(field_value.isMember(LTE)){
                end = field_value[LTE];
                info.range_type = RANGE_LE;
            } else if(field_value.isMember(LT)){
                end = field_value[LT];
                info.range_type = RANGE_LT;
            }
            if(!start.isInt() && !start.isNull()){
                log_error("range query only support integer");
                return -RT_PARSE_CONTENT_ERROR;
            }
            if(!end.isInt() && !end.isNull()){
                log_error("range query only support integer");
                return -RT_PARSE_CONTENT_ERROR;
            }
            if(start.isInt() || end.isInt()){
                fieldInfo.start = start.isInt() ? start.asInt() : 0;
                fieldInfo.end = end.isInt() ? end.asInt() : 0;
                fieldInfo.range_type = info.range_type;
                fieldInfos.push_back(fieldInfo);
            }
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

int RangeQueryProcess::GetValidDoc()
{
    std::vector<IndexInfo> index_info_vet;
    int iret = ValidDocFilter::Instance()->RangeQueryInvertIndexSearch(component_->OrKeys()
                    , index_info_vet);
    if (iret != 0) { return iret;}

    bool bRet = doc_manager_->GetDocContent(index_info_vet , valid_docs_);
    if (false == bRet){
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }
    return iret;
}






RangeQueryPreTerminal::RangeQueryPreTerminal(const Json::Value& value)
    : RangeQueryProcess(value)
    , candidate_doc_()
{}

RangeQueryPreTerminal::~RangeQueryPreTerminal()
{}

int RangeQueryPreTerminal::GetValidDoc(){
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

int RangeQueryPreTerminal::GetScore(){
    log_info("RangeQueryPreTerminal do not need get score");
    return 0;
}

void RangeQueryPreTerminal::SetResponse(){
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
}