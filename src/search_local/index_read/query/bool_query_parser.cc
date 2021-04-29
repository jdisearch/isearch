#include "bool_query_parser.h"
#include "../db_manager.h"
#include "../split_manager.h"
#include "range_query_parser.h"
#include "term_query_parser.h"
#include "match_query_parser.h"
#include "geo_distance_parser.h"

const char* const NAME ="bool";
const char* const MUST ="must";
const char* const SHOULD ="should";
const char* const MUST_NOT ="must_not";
const char* const TERM ="term";
const char* const MATCH ="match";
const char* const RANGE ="range";
const char* const GEODISTANCE ="geo_distance";

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
    if(NULL != geo_query_parser){
        delete geo_query_parser;
    }
}

int BoolQueryParser::DoJobByType(Json::Value& value, uint32_t type, QueryParserRes* query_parser_res){
    if(value.isMember(TERM)){
        term_query_parser = new TermQueryParser(appid, value[TERM]);
        return term_query_parser->ParseContent(query_parser_res, type);
    } else if(value.isMember(MATCH)){
        match_query_parser = new MatchQueryParser(appid, value[MATCH]);
        return match_query_parser->ParseContent(query_parser_res, type);
    } else if(value.isMember(RANGE)){
        range_query_parser = new RangeQueryParser(appid, value[RANGE]);
        return range_query_parser->ParseContent(query_parser_res, type);
    } else if(value.isMember(GEODISTANCE)){
        geo_query_parser = new GeoDistanceParser(appid, value[GEODISTANCE]);
        return geo_query_parser->ParseContent(query_parser_res);
    } else {
        string err_msg = "BoolQueryParser only support term/match/range/geo_distance!";
        log_error(err_msg.c_str());
        query_parser_res->ErrMsg() = err_msg;
        return -RT_PARSE_CONTENT_ERROR;
    }
    return 0;
}

int BoolQueryParser::ParseContent(QueryParserRes* query_parser_res){
    int ret = 0;
    if(value.isMember(MUST)){
        int type = ANDKEY;
        Json::Value must = value[MUST];
        if(must.isArray()){
            for(int i = 0; i < (int)must.size(); i++){
                ret = DoJobByType(must[i], type, query_parser_res);
                if(ret != 0){
                    log_error("DoJobByType error!");
                    return -RT_PARSE_CONTENT_ERROR;
                }
            }
        } else if (must.isObject()){
            ret = DoJobByType(must, type, query_parser_res);
            if(ret != 0){
                log_error("DoJobByType error!");
                return -RT_PARSE_CONTENT_ERROR;
            }
        }
    }
    if (value.isMember(SHOULD)){
        int type = ORKEY;
        Json::Value should = value[SHOULD];
        if(should.isArray()){
            for(int i = 0; i < (int)should.size(); i++){
                ret = DoJobByType(should[i], type, query_parser_res);
                if(ret != 0){
                    log_error("DoJobByType error!");
                    return -RT_PARSE_CONTENT_ERROR;
                }
            }
        } else if (should.isObject()){
            ret = DoJobByType(should, type, query_parser_res);
            if(ret != 0){
                log_error("DoJobByType error!");
                return -RT_PARSE_CONTENT_ERROR;
            }
        }
    }
    if (value.isMember(MUST_NOT)){
        int type = INVERTKEY;
        Json::Value must_not = value[MUST_NOT];
        if(must_not.isArray()){
            for(int i = 0; i < (int)must_not.size(); i++){
                ret = DoJobByType(must_not[i], type, query_parser_res);
                if(ret != 0){
                    log_error("DoJobByType error!");
                    return -RT_PARSE_CONTENT_ERROR;
                }
            }
        } else if (must_not.isObject()) {
            ret = DoJobByType(must_not, type, query_parser_res);
            if(ret != 0){
                log_error("DoJobByType error!");
                return -RT_PARSE_CONTENT_ERROR;
            }
        }
    }
    return 0;
}