#include "term_query_parser.h"
#include "../db_manager.h"


TermQueryParser::TermQueryParser(uint32_t a, Json::Value& v)
:appid(a),value(v)
{

}

TermQueryParser::~TermQueryParser(){

}

int TermQueryParser::ParseContent(QueryParserRes* query_parser_res){
    return ParseContent(query_parser_res, ORKEY);
}

int TermQueryParser::ParseContent(QueryParserRes* query_parser_res, uint32_t type){
    vector<FieldInfo> fieldInfos;
    Json::Value::Members member = value.getMemberNames();
    Json::Value::Members::iterator iter = member.begin();
    string fieldname;
    string field_value;
    Json::Value json_value;
    if(iter != member.end()){ // 一个term下只对应一个字段
        fieldname = *iter;
        json_value = value[fieldname];
        field_value = json_value.asString();
    } else {
        log_error("TermQueryParser error, value is null");
        return -RT_PARSE_CONTENT_ERROR;
    }
    uint32_t segment_tag = 0;
    FieldInfo fieldInfo;
    uint32_t field = DBManager::Instance()->GetWordField(segment_tag, appid, fieldname, fieldInfo);
    if(field != 0 && fieldInfo.index_tag == 0){
        ExtraFilterKey extra_filter_key;
        extra_filter_key.field_name = fieldname;
        extra_filter_key.field_value = field_value;
        extra_filter_key.field_type = fieldInfo.field_type;
        if(type == ORKEY){
            query_parser_res->ExtraFilterKeys().push_back(extra_filter_key);
        } else if (type == ANDKEY) {
            query_parser_res->ExtraFilterAndKeys().push_back(extra_filter_key);
        } else if (type == INVERTKEY) {
            query_parser_res->ExtraFilterInvertKeys().push_back(extra_filter_key);
        }
    } else if (field != 0)
    {
        fieldInfo.word = field_value;
        fieldInfos.push_back(fieldInfo);
    }
    if(fieldInfos.size() != 0){
        if(type == ORKEY){
            query_parser_res->OrFieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
        } else if(type == ANDKEY) {
            query_parser_res->FieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
        } else if(type == INVERTKEY){
            query_parser_res->InvertFieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
        }
    }
    return 0;
}