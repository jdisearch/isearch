#include "geo_shape_parser.h"
#include "../db_manager.h"
#include <sstream>

const char* const POINTS ="points";
const int GEO_PRECISION = 6;

GeoShapeParser::GeoShapeParser(uint32_t a, Json::Value& v)
:appid(a),value(v)
{

}

GeoShapeParser::~GeoShapeParser(){

}

vector<double> GeoShapeParser::splitDouble(const string& src, string separate_character)
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

void GeoShapeParser::SetErrMsg(QueryParserRes* query_parser_res, string err_msg){
    log_error(err_msg.c_str());
    query_parser_res->ErrMsg() = err_msg;
}

int GeoShapeParser::ParseContent(QueryParserRes* query_parser_res){
	Json::Value::Members member = value.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
	if(iter == member.end()){ // 一个geo_shape下只对应一个字段
		SetErrMsg(query_parser_res, "GeoShapeParser format error, content is null.");
		return -RT_PARSE_CONTENT_ERROR;
	}
	set<double> lat_arr;
	set<double> lon_arr;
	string fieldname = *iter;
	Json::Value field_value = value[fieldname];
	if(field_value.isMember(POINTS)){
		Json::Value points = field_value[POINTS];
		if(points.isArray()){
			for(int i = 0; i < (int)points.size(); i++){
				double lat;
				double lon;
				Json::Value geo_value = points[i];
				if(geo_value.isString()){
					string geo_str = geo_value.asString();
					vector<double> res = splitDouble(geo_str, ",");
					if(res.size() >= 2){
						lat = res[0];
						lon = res[1];
					} else {
						SetErrMsg(query_parser_res, "GeoShapeParser format error.");
						return -RT_PARSE_CONTENT_ERROR;
					}
				} else if (geo_value.isArray()){
					if(geo_value.size() >= 2){
						if(geo_value[0].isDouble()){
							lon = geo_value[0].asDouble();
						}
						if(geo_value[1].isDouble()){
							lat = geo_value[1].asDouble();
						}
					} else {
						SetErrMsg(query_parser_res, "GeoShapeParser format error.");
						return -RT_PARSE_CONTENT_ERROR;
					}
				} else if (geo_value.isObject()){
					if(geo_value.isMember("lat") && geo_value["lat"].isDouble()){
						lat = geo_value["lat"].asDouble();
					} else {
						SetErrMsg(query_parser_res, "GeoShapeParser lat format error.");
						return -RT_PARSE_CONTENT_ERROR;
					}
					if(geo_value.isMember("lon") && geo_value["lon"].isDouble()){
						lon = geo_value["lon"].asDouble();
					} else {
						SetErrMsg(query_parser_res, "GeoShapeParser lon format error.");
						return -RT_PARSE_CONTENT_ERROR;
					}
				} else {
					SetErrMsg(query_parser_res, "GeoShapeParser error, value is not string/array/object.");
					return -RT_PARSE_CONTENT_ERROR;
				}
				lat_arr.insert(lat);
				lon_arr.insert(lon);
			}
		} else {
			SetErrMsg(query_parser_res, "GeoShapeParser error, points is not a array.");
			return -RT_PARSE_CONTENT_ERROR;
		}
	} else {
		SetErrMsg(query_parser_res, "GeoShapeParser error, no points content provide.");
		return -RT_PARSE_CONTENT_ERROR;
	}
	if(lon_arr.size() > 0 && lat_arr.size() > 0){
		vector<string> gisCode = GetArroundGeoHash(*lon_arr.rbegin(), *lon_arr.begin(), *lat_arr.rbegin(), *lat_arr.begin(), GEO_PRECISION);
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
		}
	}
	return 0;
}