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
#include "request_context.h"
#include "result_context.h"

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
    void BindDataBasePointer(RequestContext* const p_data_base) { p_data_base_ = p_data_base; };

public:
    int HanPinTextInvertIndexSearch(const std::vector<FieldInfo>& keys
                    , std::vector<IndexInfo>& index_info_vet );

    int RangeQueryInvertIndexSearch(const std::vector<FieldInfo>& keys
                    , std::vector<IndexInfo>& index_info_vet);

    int TextInvertIndexSearch(const std::vector<FieldInfo>& keys
                    , std::vector<IndexInfo>& index_info_vet);

    int ProcessTerminal(const std::vector<std::vector<FieldInfo> >& and_keys, const TerminalQryCond& query_cond, std::vector<TerminalRes>& vecs);

private:
    void CalculateByWord(FieldInfo fieldInfo, const std::vector<IndexInfo> &doc_info);
    void SetDocIndexCache(const std::vector<IndexInfo> &doc_info, std::string& indexJsonStr);
    bool GetDocIndexCache(std::string word, uint32_t field, std::vector<IndexInfo> &doc_info);
    int GetDocIdSetByWord(FieldInfo fieldInfo, std::vector<IndexInfo> &doc_info);
    std::vector<IndexInfo> Union(std::vector<IndexInfo>& first_indexinfo_vet, std::vector<IndexInfo>& second_indexinfo_vet);

private:
    RequestContext* p_data_base_;
};

#endif