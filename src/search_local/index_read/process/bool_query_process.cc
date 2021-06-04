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

void BoolQueryProcess::HandleUnifiedIndex(){
    std::vector<std::vector<FieldInfo> >& and_keys = component_->AndKeys();

    std::map<uint32_t , std::vector<FieldInfo> > fieldid_fieldinfos_map;
    std::vector<std::vector<FieldInfo> >::iterator iter = and_keys.begin();
    for (; iter != and_keys.end(); ++iter){
        fieldid_fieldinfos_map.insert(std::make_pair(((*iter)[0]).field , *iter));
    }
    component_->AndKeys().clear();

    std::vector<std::string> union_key_vec;
    DBManager::Instance()->GetUnionKeyField(component_->Appid() , union_key_vec);
    std::vector<std::string>::iterator union_key_iter = union_key_vec.begin();
    for(; union_key_iter != union_key_vec.end(); union_key_iter++){
        std::string union_key = *union_key_iter;
        std::vector<int> union_field_vec = splitInt(union_key, ",");
        std::vector<int>::iterator union_field_iter = union_field_vec.begin();
        bool hit_union_key = true;
        for(; union_field_iter != union_field_vec.end(); union_field_iter++){
            if(fieldid_fieldinfos_map.find(*union_field_iter) == fieldid_fieldinfos_map.end()){
                hit_union_key = false;
                break;
            }
        }
        if(hit_union_key == true){
            std::vector<std::vector<std::string> > keys_vvec;
            std::vector<FieldInfo> unionFieldInfos;
            for(union_field_iter = union_field_vec.begin(); union_field_iter != union_field_vec.end(); union_field_iter++){
                std::vector<FieldInfo> field_info_vec = fieldid_fieldinfos_map.at(*union_field_iter);
                std::vector<std::string> key_vec;
                GetKeyFromFieldInfo(field_info_vec, key_vec);
                keys_vvec.push_back(key_vec);
                fieldid_fieldinfos_map.erase(*union_field_iter);  // 命中union_key的需要从fieldid_fieldinfos_map中删除
            }
            std::vector<std::string> union_keys = Combination(keys_vvec);
            for(int m = 0 ; m < (int)union_keys.size(); m++){
                FieldInfo info;
                info.field = 0;
                info.field_type = FIELD_STRING;
                info.segment_tag = 1;
                info.word = union_keys[m];
                unionFieldInfos.push_back(info);
            }
            component_->AddToFieldList(ANDKEY, unionFieldInfos);
        }
    }
    std::map<uint32_t, std::vector<FieldInfo> >::iterator field_key_map_iter = fieldid_fieldinfos_map.begin();
    for(; field_key_map_iter != fieldid_fieldinfos_map.end(); field_key_map_iter++){
        component_->AddToFieldList(ANDKEY, field_key_map_iter->second);
    }
}

int BoolQueryProcess::ParseContent(){
    InitQueryMember();
    int ret = 0;
    if(parse_value_.isMember(MUST)){
        ret = ParseRequest(parse_value_[MUST] , ANDKEY);
        if (ret != 0) { return ret; }
    }
    HandleUnifiedIndex();
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

void BoolQueryProcess::GetKeyFromFieldInfo(const std::vector<FieldInfo>& field_info_vec, std::vector<std::string>& key_vec){
    std::vector<FieldInfo>::const_iterator iter = field_info_vec.cbegin();
    for(; iter != field_info_vec.end(); iter++){
        key_vec.push_back((*iter).word);
    }
}

/*
** 通过递归求出二维vector每一维vector中取一个数的各种组合
** 输入：[[a],[b1,b2],[c1,c2,c3]]
** 输出：[a_b1_c1,a_b1_c2,a_b1_c3,a_b2_c1,a_b2_c2,a_b2_c3]
*/
std::vector<std::string> BoolQueryProcess::Combination(std::vector<std::vector<std::string> >& dimensionalArr){
    int FLength = dimensionalArr.size();
    if(FLength >= 2){
        int SLength1 = dimensionalArr[0].size();
        int SLength2 = dimensionalArr[1].size();
        int DLength = SLength1 * SLength2;
        std::vector<std::string> temporary(DLength);
        int index = 0;
        for(int i = 0; i < SLength1; i++){
            for (int j = 0; j < SLength2; j++) {
                temporary[index] = dimensionalArr[0][i] +"_"+ dimensionalArr[1][j];
                index++;
            }
        }
        std::vector<std::vector<std::string> > new_arr;
        new_arr.push_back(temporary);
        for(int i = 2; i < (int)dimensionalArr.size(); i++){
            new_arr.push_back(dimensionalArr[i]);
        }
        return Combination(new_arr);
    } else {
        return dimensionalArr[0];
    }
}