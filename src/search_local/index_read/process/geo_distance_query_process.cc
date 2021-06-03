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

    std::vector<std::string> gisCode = GetArroundGeoHash(geo, d_distance, GEO_PRECISION);
    if(!gisCode.empty()){
        uint32_t segment_tag = SEGMENT_NONE;
        FieldInfo fieldInfo;
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

int GeoDistanceQueryProcess::GetValidDoc(){
    std::vector<IndexInfo> index_info_vet;
    int iret = ValidDocFilter::Instance()->MixTextInvertIndexSearch(component_->AndKeys(), index_info_vet 
            , high_light_word_, docid_keyinfovet_map_ , key_doccount_map_);
    if (iret != 0) { return iret; }

    bool bRet = doc_manager_->GetDocContent(index_info_vet , o_geo_point_ , o_distance_);
    if (false == bRet){
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }
    return 0;
}

int GeoDistanceQueryProcess::GetScore(){
    switch (component_->SortType())
    {
    case SORT_RELEVANCE:
    case SORT_TIMESTAMP:
        {
            hash_double_map::iterator dis_iter = o_distance_.begin();
            for(; dis_iter != o_distance_.end(); ++dis_iter){
                std::string doc_id = dis_iter->first;
                double score = dis_iter->second;
                if ((o_geo_point_.d_distance > -0.0001 && o_geo_point_.d_distance < 0.0001) 
                    || (score + 1e-6 <= o_geo_point_.d_distance)){
                    skipList_.InsertNode(score, doc_id.c_str());
                }
            }
        }
        break;
    case DONT_SORT:
    case SORT_FIELD_ASC:
    case SORT_FIELD_DESC:
        {
            // do nothing
        }
    default:
        break;
    }

    return 0;
}

void GeoDistanceQueryProcess::SortScore(int& i_sequence , int& i_rank)
{
    SortForwardBySkipList(i_sequence , i_rank);
}