#ifndef SORT_OPERATOR_BASE_H_
#define SORT_OPERATOR_BASE_H_

#include <set>
#include "log.h"
#include "../comm.h"
#include "../result_context.h"
#include "../request_context.h"
#include "../doc_manager.h"

class SortOperatorBase
{
public:
    SortOperatorBase(RequestContext* request_cnt , DocManager* doc_manager);
    virtual ~SortOperatorBase();

public:
    std::set<ScoreDocIdNode>* GetSortOperator(uint32_t ui_sort_type);
    int GetSortFieldType() { return sort_field_type_;};

protected:
    virtual void RelevanceSort();
    virtual void NoneSort();
    virtual void AssignFieldSort();

protected:
    RequestContext* component_;
    DocManager* doc_manager_;
    ValidDocSet* p_valid_docs_set_;
    std::set<ScoreDocIdNode> scoredocid_set_;

private:
    FIELDTYPE sort_field_type_;
};

#endif