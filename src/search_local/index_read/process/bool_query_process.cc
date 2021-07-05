#include "bool_query_process.h"
#include "geo_distance_query_process.h"
#include "geo_shape_query_process.h"
#include "match_query_process.h"
#include "term_query_process.h"
#include "range_query_process.h"
#include "../key_format.h"

BoolQueryProcess::BoolQueryProcess(const Json::Value& value)
    : QueryProcess(value)
    , query_process_map_()
    , query_bitset_()
{ }

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
            std::vector<std::vector<MemCompUnionNode> > keys_vvec;
            std::vector<FieldInfo> unionFieldInfos;
            bool b_has_range = false;
            for(union_field_iter = union_field_vec.begin(); union_field_iter != union_field_vec.end(); union_field_iter++){
                std::vector<FieldInfo> field_info_vec = fieldid_fieldinfos_map.at(*union_field_iter);
                std::vector<MemCompUnionNode> key_vec;
                GetKeyFromFieldInfo(field_info_vec, key_vec , b_has_range);
                keys_vvec.push_back(key_vec);
                fieldid_fieldinfos_map.erase(*union_field_iter);  // 命中union_key的需要从fieldid_fieldinfos_map中删除
            }
            std::vector<MemCompUnionNode> union_keys = Combination(keys_vvec);
            for(int m = 0 ; m < (int)union_keys.size(); m++){
                FieldInfo info;
                info.field = 0;
                info.field_type = FIELD_INDEX;
                info.query_type= (b_has_range ? E_INDEX_READ_RANGE : E_INDEX_READ_TERM);
                info.segment_tag = (b_has_range ? SEGMENT_RANGE : SEGMENT_DEFAULT);
                info.word = union_keys[m].s_key;
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
    InitQueryMember();
    return ret;
}

int BoolQueryProcess::ParseContent(int logic_type){
    log_info("BoolQueryProcess no need parse content by logictype");
    return 0;
}

int BoolQueryProcess::GetValidDoc(int logic_type , const std::vector<FieldInfo>& keys){
    log_info("BoolQueryProcess no need get valid doc by logictype");
    return 0;
}

int BoolQueryProcess::GetValidDoc(){
    if (query_bitset_.test(E_INDEX_READ_PRE_TERM) && query_bitset_.test(E_INDEX_READ_TERM)){
            return query_process_map_[E_INDEX_READ_PRE_TERM]->GetValidDoc();
    }   

    for (uint32_t ui_key_type = ORKEY; ui_key_type < KEYTOTALNUM; ++ui_key_type){
        std::vector<std::vector<FieldInfo> >::const_iterator iter = component_->GetFieldList(ui_key_type).cbegin();
        for (; iter != component_->GetFieldList(ui_key_type).cend(); ++iter){
            std::vector<FieldInfo>::const_iterator field_info_iter = iter->cbegin();
            for (; field_info_iter != iter->cend(); ++field_info_iter){
                query_process_map_[field_info_iter->query_type]->GetValidDoc(ui_key_type , *iter);
            }
        }
    }
    return 0;
}

int BoolQueryProcess::CheckValidDoc(){
    for (uint32_t ui_query_type = E_INDEX_READ_PRE_TERM
        ; ui_query_type < E_INDEX_READ_TOTAL_NUM
        ; ++ui_query_type){
        if (!query_bitset_.test(ui_query_type)){
            continue;
        }

        if (E_INDEX_READ_GEO_DISTANCE == ui_query_type || E_INDEX_READ_GEO_SHAPE == ui_query_type){
            if (component_->SortField().empty() && !query_bitset_.test(E_INDEX_READ_RANGE)){
                return query_process_map_[ui_query_type]->CheckValidDoc();
            }
            continue;
        }
        return query_process_map_[ui_query_type]->CheckValidDoc();
    }
    return -1;
}

int BoolQueryProcess::GetScore(){
    for (uint32_t ui_query_type = E_INDEX_READ_PRE_TERM
        ; ui_query_type < E_INDEX_READ_TOTAL_NUM
        ; ++ui_query_type){
        if (!query_bitset_.test(ui_query_type)){
            continue;
        }

        if (E_INDEX_READ_GEO_DISTANCE == ui_query_type || E_INDEX_READ_GEO_SHAPE == ui_query_type){
            if (component_->SortField().empty() && !query_bitset_.test(E_INDEX_READ_RANGE)){
                return query_process_map_[ui_query_type]->GetScore();
            }
            continue;
        }
        return query_process_map_[ui_query_type]->GetScore();
    }
    return -1;
}

void BoolQueryProcess::SortScore(int& i_sequence , int& i_rank){
    for (uint32_t ui_query_type = E_INDEX_READ_PRE_TERM
        ; ui_query_type < E_INDEX_READ_TOTAL_NUM
        ; ++ui_query_type){
        if (!query_bitset_.test(ui_query_type)){
            continue;
        }

        if (E_INDEX_READ_GEO_DISTANCE == ui_query_type || E_INDEX_READ_GEO_SHAPE == ui_query_type){
            if (component_->SortField().empty() && !query_bitset_.test(E_INDEX_READ_RANGE)){
                query_process_map_[ui_query_type]->SortScore(i_sequence , i_rank);
                return;
            }
            continue;
        }
        query_process_map_[ui_query_type]->SortScore(i_sequence , i_rank);
        return;
    }
}

const Json::Value& BoolQueryProcess::SetResponse(){
    for (uint32_t ui_query_type = E_INDEX_READ_PRE_TERM
        ; ui_query_type < E_INDEX_READ_TOTAL_NUM
        ; ++ui_query_type){
        if (!query_bitset_.test(ui_query_type)){
            continue;
        }

        response_ = query_process_map_[ui_query_type]->SetResponse();
        return response_;
    }
    return response_;
}

int BoolQueryProcess::ParseRequest(
    const Json::Value& request,
    int logic_type)
{
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
        Json::Value::Members search_member = request.getMemberNames();
        Json::Value::Members::iterator iter = search_member.begin();
        for (; iter != search_member.end(); ++iter){
            iret = InitQueryProcess(logic_type, *iter);
            if(iret != 0){
                log_error("InitQueryProcess error!");
                return -RT_PARSE_CONTENT_ERROR;
            }
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
        if (query_process_map_.find(query_type) == query_process_map_.end()){
            query_process_map_.insert(std::make_pair(query_type 
                    , new TermQueryProcess(parse_value)));
        }
    } else if(value.isMember(MATCH)){
        query_type = E_INDEX_READ_MATCH;
        parse_value = parse_value_[MATCH];
        if (query_process_map_.find(query_type) == query_process_map_.end()){
            query_process_map_.insert(std::make_pair(query_type 
                    , new MatchQueryProcess(parse_value)));
        }
    } else if(value.isMember(RANGE)){
        parse_value = parse_value_[RANGE];
        if (component_->TerminalTag()){
            query_type = E_INDEX_READ_PRE_TERM;
        }else{
            query_type = E_INDEX_READ_RANGE;
        }
        if (query_process_map_.find(query_type) == query_process_map_.end()){
            query_process_map_.insert(std::make_pair(query_type 
                    , RangeQueryGenerator::Instance()->GetRangeQueryProcess(query_type , parse_value)));
        }
    } else if(value.isMember(GEODISTANCE)){
        query_type = E_INDEX_READ_GEO_DISTANCE;
        parse_value = parse_value_[GEODISTANCE];
        if (query_process_map_.find(query_type) == query_process_map_.end()){
            query_process_map_.insert(std::make_pair(query_type 
                    , new GeoDistanceQueryProcess(parse_value)));
        }
    } else if(value.isMember(POINTS)){
        query_type = E_INDEX_READ_GEO_SHAPE;
        parse_value = parse_value_[POINTS];
        if (query_process_map_.find(query_type) == query_process_map_.end()){
            query_process_map_.insert(std::make_pair(query_type 
                    , new GeoShapeQueryProcess(parse_value)));
        }
    } else {
        log_error("BoolQueryParser only support term/match/range/geo_distance!");
        return -RT_PARSE_CONTENT_ERROR;
    }

    if (!query_bitset_.test(query_type)){
        query_bitset_.set(query_type);
    }
    query_process_map_[query_type]->SetParseJsonValue(parse_value);
    query_process_map_[query_type]->ParseContent(type);
    return 0;
}

void BoolQueryProcess::GetKeyFromFieldInfo(const std::vector<FieldInfo>& field_info_vec, std::vector<MemCompUnionNode>& key_vec, bool& b_has_range){
    std::vector<FieldInfo>::const_iterator iter = field_info_vec.cbegin();
    for(; iter != field_info_vec.end(); iter++){
        if (SEGMENT_RANGE == iter->segment_tag ){
            b_has_range = true;
            key_vec.push_back(MemCompUnionNode(iter->field_type , std::to_string(iter->start)));
            key_vec.push_back(MemCompUnionNode(iter->field_type , std::to_string(iter->end)));
        }else{
            key_vec.push_back(MemCompUnionNode(iter->field_type , iter->word));
        }
    }
}

/*
** 通过递归求出二维vector每一维vector中取一个数的各种组合
** 输入：[[a],[b1,b2],[c1,c2,c3]]
** 输出：[a_b1_c1,a_b1_c2,a_b1_c3,a_b2_c1,a_b2_c2,a_b2_c3]
*/
std::vector<MemCompUnionNode> BoolQueryProcess::Combination(std::vector<std::vector<MemCompUnionNode> >& dimensionalArr){
    int FLength = dimensionalArr.size();
    if(FLength >= 2){
        int SLength1 = dimensionalArr[0].size();
        int SLength2 = dimensionalArr[1].size();
        int DLength = SLength1 * SLength2;
        std::vector<MemCompUnionNode> temporary(DLength);
        int index = 0;
        for(int i = 0; i < SLength1; i++){
            for (int j = 0; j < SLength2; j++) {
                KeyFormat::UnionKey o_keyinfo_vet;
                o_keyinfo_vet.push_back(std::make_pair(dimensionalArr[0][i].ui_field_type , dimensionalArr[0][i].s_key));
                o_keyinfo_vet.push_back(std::make_pair(dimensionalArr[1][j].ui_field_type , dimensionalArr[1][j].s_key));
                std::string s_format_key = KeyFormat::Encode(o_keyinfo_vet);

                temporary[index].s_key = s_format_key;
                temporary[index].ui_field_type = FIELD_STRING;
                index++;
            }
        }
        std::vector<std::vector<MemCompUnionNode> > new_arr;
        new_arr.push_back(temporary);
        for(int i = 2; i < (int)dimensionalArr.size(); i++){
            new_arr.push_back(dimensionalArr[i]);
        }
        return Combination(new_arr);
    } else {
        return dimensionalArr[0];
    }
}