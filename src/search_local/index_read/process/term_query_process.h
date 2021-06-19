#ifndef TERM_QUERY_PROCESS_H_
#define TERM_QUERY_PROCESS_H_

#include "query_process.h"

class TermQueryProcess : public QueryProcess{
public:
    TermQueryProcess(const Json::Value& value);
    virtual ~TermQueryProcess();

public:
    virtual int ParseContent(int logic_type);
    virtual int GetValidDoc(int logic_type, const std::vector<FieldInfo>& keys);

private:
    virtual int ParseContent();
    virtual int GetValidDoc();

private:
    friend class BoolQueryProcess;
};
#endif