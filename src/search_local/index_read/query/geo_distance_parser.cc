#include "geo_distance_parser.h"
#include "../db_manager.h"
#include <sstream>

const char* const DISTANCE ="distance";
const int GEO_PRECISION = 6;
const int DEFAULT_DISTANCE = 2;

GeoDistanceParser::GeoDistanceParser(uint32_t a, Json::Value& v)
:appid(a),value(v)
{

}

GeoDistanceParser::~GeoDistanceParser(){

}

vector<double> splitDouble(const string& src, string separate_character)
{
    vector<double> strs;

    //分割字符串的长度,这样就可以支持如“,,”多字符串的分隔符
    int separate_characterLen = separate_character.size();
    int lastPosition = 0, index = -1;
    string str;
    double pos = 0;
    while (-1 != (index = src.find(separate_character, lastPosition)))
    {
        if (src.substr(lastPosition, index - lastPosition) != " ") {
            str = src.substr(lastPosition, index - lastPosition);
            pos = atof(str.c_str());
            strs.push_back(pos);
        }
        lastPosition = index + separate_characterLen;
    }
    string lastString = src.substr(lastPosition);//截取最后一个分隔符后的内容
    if (!lastString.empty() && lastString != " "){
        pos = atof(lastString.c_str());
        strs.push_back(pos);//如果最后一个分隔符后还有内容就入队
    }
    return strs;
}

void SetErrMsg(QueryParserRes* query_parser_res, string err_msg){
    log_error(err_msg.c_str());
    query_parser_res->ErrMsg() = err_msg;
}

int GeoDistanceParser::ParseContent(QueryParserRes* query_parser_res){
    vector<FieldInfo> fieldInfos;
    double distance = 0;
    string fieldname;
    Json::Value::Members member = value.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    for(; iter != member.end(); iter++){
        Json::Value geo_value = value[*iter];
        if(DISTANCE == *iter){
            if(geo_value.isString()){
                distance = atof(geo_value.asString().c_str());
            } else {
                SetErrMsg(query_parser_res, "GeoDistanceParser distance should be string, the unit is km.");
                return -RT_PARSE_CONTENT_ERROR;
            }
        } else {
            fieldname = *iter;
            if(geo_value.isString()){
                string geo_str = geo_value.asString();
                vector<double> res = splitDouble(geo_str, ",");
                if(res.size() >= 2){
                    geo.lat = res[0];
                    geo.lon = res[1];
                } else {
                    SetErrMsg(query_parser_res, "GeoDistanceParser format error.");
                    return -RT_PARSE_CONTENT_ERROR;
                }
            } else if (geo_value.isArray()){
                if(geo_value.size() >= 2){
                    if(geo_value[0].isDouble()){
                        geo.lon = geo_value[0].asDouble();
                    }
                    if(geo_value[1].isDouble()){
                        geo.lat = geo_value[1].asDouble();
                    }
                } else {
                    SetErrMsg(query_parser_res, "GeoDistanceParser format error.");
                    return -RT_PARSE_CONTENT_ERROR;
                }
            } else if (geo_value.isObject()){
                if(geo_value.isMember("lat") && geo_value["lat"].isDouble()){
                    geo.lat = geo_value["lat"].asDouble();
                } else {
                    SetErrMsg(query_parser_res, "GeoDistanceParser lat format error.");
                    return -RT_PARSE_CONTENT_ERROR;
                }
                if(geo_value.isMember("lon") && geo_value["lon"].isDouble()){
                    geo.lon = geo_value["lon"].asDouble();
                } else {
                    SetErrMsg(query_parser_res, "GeoDistanceParser lon format error.");
                    return -RT_PARSE_CONTENT_ERROR;
                }
            } else {
                SetErrMsg(query_parser_res, "GeoDistanceParser error, value is not string/array/object.");
                return -RT_PARSE_CONTENT_ERROR;
            }
        }
    }
    if(distance == 0){
        distance = DEFAULT_DISTANCE;
    }
    vector<string> gisCode = GetArroundGeoHash(geo, distance, GEO_PRECISION);
    if(gisCode.size() > 0){
        vector<FieldInfo> fieldInfos;
        uint32_t segment_tag = 0;
        FieldInfo fieldInfo;
        uint32_t field = DBManager::Instance()->GetWordField(segment_tag, appid, fieldname, fieldInfo);
        if (field != 0 && segment_tag == 0) {
            query_parser_res->HasGis() = 1; 
            for (size_t index = 0; index < gisCode.size(); index++) {
                FieldInfo info;
                info.field = fieldInfo.field;
                info.field_type = fieldInfo.field_type;
                info.segment_tag = fieldInfo.segment_tag;
                info.word = gisCode[index];
                fieldInfos.push_back(info);
            }
        }
        if (fieldInfos.size() != 0) {
            query_parser_res->FieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
        }
        stringstream sslat;
        stringstream sslon;
        sslat << geo.lat;
        query_parser_res->Latitude() = sslat.str();
        sslon << geo.lon;
        query_parser_res->Longitude() = sslon.str();
        query_parser_res->Distance() = distance;
    }
    return 0;
}