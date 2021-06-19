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

#include <iostream>
#include <sstream>
#include "../component.h"
#include "../valid_doc_filter.h"
#include "../doc_manager.h"
#include "../db_manager.h"
#include "../split_manager.h"
#include "../comm.h"
#include "../system_status.h"
#include "skiplist.h"
#include "task_request.h"

const char* const BOOL ="bool";
const char* const MUST ="must";
const char* const SHOULD ="should";
const char* const MUST_NOT ="must_not";
const char* const TERM ="term";
const char* const MATCH ="match";
const char* const RANGE ="range";
const char* const GEODISTANCE ="geo_distance";
const char* const DISTANCE = "distance";
const char* const GEOSHAPE ="geo_polygon";

#define FIRST_TEST_INDEX 0
#define FIRST_SPLIT_WORD_INDEX 0

enum E_INDEX_READ_QUERY_PROCESS{
    E_INDEX_READ_GEO_DISTANCE,
    E_INDEX_READ_GEO_SHAPE,
    E_INDEX_READ_MATCH,
    E_INDEX_READ_TERM,
    E_INDEX_READ_RANGE,
    E_INDEX_READ_PRE_TERM,
    E_INDEX_READ_TOTAL_NUM
};

class QueryProcess{
public:
    QueryProcess(const Json::Value& value);
    virtual~ QueryProcess();

public:
    int StartQuery();
    void SetRequest(CTaskRequest* const request) { request_ = request; };
    void SetParseJsonValue(const Json::Value& value) { parse_value_ = value; };
    void SetComponent(RequestContext* const component) { component_ = component;};
    void SetDocManager(DocManager* const doc_manager) { doc_manager_ = doc_manager;};

public:
    virtual int ParseContent(int logic_type) = 0;
    virtual int GetValidDoc(int logic_type , const std::vector<FieldInfo>& keys) = 0;

protected:
    virtual int ParseContent() = 0;
    virtual int GetValidDoc() = 0;
    virtual int GetScore();
    virtual void SortScore(int& i_sequence , int& i_rank);
    virtual const Json::Value& SetResponse();

protected:
    void SortByCOrderOp(int& i_rank);
    void AscSort(int& i_sequence , int& i_rank);
    void DescSort(int& i_sequence , int& i_rank);
    void AppendHighLightWord();

protected:
    RequestContext* component_;
    DocManager* doc_manager_;
    CTaskRequest* request_;
    ValidDocSet* p_valid_docs_set_;

    Json::Value parse_value_;
    std::set<ScoreDocIdNode> scoredocid_set_;
    Json::Value response_;

private:
    FIELDTYPE sort_field_type_;
};

#endif