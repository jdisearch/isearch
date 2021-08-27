#include "geo_shape_query_process.h"

GeoShapeQueryProcess::GeoShapeQueryProcess(const Json::Value& value)
    : GeoDistanceQueryProcess(value)
{ }

GeoShapeQueryProcess::~GeoShapeQueryProcess()
{ }

int GeoShapeQueryProcess::ParseContent(int logic_type){
    Json::Value::Members member = parse_value_.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    if(iter == member.end()){ // 一个geo_shape下只对应一个字段
        return -RT_PARSE_CONTENT_ERROR;
    }
    std::string fieldname = *iter;
    Json::Value field_value = parse_value_[fieldname];
    GeoShapeContext o_geo_shape;
    if(field_value.isMember(POINTS)){
        o_geo_shape(field_value[POINTS]);
    } else {
        return -RT_PARSE_CONTENT_ERROR;
    }

    if (o_geo_shape.IsGeoShapeFormat()){
        std::vector<std::string> gisCode = GetArroundGeoHash(o_geo_shape.GetMinEnclosRect(), GEO_PRECISION);
        if(!gisCode.empty()){
            vector<FieldInfo> fieldInfos;
            uint32_t segment_tag = SEGMENT_NONE;
            FieldInfo fieldInfo;
            fieldInfo.query_type = E_INDEX_READ_GEO_SHAPE;

            uint32_t uiRet = DBManager::Instance()->GetWordField(segment_tag, component_->Appid()
                        , fieldname, fieldInfo);

            if (0 == uiRet){
                log_error("field_name:[%s] error ,not in the app_field_define", fieldname.c_str());
                return -RT_PARSE_CONTENT_ERROR;
            }

            if (uiRet != 0 && SEGMENT_NONE == segment_tag) {
                component_->SetHasGisFlag(true);
                for (size_t index = 0; index < gisCode.size(); index++) {
                    fieldInfo.word = gisCode[index];
                    log_debug("geo shape point:%s", fieldInfo.word.c_str());
                    fieldInfos.push_back(fieldInfo);
                }
            }
            if (!fieldInfos.empty()) {
                component_->AddToFieldList(logic_type, fieldInfos);
            }
        }
    }
    return 0;
}
