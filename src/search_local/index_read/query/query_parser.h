/*
 * =====================================================================================
 *
 *       Filename:  query_parser.h
 *
 *    Description:  query_parser class definition.
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

#ifndef __QUERY_PARSER_H__
#define __QUERY_PARSER_H__
#include "../comm.h"
#include <map>

class QueryParserRes{
public:
    QueryParserRes(){
        m_has_gis = 0;
    }
    map<uint32_t, vector<FieldInfo> >& FieldKeysMap(){
        return field_keys_map;
    }
    map<uint32_t, vector<FieldInfo> >& OrFieldKeysMap(){
        return or_field_keys_map;
    }
    vector<ExtraFilterKey>& ExtraFilterKeys(){
        return extra_filter_keys;
    }
    vector<ExtraFilterKey>& ExtraFilterAndKeys(){
        return extra_filter_and_keys;
    }
    vector<ExtraFilterKey>& ExtraFilterInvertKeys(){
        return extra_filter_invert_keys;
    }
    uint32_t& HasGis(){
        return m_has_gis;
    }
    string& Latitude(){
        return latitude;
    }
    string& Longitude(){
        return latitude;
    }
    double& Distance(){
        return distance;
    }
private:
    uint32_t m_has_gis;
    string latitude;
	string longitude;
	double distance;
    map<uint32_t, vector<FieldInfo> > field_keys_map;
	map<uint32_t, vector<FieldInfo> > or_field_keys_map;
    vector<ExtraFilterKey> extra_filter_keys;
	vector<ExtraFilterKey> extra_filter_and_keys;
	vector<ExtraFilterKey> extra_filter_invert_keys;
};

class QueryParser{
public:
    virtual void ParseContent(QueryParserRes* query_parser_res) = 0;
    virtual  ~QueryParser() {};
};

#endif