/*
 * =====================================================================================
 *
 *       Filename:  term_query_process.h
 *
 *    Description:  term_query_process class definition.
 *
 *        Version:  1.0
 *        Created:  20/05/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __TERM_QUERY_PROCESS_H__
#define __TERM_QUERY_PROCESS_H__

#include "query_process.h"

class TermQueryProcess: public QueryProcess{
public:
    TermQueryProcess(uint32_t appid, Json::Value& value, Component* component);
    ~TermQueryProcess();
    int ParseContent();
    int GetValidDoc();
    int GetScoreAndSort();
    void TaskEnd();

    int ParseContent(uint32_t type);
    void AppendHighLightWord(Json::Value& response);

private:
    set<string> highlightWord_;
    map<string, vec> doc_info_map_;
    map<string, uint32_t> key_in_doc_;
    vector<IndexInfo> doc_vec_;
    hash_double_map distances_;
    set<string> valid_docs_;
    uint32_t sort_type_;
    string sort_field_;
    bool has_gis_;
    FIELDTYPE sort_field_type_;
};

#endif