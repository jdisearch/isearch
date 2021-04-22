#include "match_query_parser.h"
#include "../db_manager.h"
#include "../split_manager.h"

MatchQueryParser::MatchQueryParser(uint32_t a, Json::Value& v)
:appid(a),value(v)
{

}

MatchQueryParser::~MatchQueryParser(){

}

void MatchQueryParser::ParseContent(QueryParserRes* query_parser_res){
    ParseContent(query_parser_res, ORKEY);
}

void MatchQueryParser::ParseContent(QueryParserRes* query_parser_res, uint32_t type){
    vector<FieldInfo> fieldInfos;
	Json::Value::Members member = value.getMemberNames();
	Json::Value::Members::iterator iter = member.begin();
	string fieldname;
	Json::Value field_value;
	if(iter != member.end()){ // 一个match下只对应一个字段
		fieldname = *iter;
		field_value = value[fieldname];
	} else {
		return;
	}
	uint32_t segment_tag = 0;
	FieldInfo fieldInfo;
	uint32_t field = DBManager::Instance()->GetWordField(segment_tag, appid, fieldname, fieldInfo);
	if (field != 0 && segment_tag == 1)
	{
		string split_data = SplitManager::Instance()->split(field_value.asString(), appid);
		log_debug("split_data: %s", split_data.c_str());
		vector<string> split_datas = splitEx(split_data, "|");
		for(size_t index = 0; index < split_datas.size(); index++)
		{
			FieldInfo info;
			info.field = fieldInfo.field;
			info.field_type = fieldInfo.field_type;
			info.word = split_datas[index];
			info.segment_tag = fieldInfo.segment_tag;
			fieldInfos.push_back(info);
		}
	}
	else if (field != 0)
	{
		fieldInfo.word = field_value.asString();
		fieldInfos.push_back(fieldInfo);
	}

	if(fieldInfos.size() != 0){
        if(type == ORKEY){
            query_parser_res->OrFieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
        } else {
            query_parser_res->FieldKeysMap().insert(make_pair(fieldInfo.field, fieldInfos));
        }
	}
	return;
}