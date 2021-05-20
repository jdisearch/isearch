/*
 * =====================================================================================
 *
 *       Filename:  query_process.h
 *
 *    Description:  query_process class definition.
 *
 *        Version:  1.0
 *        Created:  14/05/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __QUERY_PROCESS_H__
#define __QUERY_PROCESS_H__

#include "../component.h"
#include "../logical_operate.h"
#include "../doc_manager.h"
#include "../comm.h"
#include "../db_manager.h"
#include "../split_manager.h"
#include "skiplist.h"
#include "task_request.h"

class QueryProcess{
public:
    QueryProcess(uint32_t appid, Json::Value& value, Component* component);
    ~QueryProcess();
    int DoJob();
    void SetSkipList(SkipList& skipList);
    void SetRequest(CTaskRequest* request);
	void SetErrMsg(string err_msg);
	string GetErrMsg();

protected:
    void TaskBegin();
    virtual int ParseContent();
    virtual int GetValidDoc();
    virtual int GetScoreAndSort();
    virtual void TaskEnd();

protected:
    Component* component_;
    LogicalOperate* logical_operate_;
    DocManager* doc_manager_;
    uint32_t appid_;
    Json::Value value_;
    SkipList skipList_;
    CTaskRequest* request_;
	string err_msg_;
};

#endif