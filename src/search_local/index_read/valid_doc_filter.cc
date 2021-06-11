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
    , o_or_and_not_functor_()
{ }

ValidDocFilter::~ValidDocFilter()
{ }

// int ValidDocFilter::GetTopDocScore(std::map<std::string, double>& top_doc_score)
// {
//     std::vector<TopDocInfo> doc_info;
//     for (size_t index = 0; index < p_data_base_->OrKeys().size(); index++) {
//         vector<FieldInfo> topInfos = p_data_base_->OrKeys()[index];
//         vector<FieldInfo>::iterator iter;
//         for (iter = topInfos.begin(); iter != topInfos.end(); iter++) {
//             int ret = GetTopDocIdSetByWord(*iter, doc_info);
//             if (ret != 0) {
//                 return -RT_GET_DOC_ERR;
//             }
//         }
//     }

//     double score = 0;
//     for(size_t i = 0; i < doc_info.size(); i++)
//     {
//         score = (double)doc_info[i].weight;
//         if (p_data_base_->SortType() == DONT_SORT) {
//             score = 1;
//         } else if (p_data_base_->SortType() == SORT_TIMESTAMP) {
//             score = (double)doc_info[i].created_time;
//         }

//         top_doc_score[doc_info[i].doc_id] = score;
//     }

//     return 0;
// }

int ValidDocFilter::OrAndInvertKeyFilter(std::vector<IndexInfo>& index_info_vet
            , std::set<std::string>& highlightWord, std::map<std::string, KeyInfoVet>& docid_keyinfo_map
            , std::map<std::string, uint32_t>& key_doccount_map){
        int iret = 0;
        if (!p_data_base_->OrKeys().empty()){
            iret = OrKeyFilter(index_info_vet , highlightWord , docid_keyinfo_map , key_doccount_map);
            if (iret != 0){
                return iret;
            }
        }
        if (!p_data_base_->AndKeys().empty()){
            iret = AndKeyFilter(index_info_vet , highlightWord , docid_keyinfo_map , key_doccount_map);
            if (iret != 0){
                return iret;
            }
        }
        if (!p_data_base_->InvertKeys().empty()){
            iret = InvertKeyFilter(index_info_vet , highlightWord , docid_keyinfo_map , key_doccount_map);
            if (iret != 0){
                return iret;
            }
        }
        return iret;
}

// int ValidDocFilter::GetTopDocIdSetByWord(FieldInfo fieldInfo, std::vector<TopDocInfo>& doc_info){
//     if (DataManager::Instance()->IsSensitiveWord(fieldInfo.word)) {
//         log_debug("%s is a sensitive word.", fieldInfo.word.c_str());
//         return 0;
//     }

//     std::string word_new = stem(fieldInfo.word);
//     std::vector<TopDocInfo> no_filter_docs;
//     bool bRet = g_IndexInstance.GetTopDocInfo(p_data_base_->Appid(), word_new, no_filter_docs);
//     if (false == bRet) {
//         log_error("GetTopDocInfo error.");
//         return -RT_DTC_ERR;
//     }

//     if (0 == no_filter_docs.size())
//         return 0;

//     if (1 == p_data_base_->SnapshotSwitch()) {
//         bRet = g_IndexInstance.TopDocValid(p_data_base_->Appid(), no_filter_docs, doc_info);
//         if (false == bRet) {
//             log_error("GetTopDocInfo by snapshot error.");
//             return -RT_DTC_ERR;
//         }
//     }
//     else {
//         doc_info.swap(no_filter_docs);
//         // for (size_t i = 0; i < no_filter_docs.size(); i++)
//         // {
//         //     TopDocInfo info = no_filter_docs[i];
//         //     doc_info.push_back(info);
//         // }
//     }
//     return 0;
// }

int ValidDocFilter::OrKeyFilter(std::vector<IndexInfo>& index_info_vet
            , std::set<std::string>& highlightWord, std::map<std::string, KeyInfoVet>& docid_keyinfo_map
            , std::map<std::string, uint32_t>& key_doccount_map)
{
    std::vector<IndexInfo> or_vecs;
    o_or_and_not_functor_ = std::bind(vec_union , std::placeholders::_1, std::placeholders::_2);

    int ret = Process(p_data_base_->OrKeys(), or_vecs, highlightWord, docid_keyinfo_map, key_doccount_map);
    if (ret != 0) {
        log_debug("OrKeyFilter error.");
        return -RT_GET_DOC_ERR;
    }
    index_info_vet = vec_union(index_info_vet , or_vecs);

    if ((index_info_vet.size() == 0) && (p_data_base_->OrKeys().size() != 0)) {
        log_debug("search result of keys is empty.");
        return 0;
    }
    log_debug("OrKeyFilter begin: %lld.", (long long int)GetSysTimeMicros());
    return 0;
}

int ValidDocFilter::AndKeyFilter(std::vector<IndexInfo>& index_info_vet
            , std::set<std::string>& highlightWord, std::map<std::string, KeyInfoVet>& docid_keyinfo_map
            , std::map<std::string, uint32_t>& key_doccount_map)
{
    std::vector<IndexInfo> and_vecs;
    o_or_and_not_functor_ = std::bind(vec_intersection , std::placeholders::_1, std::placeholders::_2);

    int ret = Process(p_data_base_->AndKeys(), and_vecs, highlightWord, docid_keyinfo_map, key_doccount_map);
    if (ret != 0) {
        log_debug("AndKeyFilter error.");
        return -RT_GET_DOC_ERR;
    }

    if (index_info_vet.empty()){
        index_info_vet.assign(and_vecs.begin(),and_vecs.end());
    }else{
        index_info_vet = vec_intersection(index_info_vet , and_vecs);
    }

    if ((and_vecs.size() == 0) && (p_data_base_->AndKeys().size() != 0)) {
        log_debug("search result of and_keys is empty.");
        return 0;
    }
    log_debug("AndKeyFilter end: %lld.", (long long int)GetSysTimeMicros());
    return 0;
}

int ValidDocFilter::InvertKeyFilter(std::vector<IndexInfo>& index_info_vet
            , std::set<std::string>& highlightWord, std::map<std::string, KeyInfoVet>& docid_keyinfo_map
            , std::map<std::string, uint32_t>& key_doccount_map)
{
    std::vector<IndexInfo> invert_vecs;
    o_or_and_not_functor_ = std::bind(vec_union , std::placeholders::_1, std::placeholders::_2);

    int ret = Process(p_data_base_->InvertKeys(), invert_vecs, highlightWord, docid_keyinfo_map, key_doccount_map);
    if (ret != 0) {
        return -RT_GET_DOC_ERR;
    }
    index_info_vet = vec_difference(index_info_vet, invert_vecs);

    if (index_info_vet.size() == 0){
        return 0;
    }
    return 0;
}

int ValidDocFilter::Process(const std::vector<std::vector<FieldInfo> >& keys, std::vector<IndexInfo>& index_info_vet
        , std::set<std::string>& highlightWord, std::map<std::string, KeyInfoVet>& docid_keyinfo_map, std::map<std::string, uint32_t>& key_doccount_map){
    for (size_t index = 0; index < keys.size(); index++)
    {
        std::vector<IndexInfo> doc_id_vec;
        std::vector<FieldInfo> fieldInfos = keys[index];
        std::vector<FieldInfo>::iterator it;
        for (it = fieldInfos.begin(); it != fieldInfos.end(); it++) {
            std::vector<IndexInfo> doc_info;
            if ((*it).segment_tag == SEGMENT_CHINESE) {
                int ret = GetDocByShiftWord(*it, doc_info, p_data_base_->Appid(), highlightWord);
                if (ret != 0) {
                    doc_id_vec.clear();
                    return -RT_GET_DOC_ERR;
                }
                std::sort(doc_info.begin(), doc_info.end());
                for (size_t doc_info_idx = 0; doc_info_idx < doc_info.size(); doc_info_idx++){
                    KeyInfo info;
                    info.word_freq = 1;
                    info.field = (*it).field;
                    info.word = (*it).word;
                    docid_keyinfo_map[doc_info[doc_info_idx].doc_id].push_back(info);
                }
            } else if ((*it).segment_tag == SEGMENT_ENGLISH) {
                int ret = GetDocByShiftEnWord(*it, doc_info, p_data_base_->Appid(), highlightWord);
                if (ret != 0) {
                    doc_id_vec.clear();
                    return -RT_GET_DOC_ERR;
                }
                std::sort(doc_info.begin(), doc_info.end());
                for (size_t doc_info_idx = 0; doc_info_idx < doc_info.size(); doc_info_idx++){
                    KeyInfo info;
                    info.word_freq = 1;
                    info.field = (*it).field;
                    info.word = (*it).word;
                    docid_keyinfo_map[doc_info[doc_info_idx].doc_id].push_back(info);
                }
            } else if ((*it).segment_tag == SEGMENT_RANGE && (*it).word == "") { // 范围查询
                std::stringstream ss;
                ss << p_data_base_->Appid();
                InvertIndexEntry startEntry(ss.str(), (*it).field, (double)(*it).start);
                InvertIndexEntry endEntry(ss.str(), (*it).field, (double)(*it).end);
                std::vector<InvertIndexEntry> resultEntry;
                globalSyncIndexTimer->GetSearchIndex()->GetRangeIndex((*it).range_type, startEntry, endEntry, resultEntry);
                std::vector<InvertIndexEntry>::iterator iter = resultEntry.begin();
                for (; iter != resultEntry.end(); iter ++) {
                    IndexInfo info;
                    info.doc_id = (*iter)._InvertIndexDocId;
                    info.doc_version = (*iter)._InvertIndexDocVersion;
                    doc_info.push_back(info);
                }
                log_debug("appid: %s, field: %d, count: %d", startEntry._InvertIndexAppid.c_str(), (*it).field, (int)resultEntry.size());
            } else { // SEGMENT_NGRAM or SEGMENT_DEFAULT
                int ret = GetDocIdSetByWord(*it, doc_info);
                if (ret != 0){
                    return -RT_GET_DOC_ERR;
                }
                if (doc_info.size() == 0)
                    continue;
                if (!p_data_base_->GetHasGisFlag() || !isAllNumber((*it).word))
                    highlightWord.insert((*it).word);
                if(!p_data_base_->GetHasGisFlag() && (SORT_RELEVANCE == p_data_base_->SortType() || SORT_TIMESTAMP == p_data_base_->SortType())){
                    CalculateByWord(*it, doc_info, docid_keyinfo_map, key_doccount_map);
                }
            } 
            doc_id_vec = vec_union(doc_id_vec, doc_info);
        }
        if(index == 0){ // 第一个直接赋值给vecs，后续的依次与前面的进行逻辑运算
            index_info_vet.assign(doc_id_vec.begin(), doc_id_vec.end());
        } else {
            index_info_vet = o_or_and_not_functor_(index_info_vet, doc_id_vec);
        }
    }
    return 0;
}

//汉拼无需memcomparable format
int ValidDocFilter::HanPinTextInvertIndexSearch(const std::vector<std::vector<FieldInfo> >& keys
                    , std::vector<IndexInfo>& index_info_vet
                    , std::set<std::string>& highlightWord
                    , std::map<std::string, KeyInfoVet>& docid_keyinfo_map){
        if (keys.empty() || keys.size() > 1){
            return -RT_GET_FIELD_ERROR;
        }
        const std::vector<FieldInfo>& key_field_info_vet = keys[0];
        std::vector<FieldInfo>::const_iterator iter = key_field_info_vet.cbegin();
        for (; iter != key_field_info_vet.cend(); ++iter){
            std::vector<IndexInfo> doc_info;
            if ((iter->segment_tag) == SEGMENT_CHINESE) {
                int ret = GetDocByShiftWord(*iter, doc_info, p_data_base_->Appid(), highlightWord);
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
                    docid_keyinfo_map[doc_info[doc_info_idx].doc_id].push_back(info);
                }
            } else if ((iter->segment_tag) == SEGMENT_ENGLISH) {
                int ret = GetDocByShiftEnWord(*iter, doc_info, p_data_base_->Appid(), highlightWord);
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
                    docid_keyinfo_map[doc_info[doc_info_idx].doc_id].push_back(info);
                }
            }
            index_info_vet = vec_union(index_info_vet, doc_info);
        }
        return 0;
}

int ValidDocFilter::RangeQueryInvertIndexSearch(const std::vector<std::vector<FieldInfo> >& keys
                    , std::vector<IndexInfo>& index_info_vet){
        if (keys.empty() || keys.size() > 1){
            return -RT_GET_DOC_ERR;
        }
        const std::vector<FieldInfo>& key_field_info_vet = keys[0];
        std::vector<FieldInfo>::const_iterator iter = key_field_info_vet.cbegin();
        for (; iter != key_field_info_vet.cend(); ++iter){
            std::vector<IndexInfo> doc_info;
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
            index_info_vet = vec_union(index_info_vet, doc_info);
        }
        return 0;
}

int ValidDocFilter::TextInvertIndexSearch(const std::vector<std::vector<FieldInfo> >& keys
                    , std::vector<IndexInfo>& index_info_vet
                    , std::set<std::string>& highlightWord
                    , std::map<std::string, KeyInfoVet>& docid_keyinfo_map
                    , std::map<std::string, uint32_t>& key_doccount_map){
        if (keys.empty() || keys.size() > 1){
            return -RT_GET_FIELD_ERROR;
        }
        const std::vector<FieldInfo>& key_field_info_vet = keys[0];
        std::vector<FieldInfo>::const_iterator iter = key_field_info_vet.cbegin();
        for (; iter != key_field_info_vet.cend(); ++iter){
            std::vector<IndexInfo> doc_info;
            int ret = GetDocIdSetByWord(*iter, doc_info);
            if (ret != 0){
                return -RT_GET_DOC_ERR;
            }
            if (doc_info.size() == 0)
                continue;
            if (!p_data_base_->GetHasGisFlag() || !isAllNumber(iter->word))
                highlightWord.insert(iter->word);
            if(!p_data_base_->GetHasGisFlag() && (SORT_RELEVANCE == p_data_base_->SortType() || SORT_TIMESTAMP == p_data_base_->SortType())){
                CalculateByWord(*iter, doc_info, docid_keyinfo_map, key_doccount_map);
            }
            index_info_vet = vec_union(index_info_vet, doc_info);
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

void ValidDocFilter::CalculateByWord(FieldInfo fieldInfo, const std::vector<IndexInfo> &doc_info
                , std::map<std::string, KeyInfoVet> &ves, std::map<std::string, uint32_t> &key_in_doc) {
    std::string doc_id;
    uint32_t word_freq = 0;
    uint32_t field = 0;
    uint32_t created_time;
    std::string pos_str = "";
    for (size_t i = 0; i < doc_info.size(); i++) {
        doc_id = doc_info[i].doc_id;
        word_freq = doc_info[i].word_freq;
        field = doc_info[i].field;
        created_time = doc_info[i].created_time;
        pos_str = doc_info[i].pos;
        std::vector<int> pos_vec;
        if (pos_str != "" && pos_str.size() > 2) {
            pos_str = pos_str.substr(1, pos_str.size() - 2);
            pos_vec = splitInt(pos_str, ",");
        }
        KeyInfo info;
        info.word_freq = word_freq;
        info.field = field;
        info.word = fieldInfo.word;
        info.created_time = created_time;
        info.pos_vec = pos_vec;
        ves[doc_id].push_back(info);
    }
    key_in_doc[fieldInfo.word] = doc_info.size();
}


bool ValidDocFilter::GetDocIndexCache(std::string word, uint32_t field, std::vector<IndexInfo> &doc_info) {
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
                index_cache.isMember("pos")     && index_cache["pos"].isString())
            {
                info.appid = index_cache["appid"].asUInt();
                info.doc_id = index_cache["id"].asString();
                info.doc_version = index_cache["version"].asUInt();
                info.field = index_cache["field"].asUInt();
                info.word_freq = index_cache["freq"].asUInt();
                info.created_time = index_cache["time"].asUInt();
                info.pos = index_cache["pos"].asString();
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

void ValidDocFilter::SetDocIndexCache(const std::vector<IndexInfo> &doc_info, std::string& indexJsonStr) {
    Json::Value indexJson;
    Json::FastWriter writer;
    for (size_t i = 0; i < doc_info.size(); i++) {
        Json::Value json_tmp;
        json_tmp["appid"] = doc_info[i].appid;
        json_tmp["id"] = doc_info[i].doc_id;
        json_tmp["version"] = doc_info[i].doc_version;
        json_tmp["field"] = doc_info[i].field;
        json_tmp["freq"] = doc_info[i].word_freq;
        json_tmp["time"] = doc_info[i].created_time;
        json_tmp["pos"] = doc_info[i].pos;
        indexJson.append(json_tmp);
    }
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