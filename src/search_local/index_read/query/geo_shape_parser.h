/*
 * =====================================================================================
 *
 *       Filename:  geo_shape_parser.h
 *
 *    Description:  geo_shape_parser class definition.
 *
 *        Version:  1.0
 *        Created:  08/05/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __GEO_SHAPE_PARSER_H__
#define __GEO_SHAPE_PARSER_H__
#include "query_parser.h"
#include "json/json.h"
#include "geohash.h"

class GeoShapeParser : public QueryParser
{
public:
    GeoShapeParser(uint32_t a, Json::Value& v);
    ~GeoShapeParser();
    int ParseContent(QueryParserRes* query_parser_res);

private:
	void SetErrMsg(QueryParserRes* query_parser_res, string err_msg);
	vector<double> splitDouble(const string& src, string separate_character);

private:
    uint32_t appid;
    Json::Value value;
    GeoPoint geo;
    double distance;
};

#endif