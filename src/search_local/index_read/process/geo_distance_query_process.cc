#include "geo_distance_query_process.h"
#include "../valid_doc_filter.h"

GeoDistanceQueryProcess::GeoDistanceQueryProcess(const Json::Value& value)
    : QueryProcess(value)
    , o_geo_point_()
    , o_distance_()
{
    response_["type"] = 1;
}

GeoDistanceQueryProcess::~GeoDistanceQueryProcess()
{ }

int GeoDistanceQueryProcess::ParseContent(){
    return ParseContent(ANDKEY);
}

int GeoDistanceQueryProcess::ParseContent(int logic_type){
    std::string s_geo_distance_fieldname("");
    Json::Value::Members member = parse_value_.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    for(; iter != member.end(); ++iter){
        Json::Value geo_value = parse_value_[*iter];
        if (DISTANCE == (*iter)){
            if (geo_value.isString()){
                o_geo_point_.SetDistance(atof(geo_value.asString().c_str()));
            } else {
                log_error("GeoDistanceParser distance should be string, the unit is km.");
                return -RT_PARSE_CONTENT_ERROR;
            }
        } else {
            s_geo_distance_fieldname = (*iter);
            o_geo_point_(geo_value);
        }
    }

    GeoPoint geo;
    geo.lon = atof(o_geo_point_.sLongtitude.c_str());
    geo.lat = atof(o_geo_point_.sLatitude.c_str());
    double d_distance = o_geo_point_.d_distance;
    log_debug("geo lng:%f ,lat:%f , dis:%f" , geo.lon , geo.lat , d_distance);

    std::vector<std::string> gisCode = GetArroundGeoHash(geo, d_distance, GEO_PRECISION);
    if(!gisCode.empty()){
        uint32_t segment_tag = SEGMENT_NONE;
        FieldInfo fieldInfo;
        fieldInfo.query_type = E_INDEX_READ_GEO_DISTANCE;
        
        uint32_t uiRet = DBManager::Instance()->GetWordField(segment_tag, component_->Appid()
            , s_geo_distance_fieldname , fieldInfo);

        std::vector<FieldInfo> fieldInfos;
        if (uiRet != 0 && SEGMENT_NONE == segment_tag) {
            component_->SetHasGisFlag(true);
            for (size_t index = 0; index < gisCode.size(); index++) {
                FieldInfo info;
                info.field = fieldInfo.field;
                info.field_type = fieldInfo.field_type;
                info.segment_tag = fieldInfo.segment_tag;
                info.word = gisCode[index];
                fieldInfos.push_back(info);
            }
        }

        if (!fieldInfos.empty()) {
            component_->AddToFieldList(logic_type, fieldInfos);
        }
    }
    return 0;
}

int GeoDistanceQueryProcess::GetValidDoc()
{
    return GetValidDoc(ANDKEY , component_->GetFieldList(ANDKEY)[FIRST_TEST_INDEX]);
}

int GeoDistanceQueryProcess::GetValidDoc(
    int logic_type,
    const std::vector<FieldInfo>& keys)
{
    std::vector<IndexInfo> index_info_vet;
    int iret = ValidDocFilter::Instance()->TextInvertIndexSearch(keys, index_info_vet);
    if (iret != 0) { return iret; }
    ResultContext::Instance()->SetIndexInfos(logic_type , index_info_vet);
    return 0;
}

int GeoDistanceQueryProcess::CheckValidDoc()
{
    bool bRet = doc_manager_->GetDocContent(o_geo_point_);
    if (false == bRet){
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }
    return 0;
}

int GeoDistanceQueryProcess::GetScore()
{
    switch (component_->SortType())
    {
    case SORT_RELEVANCE:
    case SORT_TIMESTAMP:
    case SORT_FIELD_ASC:
    case SORT_FIELD_DESC:
        {
            hash_double_map::const_iterator dis_iter = doc_manager_->GetDocDistance().cbegin();
            for(; dis_iter != doc_manager_->GetDocDistance().cend(); ++dis_iter){
                scoredocid_set_.insert(ScoreDocIdNode(dis_iter->second , dis_iter->first));
            }
        }
        break;
    case DONT_SORT:
        {
            std::set<std::string>::iterator valid_docs_iter = p_valid_docs_set_->begin();
            for(; valid_docs_iter != p_valid_docs_set_->end(); valid_docs_iter++){
                scoredocid_set_.insert(ScoreDocIdNode(1 , *valid_docs_iter));
            }
        }
        break;
    default:
        break;
    }

    return 0;
}

void GeoDistanceQueryProcess::SortScore(int& i_sequence , int& i_rank)
{
    // 降序和不排序处理 
    if (SORT_FIELD_DESC == component_->SortType()
        || DONT_SORT == component_->SortType()){
        DescSort(i_sequence , i_rank);
    }else { // 不指定情况下，默认升序，距离近在前
        AscSort(i_sequence , i_rank);
    }
}