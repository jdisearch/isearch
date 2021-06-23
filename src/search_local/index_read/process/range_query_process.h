#ifndef RANGE_QUERY_PROCESS_H_
#define RANGE_QUERY_PROCESS_H_

#include "singleton.h"
#include "noncopyable.h"
#include "query_process.h"

const char* const GTE ="gte";
const char* const GT ="gt";
const char* const LTE ="lte";
const char* const LT ="lt";

class RangeQueryProcess: public QueryProcess{
public:
    RangeQueryProcess(const Json::Value& value , uint32_t ui_query_type);
    virtual~ RangeQueryProcess();

public:
    virtual int ParseContent(int logic_type);
    virtual int GetValidDoc(int logic_type, const std::vector<FieldInfo>& keys);
    virtual int GetValidDoc();

protected:
    virtual int ParseContent();

private:
    uint32_t ui_query_type_;
};

class PreTerminal : public RangeQueryProcess{
public:
    PreTerminal(const Json::Value& value, uint32_t ui_query_type);
    virtual~ PreTerminal();

public:
    virtual int GetValidDoc(int logic_type, const std::vector<FieldInfo>& keys);

    virtual int GetValidDoc();
    virtual int CheckValidDoc();
    virtual int GetScore();
    virtual const Json::Value& SetResponse();

private:
    std::vector<TerminalRes> candidate_doc_;
};

class RangeQueryGenerator : private noncopyable{
public:
    RangeQueryGenerator() { };
    virtual~ RangeQueryGenerator() { };

public:
    static RangeQueryGenerator* Instance(){
        return CSingleton<RangeQueryGenerator>::Instance();
    };

    static void Destroy(){
        CSingleton<RangeQueryGenerator>::Destroy();
    };

public:
    // 内存释放由调用方处理
    QueryProcess* GetRangeQueryProcess(int iType , const Json::Value& parse_value){
        QueryProcess* current_range_query = NULL;
        switch (iType){
        case E_INDEX_READ_RANGE:{
            current_range_query = new RangeQueryProcess(parse_value , E_INDEX_READ_RANGE);
            }
            break;
        case E_INDEX_READ_PRE_TERM:{
             current_range_query = new PreTerminal(parse_value , E_INDEX_READ_PRE_TERM);
            }
            break;
        default:
            break;
        }

        return current_range_query;
    }
};

#endif