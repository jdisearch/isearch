/*
 * =====================================================================================
 *
 *       Filename:  geo_distance_parser.h
 *
 *    Description:  geo_distance_parser class definition.
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

#ifndef __GEO_DISTANCE_PARSER_H__
#define __GEO_DISTANCE_PARSER_H__
#include "query_parser.h"
#include "json/json.h"
#include "geohash.h"

class GeoDistanceParser : public QueryParser
{
public:
    GeoDistanceParser(uint32_t a, Json::Value& v);
	~GeoDistanceParser();
    void ParseContent(QueryParserRes* query_parser_res);

private:
	uint32_t appid;
	Json::Value value;
    GeoPoint geo;
    double distance;
};

#endif