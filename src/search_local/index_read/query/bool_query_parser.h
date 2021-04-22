/*
 * =====================================================================================
 *
 *       Filename:  bool_query_parser.h
 *
 *    Description:  bool_query_parser class definition.
 *
 *        Version:  1.0
 *        Created:  05/03/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __BOOL_QUERY_PARSER_H__
#define __BOOL_QUERY_PARSER_H__
#include "query_parser.h"
#include "json/json.h"

class RangeQueryParser;
class TermQueryParser;
class MatchQueryParser;
class BoolQueryParser : public QueryParser
{
public:
	BoolQueryParser(uint32_t a, Json::Value& v);
	~BoolQueryParser();

	void ParseContent(QueryParserRes* query_parser_res);

private:
	void DoJobByType(Json::Value& value, uint32_t type, QueryParserRes* query_parser_res);

private:
	uint32_t appid;
	Json::Value value;
	static const char* const NAME;
	static const char* const MUST;
	static const char* const SHOULD;
	static const char* const TERM;
	static const char* const MATCH;
	static const char* const RANGE;
	RangeQueryParser* range_query_parser;
	TermQueryParser* term_query_parser;
	MatchQueryParser* match_query_parser;
};

#endif