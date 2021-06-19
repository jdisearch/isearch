#ifndef BOOL_QUERY_PROCESS_H_
#define BOOL_QUERY_PROCESS_H_

#include "query_process.h"
#include <bitset>
#include <algorithm>

class QueryProcess;
class PreTerminal;
class GeoDistanceQueryProcess;

struct MemCompUnionNode{
    uint32_t ui_field_type;
    std::string s_key;

    MemCompUnionNode(uint32_t field_type , std::string key)
        : ui_field_type(field_type)
        , s_key(key)
    { }
};

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
    void HandleUnifiedIndex();
    void GetKeyFromFieldInfo(const std::vector<FieldInfo>& field_info_vec, std::vector<MemCompUnionNode>& key_vec , bool& b_has_range);
    std::vector<MemCompUnionNode> Combination(std::vector<std::vector<MemCompUnionNode> >& dimensionalArr);

private:
    std::map<int , QueryProcess*> query_process_map_;
    std::bitset<E_INDEX_READ_TOTAL_NUM> query_bitset_;
    PreTerminal* range_query_pre_term_;
};

#endif