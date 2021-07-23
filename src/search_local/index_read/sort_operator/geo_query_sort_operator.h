#ifndef GEO_QUERY_SORT_OPERATOR_H_
#define GEO_QUERY_SORT_OPERATOR_H_

#include "sort_operator_base.h"

class GeoQuerySortOperator : public SortOperatorBase
{
public:
    GeoQuerySortOperator(RequestContext* request_cnt , DocManager* doc_manager);
    virtual~ GeoQuerySortOperator();

private:
    virtual void RelevanceSort();
};
#endif