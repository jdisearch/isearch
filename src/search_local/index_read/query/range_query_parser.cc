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

int RangeQueryParser::ParseContent(QueryParserRes* query_parser_res){
    return ParseContent(query_parser_res, ORKEY);
}

int RangeQueryParser::ParseContent(QueryParserRes* query_parser_res, uint32_t type){
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
                } else {
                    info.range_type = RANGE_GE;
                }
            } else if(field_value.isMember(GT)){
                start = field_value[GT];
                if(field_value.isMember(LTE)){
                    end = field_value[LTE];
                    info.range_type = RANGE_GTLE;
                } else if(field_value.isMember(LT)){
                    end = field_value[LT];
                    info.range_type = RANGE_GTLT;
                } else {
                    info.range_type = RANGE_GT;
                }
            } else if(field_value.isMember(LTE)){
                end = field_value[LTE];
                info.range_type = RANGE_LE;
            } else if(field_value.isMember(LT)){
                end = field_value[LT];
                info.range_type = RANGE_LT;
            }
            if(!start.isInt() && !start.isNull()){
                log_error("range query only support integer");
                return -RT_PARSE_CONTENT_ERROR;
            }
            if(!end.isInt() && !end.isNull()){
                log_error("range query only support integer");
                return -RT_PARSE_CONTENT_ERROR;
            }
            if(start.isInt() || end.isInt()){
                fieldInfo.start = start.isInt() ? start.asInt() : 0;
                fieldInfo.end = end.isInt() ? end.asInt() : 0;
                fieldInfo.range_type = info.range_type;
                fieldInfos.push_back(fieldInfo);
            }
        }
        if(fieldInfos.size() != 0){
            if(type == ORKEY){
                query_parser_res->OrFieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
            } else if(type == ANDKEY){
                query_parser_res->FieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
            } else if(type == INVERTKEY){
                query_parser_res->InvertFieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
            }
        }
    } else {
        log_error("RangeQueryParser error, value is null");
        return -RT_PARSE_CONTENT_ERROR;
    }
    return 0;
}