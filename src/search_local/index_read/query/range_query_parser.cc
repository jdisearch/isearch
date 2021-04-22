#include "range_query_parser.h"
#include "../db_manager.h"

const char* const GTE ="gte";
const char* const GT ="gt";
const char* const LTE ="lte";
const char* const LT ="lt";

RangeQueryParser::RangeQueryParser(uint32_t a, Json::Value& v)
:appid(a),value(v)
{

}

RangeQueryParser::~RangeQueryParser(){

}

void RangeQueryParser::ParseContent(QueryParserRes* query_parser_res){
    ParseContent(query_parser_res, ORKEY);
}

void RangeQueryParser::ParseContent(QueryParserRes* query_parser_res, uint32_t type){
    vector<FieldInfo> fieldInfos;
	Json::Value::Members member = value.getMemberNames();
	Json::Value::Members::iterator iter = member.begin();
	if(iter != member.end()){ // 一个range下只对应一个字段
		string fieldname = *iter;
		uint32_t segment_tag = 0;
		FieldInfo fieldInfo;
		DBManager::Instance()->GetWordField(segment_tag, appid, fieldname, fieldInfo);
		Json::Value field_value = value[fieldname];
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
				}
			} else if(field_value.isMember(GT)){
				start = field_value[GT];
				if(field_value.isMember(LTE)){
					end = field_value[LTE];
					info.range_type = RANGE_GTLE;
				} else if(field_value.isMember(LT)){
					end = field_value[LT];
					info.range_type = RANGE_GTLT;
				}
			}
			info.start = start.asInt();
			info.end = end.asInt();
			fieldInfos.push_back(info);
		}
		if(fieldInfos.size() != 0){
            if(type == ORKEY){
                query_parser_res->OrFieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
            } else {
			    query_parser_res->FieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
            }
		}
	}
	return;
}