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

#include "valid_doc_filter.h"
#include "search_util.h"
#include "cachelist_unit.h"
#include "data_manager.h"
#include "json/reader.h"
#include "json/writer.h"
#include "index_tbl_op.h"
#include "index_sync/sync_index_timer.h"
#include "index_sync/sequence_search_index.h"
#include "stem.h"
#include "key_format.h"
#include <sstream>
#include <iomanip>

extern SyncIndexTimer* globalSyncIndexTimer;
extern CCacheListUnit* indexcachelist;

ValidDocFilter::ValidDocFilter()
    : p_data_base_(NULL)
{ }

ValidDocFilter::~ValidDocFilter()
{ }

//汉拼无需memcomparable format
int ValidDocFilter::HanPinTextInvertIndexSearch(const std::vector<FieldInfo>& keys
                    , std::vector<IndexInfo>& index_info_vet){
        if (keys.empty()){
            return -RT_GET_FIELD_ERROR;
        }
        std::vector<FieldInfo>::const_iterator iter = keys.cbegin();
        for (; iter != keys.cend(); ++iter){
            std::vector<IndexInfo> doc_info;
            if ((iter->segment_tag) == SEGMENT_CHINESE) {
                int ret = GetDocByShiftWord(*iter, doc_info, p_data_base_->Appid());
                if (ret != 0) {
                    index_info_vet.clear();
                    return -RT_GET_DOC_ERR;
                }
                std::sort(doc_info.begin(), doc_info.end());
                for (size_t doc_info_idx = 0; doc_info_idx < doc_info.size(); doc_info_idx++){
                    KeyInfo info;
                    info.word_freq = 1;
                    info.field = (iter->field);
                    info.word = (iter->word);
                    ResultContext::Instance()->SetDocKeyinfoMap(doc_info[doc_info_idx].doc_id , info);
                }
            } else if ((iter->segment_tag) == SEGMENT_ENGLISH) {
                int ret = GetDocByShiftEnWord(*iter, doc_info, p_data_base_->Appid());
                if (ret != 0) {
                    index_info_vet.clear();
                    return -RT_GET_DOC_ERR;
                }
                std::sort(doc_info.begin(), doc_info.end());
                for (size_t doc_info_idx = 0; doc_info_idx < doc_info.size(); doc_info_idx++){
                    KeyInfo info;
                    info.word_freq = 1;
                    info.field = (iter->field);
                    info.word = (iter->word);
                    ResultContext::Instance()->SetDocKeyinfoMap(doc_info[doc_info_idx].doc_id , info);
                }
            }
            index_info_vet = Union(index_info_vet, doc_info);
        }
        return 0;
}

int ValidDocFilter::RangeQueryInvertIndexSearch(const std::vector<FieldInfo>& keys
                    , std::vector<IndexInfo>& index_info_vet){
        if (keys.empty()){
            return -RT_GET_DOC_ERR;
        }
        std::vector<FieldInfo>::const_iterator iter = keys.cbegin();
        for (; iter != keys.cend(); ++iter){
            std::vector<IndexInfo> doc_info;
            log_debug("segment:%d , word:%s" , iter->segment_tag ,iter->word.c_str());
            if (SEGMENT_RANGE == (iter->segment_tag) && (iter->word).empty()){
                std::stringstream ss;
                ss << p_data_base_->Appid();
                InvertIndexEntry startEntry(ss.str(), iter->field, (double)(iter->start));
                InvertIndexEntry endEntry(ss.str(), iter->field, (double)(iter->end));
                std::vector<InvertIndexEntry> resultEntry;
                globalSyncIndexTimer->GetSearchIndex()->GetRangeIndex(iter->range_type, startEntry, endEntry, resultEntry);
                std::vector<InvertIndexEntry>::iterator res_iter = resultEntry.begin();
                for (; res_iter != resultEntry.end(); res_iter ++) {
                    IndexInfo info;
                    info.doc_id = res_iter->_InvertIndexDocId;
                    info.doc_version = res_iter->_InvertIndexDocVersion;
                    doc_info.push_back(info);
                }
                log_debug("appid: %s, field: %d, count: %d", startEntry._InvertIndexAppid.c_str(), iter->field, (int)resultEntry.size());
            }
            index_info_vet = Union(index_info_vet, doc_info);
        }
        return 0;
}

int ValidDocFilter::TextInvertIndexSearch(const std::vector<FieldInfo>& keys, std::vector<IndexInfo>& index_info_vet){
        if (keys.empty()){
            return -RT_GET_FIELD_ERROR;
        }

        std::vector<FieldInfo>::const_iterator iter = keys.cbegin();
        for (; iter != keys.cend(); ++iter){
            std::vector<IndexInfo> doc_info;
            int ret = GetDocIdSetByWord(*iter, doc_info);
            if (ret != 0){
                return -RT_GET_DOC_ERR;
            }
            if (doc_info.size() == 0)
                continue;
            if (!p_data_base_->GetHasGisFlag() || !isAllNumber(iter->word)){
                if (iter->field_type != FIELD_INDEX){
                    ResultContext::Instance()->SetHighLightWordSet(iter->word);
                }
            }
            if(!p_data_base_->GetHasGisFlag() && (SORT_RELEVANCE == p_data_base_->SortType() || SORT_TIMESTAMP == p_data_base_->SortType())){
                CalculateByWord(*iter, doc_info);
            }
            index_info_vet = Union(index_info_vet, doc_info);
        }
        return 0;
}

int ValidDocFilter::ProcessTerminal(const std::vector<std::vector<FieldInfo> >& and_keys, const TerminalQryCond& query_cond, std::vector<TerminalRes>& vecs){
    if(and_keys.size() != 1){
        return 0;
    }
    std::vector<FieldInfo> field_vec = and_keys[0];
    if(field_vec.size() != 1){
        return 0;
    }
    FieldInfo field_info = field_vec[0];
    if(field_info.segment_tag != SEGMENT_RANGE){
        return 0;
    }

    std::stringstream ss;
    ss << p_data_base_->Appid();
    InvertIndexEntry beginEntry(ss.str(), field_info.field, (double)field_info.start);
    InvertIndexEntry endEntry(ss.str(), field_info.field, (double)field_info.end);
    std::vector<InvertIndexEntry> resultEntry;
    globalSyncIndexTimer->GetSearchIndex()->GetRangeIndexInTerminal(field_info.range_type, beginEntry, endEntry, query_cond, resultEntry);
    std::vector<InvertIndexEntry>::iterator iter = resultEntry.begin();
    for (; iter != resultEntry.end(); iter ++) {
        TerminalRes info;
        info.doc_id = (*iter)._InvertIndexDocId;
        info.score = (*iter)._InvertIndexKey;
        vecs.push_back(info);
    }
    return 0;
}

void ValidDocFilter::CalculateByWord(FieldInfo fieldInfo, const std::vector<IndexInfo>& doc_info) {
    std::vector<IndexInfo>::const_iterator iter = doc_info.cbegin();
    for ( ; iter != doc_info.cend(); ++iter) {
        std::string pos_str = iter->pos;
        std::vector<int> pos_vec;
        if (pos_str != "" && pos_str.size() > 2) {
            pos_str = pos_str.substr(1, pos_str.size() - 2);
            pos_vec = splitInt(pos_str, ",");
        }
        KeyInfo info;
        info.word_freq = iter->word_freq;
        info.field = iter->field;
        info.word = fieldInfo.word;
        info.created_time = iter->created_time;
        info.pos_vec = pos_vec;
        ResultContext::Instance()->SetDocKeyinfoMap(iter->doc_id , info);
    }
    ResultContext::Instance()->SetWordDoccountMap(fieldInfo.word , doc_info.size());
}


bool ValidDocFilter::GetDocIndexCache(std::string word, uint32_t field, std::vector<IndexInfo>& doc_info) {
    log_debug("get doc index start");
    bool res = false;
    uint8_t value[MAX_VALUE_LEN] = { 0 };
    unsigned vsize = 0;
    std::string output = "";
    std::string indexCache = word + "|" + ToString(field);
    if (p_data_base_->CacheSwitch() == 1 && indexcachelist->in_list(indexCache.c_str(), indexCache.size(), value, vsize))
    {
        log_debug("hit index cache.");
        value[vsize] = '\0';
        output = (char *)value;
        res = true;
    }

    if (res) {
        Json::Value packet;
        Json::Reader r(Json::Features::strictMode());
        int ret;
        ret = r.parse(output.c_str(), output.c_str() + output.size(), packet);
        if (0 == ret)
        {
            log_error("the err json string is : %s, errmsg : %s", output.c_str(), r.getFormattedErrorMessages().c_str());
            res = false;
            return res;
        }

        for (uint32_t i = 0; i < packet.size(); ++i) {
            IndexInfo info;
            Json::Value& index_cache = packet[i];
            if (index_cache.isMember("appid")   && index_cache["appid"].isUInt()   && 
                index_cache.isMember("id")      && index_cache["id"].isString()    &&
                index_cache.isMember("version") && index_cache["version"].isUInt() && 
                index_cache.isMember("field")   && index_cache["field"].isUInt()   && 
                index_cache.isMember("freq")    && index_cache["freq"].isUInt()    && 
                index_cache.isMember("time")    && index_cache["time"].isUInt()    && 
                index_cache.isMember("pos")     && index_cache["pos"].isString()   &&
                index_cache.isMember("extend")  && index_cache["extend"].isString())
            {
                info.appid = index_cache["appid"].asUInt();
                info.doc_id = index_cache["id"].asString();
                info.doc_version = index_cache["version"].asUInt();
                info.field = index_cache["field"].asUInt();
                info.word_freq = index_cache["freq"].asUInt();
                info.created_time = index_cache["time"].asUInt();
                info.pos = index_cache["pos"].asString();
                info.extend = index_cache["extend"].asString();
                doc_info.push_back(info);
            }
            else {
                log_error("parse index_cache error, no appid");
                doc_info.clear();
                res = false;
                break;
            }
        }
    }
    return res;
}

void ValidDocFilter::SetDocIndexCache(const std::vector<IndexInfo>& doc_info, std::string& indexJsonStr) {
    Json::Value indexJson;
    std::vector<IndexInfo>::const_iterator iter = doc_info.cbegin();
    for ( ; iter != doc_info.cend(); ++iter) {
        Json::Value json_tmp;
        json_tmp["appid"] = iter->appid;
        json_tmp["id"] = iter->doc_id;
        json_tmp["version"] = iter->doc_version;
        json_tmp["field"] = iter->field;
        json_tmp["freq"] = iter->word_freq;
        json_tmp["time"] = iter->created_time;
        json_tmp["pos"] = iter->pos;
        json_tmp["extend"] = iter->extend;
        indexJson.append(json_tmp);
    }

    Json::FastWriter writer;
    indexJsonStr = writer.write(indexJson);
}

int ValidDocFilter::GetDocIdSetByWord(FieldInfo fieldInfo, std::vector<IndexInfo> &doc_info) {
    bool bRet = false;
    if (DataManager::Instance()->IsSensitiveWord(fieldInfo.word)) {
        log_debug("%s is a sensitive word.", fieldInfo.word.c_str());
        return 0;
    }

    std::stringstream ss_key;
    ss_key << p_data_base_->Appid();
    ss_key << "#00#";

    if (FIELD_IP == fieldInfo.field_type) {
        uint32_t word_id = GetIpNum(fieldInfo.word);
        if (word_id == 0) { return 0; }
        std::stringstream stream_ip;
        stream_ip << word_id;
        fieldInfo.word = stream_ip.str();
    }
    
    // 联合索引MemFormat在拼接的时候已经完成，此处无需再次编码
    if(FIELD_INDEX == fieldInfo.field_type){
        ss_key << fieldInfo.word;
    }else {
        KeyFormat::UnionKey o_keyinfo_vet;
        o_keyinfo_vet.push_back(std::make_pair(fieldInfo.field_type , fieldInfo.word));
        std::string s_format_key = KeyFormat::Encode(o_keyinfo_vet);
        ss_key << s_format_key;
    }

    log_debug("appid [%u], key[%s]", p_data_base_->Appid(), ss_key.str().c_str());
    if (p_data_base_->GetHasGisFlag() && GetDocIndexCache(ss_key.str(), fieldInfo.field, doc_info)) {
        return 0;
    }

    bRet = g_IndexInstance.GetDocInfo(p_data_base_->Appid(), ss_key.str(), fieldInfo.field, doc_info);
    if (false == bRet) {
        log_error("GetDocInfo error.");
        return -RT_DTC_ERR;
    }

    if (p_data_base_->CacheSwitch() == 1 && p_data_base_->GetHasGisFlag() == 1 
                && doc_info.size() > 0 && doc_info.size() <= 1000) {
        std::string index_str;
        SetDocIndexCache(doc_info, index_str);
        if (index_str != "" && index_str.size() < MAX_VALUE_LEN) {
            std::string indexCache = ss_key.str() + "|" + ToString(fieldInfo.field);
            unsigned data_size = indexCache.size();
            int ret = indexcachelist->add_list(indexCache.c_str(), index_str.c_str(), data_size, index_str.size());
            if (ret != 0) {
                log_error("add to index_cache_list error, ret: %d.", ret);
            }
            else {
                log_debug("add to index_cache_list: %s.", indexCache.c_str());
            }
        }
    }
    return 0;
}

std::vector<IndexInfo> ValidDocFilter::Union(std::vector<IndexInfo>& first_indexinfo_vet, std::vector<IndexInfo>& second_indexinfo_vet){
    std::vector<IndexInfo> index_info_result;
    int i_max_size = first_indexinfo_vet.size() + second_indexinfo_vet.size();
    index_info_result.resize(i_max_size);

    std::sort(first_indexinfo_vet.begin() , first_indexinfo_vet.end());
    std::sort(second_indexinfo_vet.begin() , second_indexinfo_vet.end());

    std::set_union(first_indexinfo_vet.begin(), first_indexinfo_vet.end()
                ,second_indexinfo_vet.begin() , second_indexinfo_vet.end() , index_info_result.begin());

    return index_info_result;
}