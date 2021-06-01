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
#include "../valid_doc_filter.h"
#include "../doc_manager.h"
#include "../../index_write/comm.h"
#include "../db_manager.h"
#include "../split_manager.h"
#include "skiplist.h"
#include "task_request.h"


class QueryProcess{
public:
    QueryProcess(Json::Value& value);
    virtual~ QueryProcess();

public:
    int StartQuery();
    void SetRequest(const CTaskRequest* request) { request_ = request; };
    void SetParseJsonValue(const Json::Value& value) { parse_value_ = value; };
    void SetComponent(const Component* component) { component_ = component;};
    void SetDocManager(const DocManager* doc_manager) { doc_manager_ = doc_manager;};
    void SetQueryContext(const ValidDocSet& valid_doc, const HighLightWordSet& high_light_word
                , const DocKeyinfosMap& doc_keyinfos, const KeywordDoccountMap& keyword_doccount){
                    valid_docs_ = valid_doc;
                    high_light_word_ = high_light_word;
                    docid_keyinfovet_map_ = doc_keyinfos;
                    key_doccount_map_ = keyword_doccount;
    };

protected:
    virtual int ParseContent() = 0;
    virtual int ParseContent(int logic_type) = 0;
    virtual int GetValidDoc() = 0;
    virtual int GetScore();
    virtual void SortScore(int& i_sequence , int& i_rank);
    virtual void SetResponse();

protected:
    void SortByCOrderOp();
    void SortForwardBySkipList(int& i_sequence , int& i_rank);
    void SortBackwardBySkipList(int& i_sequence , int& i_rank);
    void AppendHighLightWord();

protected:
    Component* component_;
    DocManager* doc_manager_;
    CTaskRequest* request_;

    Json::Value parse_value_;
    SkipList skipList_;
    Json::Value response_;

    ValidDocSet valid_docs_;
    HighLightWordSet high_light_word_;
    DocKeyinfosMap docid_keyinfovet_map_;
    KeywordDoccountMap key_doccount_map_;

private:
    FIELDTYPE sort_field_type_;
};

#endif