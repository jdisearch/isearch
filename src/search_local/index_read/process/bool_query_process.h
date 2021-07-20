#ifndef BOOL_QUERY_PROCESS_H_
#define BOOL_QUERY_PROCESS_H_

#include "query_process.h"
#include <bitset>
#include <algorithm>

class QueryProcess;
class PreTerminal;
class GeoDistanceQueryProcess;

class BoolQueryProcess : public QueryProcess{
public:
    BoolQueryProcess(const Json::Value& value);
    virtual ~BoolQueryProcess();

private:
    virtual int ParseContent(int logic_type);
    virtual int GetValidDoc(int logic_type , const std::vector<FieldInfo>& keys);

    virtual int ParseContent();
    virtual int GetValidDoc();
    virtual int CheckValidDoc();
    virtual int GetScore();
    virtual const Json::Value& SetResponse();

private:
    int ParseRequest(const Json::Value& request, int logic_type);
    int InitQueryProcess(uint32_t type , const std::string& query_key, const Json::Value& parse_value);
    void HandleUnifiedIndex();
    void GetKeyFromFieldInfo(const std::vector<FieldInfo>& field_info_vec, std::vector<std::string>& key_vec , bool& b_has_range);
    std::vector<std::string> Combination(std::vector<std::vector<std::string> >& dimensionalArr);

private:
    std::map<int , QueryProcess*> query_process_map_;
    std::bitset<E_INDEX_READ_TOTAL_NUM> query_bitset_;
};

#endif