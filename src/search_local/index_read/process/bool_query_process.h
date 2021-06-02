#ifndef BOOL_QUERY_PROCESS_H_
#define BOOL_QUERY_PROCESS_H_

#include "query_process.h"

class RangeQueryProcess;
class RangeQueryPreTerminal;
class GeoDistanceQueryProcess;

class BoolQueryProcess : public QueryProcess{
public:
    BoolQueryProcess(const Json::Value& value);
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