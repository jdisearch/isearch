#include "bool_query_process.h"
#include "geo_distance_query_process.h"
#include "geo_shape_query_process.h"
#include "match_query_process.h"
#include "term_query_process.h"
#include "range_query_process.h"

BoolQueryProcess::BoolQueryProcess(const Json::Value& value)
    : QueryProcess(value)
    , query_process_map_()
{
    query_process_map_.insert(std::make_pair(E_INDEX_READ_GEO_DISTANCE 
                    , new GeoDistanceQueryProcess(parse_value_)));
    query_process_map_.insert(std::make_pair(E_INDEX_READ_GEO_SHAPE 
                    , new GeoShapeQueryProcess(parse_value_ )));
    query_process_map_.insert(std::make_pair(E_INDEX_READ_MATCH 
                    , new MatchQueryProcess(parse_value_ )));
    query_process_map_.insert(std::make_pair(E_INDEX_READ_TERM 
                    , new TermQueryProcess(parse_value_ )));
    query_process_map_.insert(std::make_pair(E_INDEX_READ_RANGE
                    , RangeQueryGenerator::Instance()->GetRangeQueryProcess(E_INDEX_READ_RANGE , parse_value_)));
    query_process_map_.insert(std::make_pair(E_INDEX_READ_RANGE_PRE_TERM 
                    , RangeQueryGenerator::Instance()->GetRangeQueryProcess(E_INDEX_READ_RANGE_PRE_TERM , parse_value_)));
}

BoolQueryProcess::~BoolQueryProcess()
{ 
    std::map<int , QueryProcess*>::iterator iter = query_process_map_.begin();
    for ( ; iter != query_process_map_.end(); ++iter){
        if (iter->second != NULL){
            delete iter->second;
            iter->second = NULL;
        }
    }
}

void BoolQueryProcess::InitQueryMember(){
    std::map<int , QueryProcess*>::iterator iter = query_process_map_.begin();
    for ( ; iter != query_process_map_.end(); ++iter){
        iter->second->SetRequest(request_);
        iter->second->SetComponent(component_);
        iter->second->SetDocManager(doc_manager_);
    }
}

int BoolQueryProcess::ParseContent(){
    InitQueryMember();
    int ret = 0;
    if(parse_value_.isMember(MUST)){
        ret = ParseRequest(parse_value_[MUST] , ANDKEY);
        if (ret != 0) { return ret; }
    }
    if (parse_value_.isMember(SHOULD)){
        ret = ParseRequest(parse_value_[SHOULD] , ORKEY);
        if (ret != 0) { return ret; }
    }
    if (parse_value_.isMember(MUST_NOT)){
        ret = ParseRequest(parse_value_[MUST_NOT] , INVERTKEY);
        if (ret != 0) { return ret; }
    }
    return ret;
}

int BoolQueryProcess::ParseContent(int logic_type){
    log_info("BoolQueryProcess no need parse content by logictype");
    return 0;
}

int BoolQueryProcess::GetValidDoc(){
    bool bRet = false;
    if (component_->TerminalTag()){
        range_query_pre_term_ = dynamic_cast<RangeQueryPreTerminal*>(query_process_map_[E_INDEX_READ_RANGE_PRE_TERM]);
        if (range_query_pre_term_ != NULL){
            return range_query_pre_term_->GetValidDoc();
        }
    }

    std::vector<IndexInfo> index_info_vet;
    int iret = ValidDocFilter::Instance()->OrAndInvertKeyFilter(index_info_vet , high_light_word_ 
            , docid_keyinfovet_map_ , key_doccount_map_);
    if (iret != 0) { return iret; }
    
    if (component_->GetHasGisFlag()){
        geo_distance_query_ = dynamic_cast<GeoDistanceQueryProcess*>(query_process_map_[E_INDEX_READ_GEO_DISTANCE]);
        if (geo_distance_query_ != NULL){
            bRet = doc_manager_->GetDocContent(index_info_vet 
                        , geo_distance_query_->o_geo_point_ 
                        , geo_distance_query_->o_distance_);
            geo_distance_query_->SetQueryContext(valid_docs_ , high_light_word_ 
                        , docid_keyinfovet_map_ , key_doccount_map_);
        }
    }else{
        range_query_ = dynamic_cast<RangeQueryProcess*>(query_process_map_[E_INDEX_READ_RANGE]);
        if (range_query_ != NULL){
            bRet = doc_manager_->GetDocContent(index_info_vet , valid_docs_);
            range_query_->SetQueryContext(valid_docs_ , high_light_word_ 
                        , docid_keyinfovet_map_ , key_doccount_map_);
        }
    }
}

int BoolQueryProcess::GetScore(){
    if (range_query_pre_term_ != NULL){
        return range_query_pre_term_->GetScore();
    }

    if (geo_distance_query_ != NULL){
        return geo_distance_query_->GetScore();
    }

    if(range_query_ != NULL){
        return range_query_->GetScore();
    }
}

void BoolQueryProcess::SortScore(int& i_sequence , int& i_rank){
    if (range_query_pre_term_ != NULL){
        return range_query_pre_term_->SortScore(i_sequence , i_rank);
    }

    if (geo_distance_query_ != NULL){
        geo_distance_query_->SortScore(i_sequence , i_rank);
    }

    if(range_query_ != NULL){
        range_query_->SortScore(i_sequence , i_rank);
    }
}

void BoolQueryProcess::SetResponse(){
    if (range_query_pre_term_ != NULL){
        range_query_pre_term_->SetResponse();
        response_ = range_query_pre_term_->response_;
    }

    if (geo_distance_query_ != NULL){
        geo_distance_query_->SetResponse();
        response_ = geo_distance_query_->response_;
    }

    if(range_query_ != NULL){
        range_query_->SetResponse();
        response_ = range_query_->response_;
    }
}

int BoolQueryProcess::ParseRequest(const Json::Value& request, int logic_type){
    int iret = 0;
    if(request.isArray()){
        for(int i = 0; i < (int)request.size(); i++){
            iret = InitQueryProcess(logic_type , request[i]);
            if(iret != 0){
                log_error("InitQueryProcess error!");
                return -RT_PARSE_CONTENT_ERROR;
            }
        }
    } else if (request.isObject()) {
        iret = InitQueryProcess(logic_type, request);
        if(iret != 0){
            log_error("InitQueryProcess error!");
            return -RT_PARSE_CONTENT_ERROR;
        }
    }
    return 0;
}

int BoolQueryProcess::InitQueryProcess(uint32_t type , const Json::Value& value){
    int query_type = -1;
    Json::Value parse_value;

    if(value.isMember(TERM)){
        query_type = E_INDEX_READ_TERM;
        parse_value = parse_value_[TERM];
    } else if(value.isMember(MATCH)){
        query_type = E_INDEX_READ_MATCH;
        parse_value = parse_value_[MATCH];
    } else if(value.isMember(RANGE)){
        parse_value = parse_value_[RANGE];
        if (component_->TerminalTag()){
            query_type = E_INDEX_READ_RANGE_PRE_TERM;
            
        }else{
            query_type = E_INDEX_READ_RANGE;
        }
    } else if(value.isMember(GEODISTANCE)){
        query_type = E_INDEX_READ_GEO_DISTANCE;
        parse_value = parse_value_[GEODISTANCE];
    } else if(value.isMember(POINTS)){
        query_type = E_INDEX_READ_GEO_SHAPE;
        parse_value = parse_value_[POINTS];
    } else {
        log_error("BoolQueryParser only support term/match/range/geo_distance!");
        return -RT_PARSE_CONTENT_ERROR;
    }

    query_process_map_[query_type]->SetParseJsonValue(parse_value);
    query_process_map_[query_type]->ParseContent(type);
    return 0;
}
