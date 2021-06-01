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
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef _SEARCH_TASK_H_
#define _SEARCH_TASK_H_

#include "comm.h"
#include "process_task.h"
#include "json/value.h"
#include "search_conf.h"
#include "index_tbl_op.h"
#include "task_request.h"
#include "skiplist.h"
#include "component.h"
#include "valid_doc_filter.h"
#include "doc_manager.h"
#include <string>
#include <map>
#include <vector>
#include "task_request.h"
#include "process/query_process.h"

class SearchTask : public ProcessTask
{
public:
    SearchTask();
    virtual ~SearchTask();
    
public:
    virtual int Process(CTaskRequest *request);

private:
    Component* component_;
    DocManager* doc_manager_;
    QueryProcess* query_process_;
};

#endif
