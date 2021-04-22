/*
 * =====================================================================================
 *
 *       Filename:  term_query_parser.h
 *
 *    Description:  term_query_parser class definition.
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

#ifndef __TERM_QUERY_PARSER_H__
#define __TERM_QUERY_PARSER_H__
#include "query_parser.h"
#include "json/json.h"

class TermQueryParser : public QueryParser
{
public:
    TermQueryParser(uint32_t a, Json::Value& v);
	~TermQueryParser();
    void ParseContent(QueryParserRes* query_parser_res);
    void ParseContent(QueryParserRes* query_parser_res, uint32_t type);

private:
	uint32_t appid;
	Json::Value value;
};

#endif