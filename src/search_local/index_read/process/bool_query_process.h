#ifndef BOOL_QUERY_PROCESS_H_
#define BOOL_QUERY_PROCESS_H_

#include "query_process.h"

const char* const NAME ="bool";
const char* const MUST ="must";
const char* const SHOULD ="should";
const char* const MUST_NOT ="must_not";
const char* const TERM ="term";
const char* const MATCH ="match";
const char* const RANGE ="range";
const char* const GEODISTANCE ="geo_distance";

enum E_INDEX_READ_QUERY_PROCESS{
    E_INDEX_READ_GEO_DISTANCE,
    E_INDEX_READ_GEO_SHAPE,
    E_INDEX_READ_MATCH,
    E_INDEX_READ_TERM,
    E_INDEX_READ_RANGE,
    E_INDEX_READ_RANGE_PRE_TERM
};

class BoolQueryProcess : public QueryProcess{
public:
    BoolQueryProcess(Json::Value& value);
    virtual ~BoolQueryProcess();

private:
    virtual int ParseContent();
    virtual int ParseContent(int logic_type);
    virtual int GetValidDoc();
    virtual int GetScore();
    virtual void SortScore(int& i_sequence , int& i_rank);
    virtual void SetResponse();

private:
    int ParseRequest(const Json::Value& request, int logic_type);
    int InitQueryProcess(uint32_t type , const Json::Value& value);
    void InitQueryMember();

private:
    std::map<int , QueryProcess*> query_process_map_;
    RangeQueryProcess* range_query_;
    RangeQueryPreTerminal* range_query_pre_term_;
    GeoDistanceQueryProcess* geo_distance_query_;
};

#endif