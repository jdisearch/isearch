/*
 * =====================================================================================
 *
 *       Filename:  valid_doc_filter.h
 *
 *    Description:  logical operate class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Modified by: chenyujie ,chenyujie28@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef LOGICAL_OP_H
#define LOGICAL_OP_H

#include <map>
#include <set>
#include <functional>
#include "comm.h"
#include "singleton.h"
#include "noncopyable.h"
#include "component.h"

class ValidDocFilter : private noncopyable{
public:
    ValidDocFilter();
    virtual ~ValidDocFilter();

public:
    static ValidDocFilter* Instance(){
        return CSingleton<ValidDocFilter>::Instance();
    };

    static void Destroy(){
        CSingleton<ValidDocFilter>::Destroy();
    };

public:
    void BindDataBasePointer(Component* const p_data_base) { p_data_base_ = p_data_base; };

public:
    // int GetTopDocScore(std::map<std::string, double>& top_doc_score);

    int OrAndInvertKeyFilter(std::vector<IndexInfo>& index_info_vet
            , std::set<std::string>& highlightWord, std::map<std::string, KeyInfoVet>& docid_keyinfo_map
            , std::map<std::string, uint32_t>& key_doccount_map);

    int PureTextInvertIndexSearch(const std::vector<std::vector<FieldInfo> >& keys
                    , std::vector<IndexInfo>& index_info_vet 
                    , std::set<std::string>& highlightWord
                    , std::map<std::string, KeyInfoVet>& docid_keyinfo_map);

    int RangeQueryInvertIndexSearch(const std::vector<std::vector<FieldInfo> >& keys
                    , std::vector<IndexInfo>& index_info_vet);

    int MixTextInvertIndexSearch(const std::vector<std::vector<FieldInfo> >& keys
                    , std::vector<IndexInfo>& index_info_vet
                    , std::set<std::string>& highlightWord
                    , std::map<std::string, KeyInfoVet>& docid_keyinfo_map
                    , std::map<std::string, uint32_t>& key_doccount_map);

    int ProcessTerminal(const std::vector<std::vector<FieldInfo> >& and_keys, const TerminalQryCond& query_cond, std::vector<TerminalRes>& vecs);

    void CalculateByWord(FieldInfo fieldInfo, const std::vector<IndexInfo> &doc_info
                    , std::map<std::string, KeyInfoVet> &ves, std::map<std::string, uint32_t> &key_in_doc);
    void SetDocIndexCache(const std::vector<IndexInfo> &doc_info, std::string& indexJsonStr);
    bool GetDocIndexCache(std::string word, uint32_t field, std::vector<IndexInfo> &doc_info);
    int GetDocIdSetByWord(FieldInfo fieldInfo, std::vector<IndexInfo> &doc_info);

private:
    // int GetTopDocIdSetByWord(FieldInfo fieldInfo, std::vector<TopDocInfo>& doc_info);

    int OrKeyFilter(std::vector<IndexInfo>& index_info_vet
            , std::set<std::string>& highlightWord, std::map<std::string, KeyInfoVet>& docid_keyinfo_map
            , std::map<std::string, uint32_t>& key_doccount_map);

    int AndKeyFilter(std::vector<IndexInfo>& index_info_vet
            , std::set<std::string>& highlightWord, std::map<std::string, KeyInfoVet>& docid_keyinfo_map
            , std::map<std::string, uint32_t>& key_doccount_map);

    int InvertKeyFilter(std::vector<IndexInfo>& index_info_vet
            , std::set<std::string>& highlightWord, std::map<std::string, KeyInfoVet>& docid_keyinfo_map
            , std::map<std::string, uint32_t>& key_doccount_map);

    int Process(const std::vector<std::vector<FieldInfo> >& keys, std::vector<IndexInfo>& index_info_vet
            , std::set<std::string>& highlightWord, std::map<std::string, KeyInfoVet>& docid_keyinfo_map
            , std::map<std::string, uint32_t>& key_doccount_map);

private:
    Component* p_data_base_;
    std::function<std::vector<IndexInfo>(std::vector<IndexInfo>&,std::vector<IndexInfo>&)> o_or_and_not_functor_;
};

#endif