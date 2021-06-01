/*
 * =====================================================================================
 *
 *       Filename:  component.h
 *
 *    Description:  component class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2019
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Modified by: chenyujie ,chenyujie28@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __COMPONENT_H__
#define __COMPONENT_H__

#include "comm.h"
#include "json/json.h"
#include <string>
#include <vector>

class Component
{
public:
    Component();
    ~Component();

public:
    int ParseJson(const char* sz_json, int json_len, Json::Value &recv_packet);
    void InitSwitch();

    const vector<vector<FieldInfo> >& OrKeys();
    const vector<vector<FieldInfo> >& AndKeys();
    const vector<vector<FieldInfo> >& InvertKeys();
    const vector<ExtraFilterKey>& ExtraFilterKeys();
    const vector<ExtraFilterKey>& ExtraFilterAndKeys();
    const vector<ExtraFilterKey>& ExtraFilterInvertKeys();
    
    uint32_t Appid();
    uint32_t SortType();
    uint32_t PageIndex();
    uint32_t PageSize();
    uint32_t ReturnAll();
    uint32_t CacheSwitch();
    uint32_t TopSwitch();
    uint32_t SnapshotSwitch();
    std::string SortField();
    std::string LastId();
    std::string LastScore();
    bool SearchAfter();
    std::vector<std::string>& RequiredFields();
    uint32_t TerminalTag();
    Json::Value& GetQuery();

    void AddToFieldList(int type, std::vector<FieldInfo>& fields);

    void SetHasGisFlag(bool bFlag) { has_gis_ = bFlag; };
    bool GetHasGisFlag() { return has_gis_; };

private:
    std::vector<std::vector<FieldInfo> > or_keys_;
    std::vector<std::vector<FieldInfo> > and_keys_;
    std::vector<std::vector<FieldInfo> > invert_keys_;
    std::vector<ExtraFilterKey> extra_filter_keys_;
    std::vector<ExtraFilterKey> extra_filter_and_keys_;
    std::vector<ExtraFilterKey> extra_filter_invert_keys_;

    uint32_t page_index_;
    uint32_t page_size_;

    uint32_t cache_switch_;
    uint32_t snapshot_switch_;
    uint32_t sort_type_;
    uint32_t appid_;
    std::string sort_field_;
    std::string last_id_;
    std::string last_score_;
    bool search_after_;
    std::vector<std::string> required_fields_;
    uint32_t preterminal_tag_;
    Json::Value query_value_;
    bool has_gis_;
};
#endif