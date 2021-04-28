/*
 * =====================================================================================
 *
 *       Filename:  range_query_parser.h
 *
 *    Description:  range_query_parser class definition.
 *
 *        Version:  1.0
 *        Created:  19/04/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __RANGE_QUERY_PARSER_H__
#define __RANGE_QUERY_PARSER_H__
#include "query_parser.h"
#include "json/json.h"

class RangeQueryParser : public QueryParser
{
public:
    RangeQueryParser(uint32_t a, Json::Value& v);
	~RangeQueryParser();
    int ParseContent(QueryParserRes* query_parser_res);
    int ParseContent(QueryParserRes* query_parser_res, uint32_t type);

private:
	uint32_t appid;
	Json::Value value;
};

#endif