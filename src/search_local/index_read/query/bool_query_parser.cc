#include "bool_query_parser.h"
#include "../db_manager.h"
#include "../split_manager.h"
#include "range_query_parser.h"
#include "term_query_parser.h"
#include "match_query_parser.h"

const char* const BoolQueryParser::NAME ="bool";
const char* const BoolQueryParser::MUST ="must";
const char* const BoolQueryParser::SHOULD ="should";
const char* const BoolQueryParser::TERM ="term";
const char* const BoolQueryParser::MATCH ="match";
const char* const BoolQueryParser::RANGE ="range";

BoolQueryParser::BoolQueryParser(uint32_t a, Json::Value& v)
:appid(a),value(v)
{

}

BoolQueryParser::~BoolQueryParser(){
	if(NULL != range_query_parser){
		delete range_query_parser;
	}
	if(NULL != term_query_parser){
		delete term_query_parser;
	}
	if(NULL != match_query_parser){
		delete match_query_parser;
	}
}

void BoolQueryParser::DoJobByType(Json::Value& value, uint32_t type, QueryParserRes* query_parser_res){
	if(value.isMember(TERM)){
		term_query_parser = new TermQueryParser(appid, value[TERM]);
		term_query_parser->ParseContent(query_parser_res, type);
	} else if(value.isMember(MATCH)){
		match_query_parser = new MatchQueryParser(appid, value[MATCH]);
		match_query_parser->ParseContent(query_parser_res, type);
	} else if(value.isMember(RANGE)){
		range_query_parser = new RangeQueryParser(appid, value[RANGE]);
		range_query_parser->ParseContent(query_parser_res, type);
	}
}

void BoolQueryParser::ParseContent(QueryParserRes* query_parser_res){
	if(value.isMember(MUST)){
		int type = ANDKEY;
		Json::Value must = value[MUST];
		if(must.isArray()){
			for(int i = 0; i < (int)must.size(); i++){
				DoJobByType(must[i], type, query_parser_res);
			}
		} else if (must.isObject()){
			DoJobByType(must, type, query_parser_res);
		}
	} else if (value.isMember(SHOULD)){
		int type = ORKEY;
		Json::Value should = value[SHOULD];
		if(should.isArray()){
			for(int i = 0; i < (int)should.size(); i++){
				DoJobByType(should[i], type, query_parser_res);
			}
		} else if (should.isObject()){
			DoJobByType(should, type, query_parser_res);
		}
	}
	return;
}