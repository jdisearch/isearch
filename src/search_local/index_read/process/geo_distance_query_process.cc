#include "geo_distance_query_process.h"
#include "../sort_operator/geo_query_sort_operator.h"
#include "../valid_doc_filter.h"

GeoDistanceQueryProcess::GeoDistanceQueryProcess(const Json::Value& value)
    : QueryProcess(value)
    , logictype_geopoint_map_()
{
    response_["type"] = 1;
}

GeoDistanceQueryProcess::~GeoDistanceQueryProcess()
{ }

int GeoDistanceQueryProcess::ParseContent(){
    return ParseContent(ANDKEY);
}

int GeoDistanceQueryProcess::ParseContent(int logic_type)
{
    std::string s_geo_distance_fieldname("");
    GeoPointContext o_geo_point;
    Json::Value::Members member = parse_value_.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    for(; iter != member.end(); ++iter){
        Json::Value geo_value = parse_value_[*iter];
        if (DISTANCE == (*iter)){
            if (geo_value.isString()){
                o_geo_point.SetDistance(atof(geo_value.asString().c_str()));
            } else {
                log_error("GeoDistanceParser distance should be string, the unit is km.");
                return -RT_PARSE_CONTENT_ERROR;
            }
        } else {
            s_geo_distance_fieldname = (*iter);
            o_geo_point(geo_value);
        }
    }

    logictype_geopoint_map_.insert(std::make_pair(logic_type , o_geo_point));

    GeoPoint geo;
    geo.lon = atof(o_geo_point.sLongtitude.c_str());
    geo.lat = atof(o_geo_point.sLatitude.c_str());
    double d_distance = o_geo_point.d_distance;
    log_debug("geo lng:%f ,lat:%f , dis:%f" , geo.lon , geo.lat , d_distance);

    std::vector<std::string> gisCode = GetArroundGeoHash(geo, d_distance, GEO_PRECISION);
    if(!gisCode.empty()){
        uint32_t segment_tag = SEGMENT_NONE;
        FieldInfo fieldInfo;
        fieldInfo.query_type = E_INDEX_READ_GEO_DISTANCE;
        
        uint32_t uiRet = DBManager::Instance()->GetWordField(segment_tag, component_->Appid()
            , s_geo_distance_fieldname , fieldInfo);

        if (0 == uiRet){
            log_error("field_name:[%s] error ,not in the app_field_define", s_geo_distance_fieldname.c_str());
            return -RT_PARSE_CONTENT_ERROR;
        }

        std::vector<FieldInfo> fieldInfos;
        if (uiRet != 0 && SEGMENT_NONE == segment_tag) {
            component_->SetHasGisFlag(true);
            for (size_t index = 0; index < gisCode.size(); index++) {
                fieldInfo.word = gisCode[index];
                log_debug("geo point:%s", fieldInfo.word.c_str());
                fieldInfos.push_back(fieldInfo);
            }
        }

        component_->AddToFieldList(logic_type, fieldInfos);
    }
    return 0;
}

int GeoDistanceQueryProcess::GetValidDoc()
{
    if (component_->GetFieldList(ANDKEY).empty()){
        return -RT_GET_FIELD_ERROR;
    }
    return GetValidDoc(ANDKEY , component_->GetFieldList(ANDKEY)[FIRST_TEST_INDEX]);
}

int GeoDistanceQueryProcess::GetValidDoc(
    int logic_type,
    const std::vector<FieldInfo>& keys)
{
    log_debug("geo related query GetValidDoc beginning...");
    std::vector<IndexInfo> index_info_vet;
    int iret = ValidDocFilter::Instance()->TextInvertIndexSearch(keys, index_info_vet);
    if (iret != 0) { return iret; }

    bool bRet = doc_manager_->GetDocContent(logictype_geopoint_map_[logic_type] , index_info_vet);
    if (false == bRet){
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }
    ResultContext::Instance()->SetIndexInfos(logic_type , index_info_vet);
    return 0;
}

int GeoDistanceQueryProcess::GetScore()
{
    log_debug("geo related query GetScore beginning...");
    sort_operator_base_ = new GeoQuerySortOperator(component_ , doc_manager_);
    p_scoredocid_set_ = sort_operator_base_->GetSortOperator((uint32_t)component_->SortType());
    return 0;
}

void GeoDistanceQueryProcess::SortScore(int& i_sequence , int& i_rank)
{
    log_debug("geo related query SortScore beginning...");
    
    if ((SORT_FIELD_DESC == component_->SortType() || SORT_FIELD_ASC == component_->SortType())
        && p_scoredocid_set_->empty()){
        SortByCOrderOp(i_rank);
    }else if (SORT_FIELD_DESC == component_->SortType()
        || DONT_SORT == component_->SortType()){ // 降序和不排序处理
        DescSort(i_sequence , i_rank);
    }else { // 不指定情况下，默认升序，距离近在前
        AscSort(i_sequence , i_rank);
    }
}