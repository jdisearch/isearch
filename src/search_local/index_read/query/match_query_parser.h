/*
 * =====================================================================================
 *
 *       Filename:  match_query_parser.h
 *
 *    Description:  match_query_parser class definition.
 *
 *        Version:  1.0
 *        Created:  20/04/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __MATCH_QUERY_PARSER_H__
#define __MATCH_QUERY_PARSER_H__
#include "query_parser.h"
#include "json/json.h"

class MatchQueryParser : public QueryParser
{
public:
    MatchQueryParser(uint32_t a, Json::Value& v);
    ~MatchQueryParser();
    int ParseContent(QueryParserRes* query_parser_res);
    int ParseContent(QueryParserRes* query_parser_res, uint32_t type);

private:
    uint32_t appid;
    Json::Value value;
};

#endif