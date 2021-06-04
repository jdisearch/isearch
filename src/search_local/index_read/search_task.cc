/*
 * =====================================================================================
 *
 *       Filename:  search_task.h
 *
 *    Description:  search task class definition.
 *
 *        Version:  1.0
 *        Created:  19/03/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Modified by: chenyujie ,chenyujie28@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "search_task.h"
#include "split_manager.h"
#include "monitor.h"
#include "index_tbl_op.h"
#include "valid_doc_filter.h"
#include "process/geo_distance_query_process.h"
#include "process/geo_shape_query_process.h"
#include "process/match_query_process.h"
#include "process/range_query_process.h"
#include "process/term_query_process.h"
#include "process/bool_query_process.h"

SearchTask::SearchTask()
    : ProcessTask()
    , component_(new Component())
    , doc_manager_(new DocManager(component_))
    , query_process_(NULL)
{ 
    ValidDocFilter::Instance()->BindDataBasePointer(component_);
}

SearchTask::~SearchTask() {
    if(component_ != NULL){
        delete component_;
    }
    if(doc_manager_ != NULL){
        delete doc_manager_;
    }
    if (query_process_ != NULL){
        delete query_process_;
    }
}

int SearchTask::Process(CTaskRequest *request)
{
    log_debug("SearchTask::Process begin: %lld.", (long long int)GetSysTimeMicros());
    common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.searchTask"));
    Json::Value recv_packet;
    std::string request_string = request->buildRequsetString();
    if (component_->ParseJson(request_string.c_str(), request_string.length(), recv_packet) != 0) {
        string str = GenReplyStr(PARAMETER_ERR);
        request->setResult(str);
        common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
        common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
        return -RT_PARSE_JSON_ERR;
    }
    component_->InitSwitch();

    Json::Value query = component_->GetQuery();
    if(query.isObject()){
        if(query.isMember(MATCH)){
            query_process_ = new MatchQueryProcess(query[MATCH]);
        }else if(query.isMember(TERM)){
            query_process_ = new TermQueryProcess(query[TERM]);
        }else if (query.isMember(GEODISTANCE)){
            query_process_ = new GeoDistanceQueryProcess(query[GEODISTANCE]);
        }else if (query.isMember(GEOSHAPE)){
            query_process_ = new GeoShapeQueryProcess(query[GEOSHAPE]);
        }else if (query.isMember(RANGE)){
            if (component_->TerminalTag()){
                query_process_ = RangeQueryGenerator::Instance()->GetRangeQueryProcess(E_INDEX_READ_RANGE_PRE_TERM 
                                        , query[RANGE]);
            }else{
                query_process_ = RangeQueryGenerator::Instance()->GetRangeQueryProcess(E_INDEX_READ_RANGE 
                                        , query[RANGE]);
            }
        }else if (query.isMember(BOOL)){
            query_process_ = new BoolQueryProcess(query[BOOL]);
        }else{
            log_error("no suit query process.");
            return -RT_PARSE_JSON_ERR;
        }
        
        query_process_->SetRequest(request);
        query_process_->SetComponent(component_);
        query_process_->SetDocManager(doc_manager_);

        int ret = query_process_->StartQuery();
        if(ret != 0){
            std::string str = GenReplyStr(PARAMETER_ERR);
            request->setResult(str);
            common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
            common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
            log_error("query_process_ StartQuery error, ret: %d", ret);
            return ret;
        }
    }

    common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
    return 0;
}