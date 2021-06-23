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
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "request_context.h"
#include "split_manager.h"
#include "db_manager.h"
#include "utf8_str.h"
#include <sstream>

RequestContext::RequestContext()
    : or_keys_()
    , and_keys_()
    , invert_keys_()
    , extra_filter_or_keys_()
    , extra_filter_and_keys_()
    , extra_filter_invert_keys_()
    , page_index_(0)
    , page_size_(0)
    , cache_switch_(0)
    , snapshot_switch_(0)
    , sort_type_(SORT_RELEVANCE)
    , appid_(10001)
    , return_all_(0)
    , sort_field_("")
    , last_id_("")
    , last_score_("")
    , search_after_(false)
    , required_fields_()
    , preterminal_tag_(0)
    , query_value_()
    , has_gis_(false)
{ }

RequestContext::~RequestContext(){

}

int RequestContext::ParseJson(const char *sz_json, int json_len, Json::Value &recv_packet)
{
    Json::Reader r(Json::Features::strictMode());
    int ret;
    ret = r.parse(sz_json, sz_json + json_len, recv_packet);
    if (0 == ret)
    {
        log_error("the err json string is : %s", sz_json);
        log_error("parse json error , errmsg : %s", r.getFormattedErrorMessages().c_str());
        return -RT_PARSE_JSON_ERR;
    }

    if (recv_packet.isMember("appid") && recv_packet["appid"].isUInt())
    {
        appid_ = recv_packet["appid"].asUInt();
    }
    else {
        appid_ = 10001;
    }

    if(recv_packet.isMember("query")){
        query_value_ = recv_packet["query"];
    }

    if (recv_packet.isMember("page_index") && recv_packet["page_index"].isString())
    {
        page_index_ = atoi(recv_packet["page_index"].asString().c_str());
    }
    else {
        page_index_ = 1 ;
    }

    if (recv_packet.isMember("page_size") && recv_packet["page_size"].isString())
    {
        page_size_ = atoi(recv_packet["page_size"].asString().c_str());
    }
    else {
        page_size_ = 10;
    }

    if(recv_packet.isMember("sort_type") && recv_packet["sort_type"].isString())
    {
        sort_type_ = atoi(recv_packet["sort_type"].asString().c_str());
    }
    else {
        sort_type_ = SORT_RELEVANCE;
    }

    if(recv_packet.isMember("sort_field") && recv_packet["sort_field"].isString())
    {
        sort_field_ = recv_packet["sort_field"].asString();
    }
    else {
        sort_field_ = "";
    }

    if (recv_packet.isMember("return_all") && recv_packet["return_all"].isString())
    {
        return_all_ = atoi(recv_packet["return_all"].asString().c_str());
    }
    else {
        return_all_ = 0;
    }

    if(recv_packet.isMember("fields") && recv_packet["fields"].isString())
    {
        std::string fields = recv_packet["fields"].asString();
        required_fields_ = splitEx(fields, ",");
    }

    if (recv_packet.isMember("terminal_tag") && recv_packet["terminal_tag"].isString())
    {
        preterminal_tag_ = atoi(recv_packet["terminal_tag"].asString().c_str());
    }
    else {
        preterminal_tag_ = 0;
    }

    if(recv_packet.isMember("last_id") && recv_packet["last_id"].isString())
    {
        last_id_ = recv_packet["last_id"].asString();
    }
    else {
        last_id_ = "";
    }

    bool score_flag = true;
    if (recv_packet.isMember("last_score") && recv_packet["last_score"].isString())
    {
        last_score_ = recv_packet["last_score"].asString();
    }
    else {
        score_flag = false;
        last_score_ = "0";
    }
    if(last_id_ != "" && score_flag == true){
        search_after_ = true;
    }
    if(search_after_ == true && sort_type_ != SORT_FIELD_DESC && sort_type_ != SORT_FIELD_ASC){
        log_error("in search_after mode, sort_type must be SORT_FIELD_DESC or SORT_FIELD_ASC.");
        return -RT_PARSE_JSON_ERR;
    }

    return 0;
}

void RequestContext::InitSwitch()
{
    AppInfo app_info;
    bool res = SearchConf::Instance()->GetAppInfo(appid_, app_info);
    if (true == res){
        cache_switch_ = app_info.cache_switch;
        snapshot_switch_ = app_info.snapshot_switch;
    }
}

void RequestContext::AddToFieldList(int type, vector<FieldInfo>& fields)
{
    if (fields.size() == 0)
        return ;
    if (type == ORKEY) {
        or_keys_.push_back(fields);
    } else if (type == ANDKEY) {
        and_keys_.push_back(fields);
    } else if (type == INVERTKEY) {
        invert_keys_.push_back(fields);
    }
    return ;
}

const std::vector<std::vector<FieldInfo> >& RequestContext::GetFieldList(int logic_type){
    if (ORKEY == logic_type){
        return or_keys_;
    }else if (ANDKEY == logic_type){
        return and_keys_;
    }else if (INVERTKEY == logic_type){
        return invert_keys_;
    }
}

void RequestContext::AddToExtraFieldList(int type , const ExtraFilterKey& extra_field){
    if (ORKEY == type){
        extra_filter_or_keys_.push_back(extra_field);
    }else if (ANDKEY == type){
        extra_filter_and_keys_.push_back(extra_field);
    }else if (INVERTKEY == type){
        extra_filter_invert_keys_.push_back(extra_field);
    }
    return;
}

const std::vector<std::vector<FieldInfo> >& RequestContext::OrKeys(){
    return or_keys_;
}

std::vector<std::vector<FieldInfo> >& RequestContext::AndKeys(){
    return and_keys_;
}

const std::vector<std::vector<FieldInfo> >& RequestContext::InvertKeys(){
    return invert_keys_;
}

const std::vector<ExtraFilterKey>& RequestContext::ExtraFilterOrKeys(){
    return extra_filter_or_keys_;
}

const std::vector<ExtraFilterKey>& RequestContext::ExtraFilterAndKeys(){
    return extra_filter_and_keys_;
}

const std::vector<ExtraFilterKey>& RequestContext::ExtraFilterInvertKeys(){
    return extra_filter_invert_keys_;
}

uint32_t RequestContext::Appid(){
    return appid_;
}

uint32_t RequestContext::SortType(){
    return sort_type_;
}

uint32_t RequestContext::PageIndex(){
    return page_index_;
}
uint32_t RequestContext::PageSize(){
    return page_size_;
}

uint32_t RequestContext::ReturnAll(){
    return return_all_;
}

uint32_t RequestContext::CacheSwitch(){
    return cache_switch_;
}

uint32_t RequestContext::SnapshotSwitch(){
    return snapshot_switch_;
}

string RequestContext::SortField(){
    return sort_field_;
}

string RequestContext::LastId(){
    return last_id_;
}

string RequestContext::LastScore(){
    return last_score_;
}

bool RequestContext::SearchAfter(){
    return search_after_;
}

vector<string>& RequestContext::RequiredFields(){
    return required_fields_;
}

uint32_t RequestContext::TerminalTag(){
    return preterminal_tag_;
}

Json::Value& RequestContext::GetQuery(){
    return query_value_;
}