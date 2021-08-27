#include "geo_query_sort_operator.h"

GeoQuerySortOperator::GeoQuerySortOperator(RequestContext* request_cnt, DocManager* doc_manager)
    : SortOperatorBase(request_cnt , doc_manager)
{}

GeoQuerySortOperator::~GeoQuerySortOperator()
{}

void GeoQuerySortOperator::RelevanceSort()
{
    log_debug("relevance score sort type");
    const std::vector<IndexInfo>& o_index_info_vet = ResultContext::Instance()->GetIndexInfos();
    std::set<std::string>::iterator valid_docs_iter = p_valid_docs_set_->begin();
    for(; valid_docs_iter != p_valid_docs_set_->end(); valid_docs_iter++){
        std::vector<IndexInfo>::const_iterator index_info_iter = o_index_info_vet.cbegin();
        for (; index_info_iter != o_index_info_vet.cend(); ++index_info_iter){
            if ((*valid_docs_iter) == (index_info_iter->doc_id)){
                scoredocid_set_.insert(ScoreDocIdNode(index_info_iter->distance , index_info_iter->doc_id));
            }
        }
    }
}