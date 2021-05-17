/*
 * =====================================================================================
 *
 *       Filename:  search_task.h
 *
 *    Description:  search task class definition.
 *
 *        Version:  1.0
 *        Created:  19/03/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "split_manager.h"
#include "search_util.h"
#include "search_task.h"
#include "json/reader.h"
#include "json/writer.h"
#include "timemanager.h"
#include "cpa_md5.h"
#include "data_manager.h"
#include "stem.h"
#include "result_cache.h"
#include "cachelist_unit.h"
#include <netinet/in.h>
#include <algorithm>
#include <set>
#include <sstream>
#include <fstream>
#include <math.h>
#include "stat_index.h"
#include "db_manager.h"
#include "utf8_str.h"
#include "monitor.h"
#include "index_sync/sync_index_timer.h"
#include "index_sync/sequence_search_index.h"
#include "order_op.h"

#include "process/match_query_process.h"

using namespace std;
#define DOC_CNT 10000

typedef pair<string, double> PAIR;
extern CCacheListUnit *cachelist;
extern SyncIndexTimer *globalSyncIndexTimer;

struct CmpByValue {
    bool operator()(const PAIR& lhs, const PAIR& rhs) {
        if(fabs(lhs.second - rhs.second) < 0.000001){
            return lhs.first.compare(rhs.first) > 0;
        }
        return lhs.second > rhs.second;
    }
};

SearchTask::SearchTask()
{
    m_index_set_cnt = 0;
    m_has_gis = 0;
    component = new Component();
}

int SearchTask::GetTopDocIdSetByWord(FieldInfo fieldInfo, vector<TopDocInfo>& doc_info) {
    if (DataManager::Instance()->IsSensitiveWord(fieldInfo.word)) {
        log_debug("%s is a sensitive word.", fieldInfo.word.c_str());
        return 0;
    }

    string word_new = stem(fieldInfo.word);
    bool bRet = false;
    vector<TopDocInfo> no_filter_docs;
    bRet = g_IndexInstance.GetTopDocInfo(m_appid, word_new, no_filter_docs);
    if (false == bRet) {
        log_error("GetTopDocInfo error.");
        return -RT_DTC_ERR;
    }

    if (0 == no_filter_docs.size())
        return 0;

    if (component->SnapshotSwitch() == 1) {
        bRet = g_IndexInstance.TopDocValid(m_appid, no_filter_docs, doc_info);
        if (false == bRet) {
            log_error("GetTopDocInfo by snapshot error.");
            return -RT_DTC_ERR;
        }
    }
    else {
        for (size_t i = 0; i < no_filter_docs.size(); i++)
        {
            TopDocInfo info = no_filter_docs[i];
            doc_info.push_back(info);
        }
    }
    return 0;
}

int SearchTask::GetTopDocScore(map<string, double>& top_doc_score)
{
    vector<TopDocInfo> doc_info;
    for (size_t index = 0; index < component->Keys().size(); index++) {
        vector<FieldInfo> topInfos = component->Keys()[index];
        vector<FieldInfo>::iterator iter;
        for (iter = topInfos.begin(); iter != topInfos.end(); iter++) {
            int ret = GetTopDocIdSetByWord(*iter, doc_info);
            if (ret != 0) {
                return -RT_GET_DOC_ERR;
            }
        }
    }

    double score = 0;
    for(size_t i = 0; i < doc_info.size(); i++)
    {
        score = (double)doc_info[i].weight;
        if (m_sort_type == DONT_SORT) {
            score = 1;
        } else if (m_sort_type == SORT_TIMESTAMP) {
            score = (double)doc_info[i].created_time;
        }

        top_doc_score[doc_info[i].doc_id] = score;
    }

    return 0;
}

int SearchTask::GetValidDoc(map<string, vec> &ves, vector<string> &word_vec, map<string, uint32_t> &key_in_doc, hash_double_map &distances, set<string> &valid_docs){
    vector<IndexInfo> doc_id_ver_vec; // 最终求完交集并集差集的结果

    // key_or
    vector<IndexInfo> or_vecs;
    logical_operate->SetFunc(vec_union);
    int ret = logical_operate->Process(component->Keys(), or_vecs, highlightWord, ves, key_in_doc);
    if (ret != 0) {
        log_debug("logical_operate error.");
        return -RT_GET_DOC_ERR;
    }
    doc_id_ver_vec.assign(or_vecs.begin(), or_vecs.end());

    if ((doc_id_ver_vec.size() == 0) && (component->Keys().size() != 0)) {
        log_debug("search result of keys is empty.");
        return 0;
    }
    log_debug("logical_operate begin: %lld.", (long long int)GetSysTimeMicros());
    // key_and
    vector<IndexInfo> and_vecs;
    logical_operate->SetFunc(vec_intersection);
    ret = logical_operate->Process(component->AndKeys(), and_vecs, highlightWord, ves, key_in_doc);
    if (ret != 0) {
        log_debug("logical_operate error.");
        return -RT_GET_DOC_ERR;
    }

    if ((and_vecs.size() == 0) && (component->AndKeys().size() != 0)) {
        log_debug("search result of and_keys is empty.");
        return 0;
    }

    if(component->AndKeys().size() != 0){
        if(component->Keys().size() != 0){
            doc_id_ver_vec = vec_intersection(and_vecs, doc_id_ver_vec);
        } else {
            doc_id_ver_vec.assign(and_vecs.begin(), and_vecs.end());
        }
    }
    log_debug("logical_operate end: %lld.", (long long int)GetSysTimeMicros());
    // key_complete
    vector<IndexInfo> complete_vecs;
    ret = logical_operate->ProcessComplete(complete_keys, complete_vecs, word_vec, ves, key_in_doc);
    if (ret != 0) {
        return -RT_GET_DOC_ERR;
    }
    
    if ((complete_vecs.size() == 0) && (complete_keys.size() != 0)) {
        log_debug("search result of complete_keys is empty.");
        return 0;
    }

    if(complete_keys.size() != 0){
        if(component->AndKeys().size() == 0 && component->Keys().size() == 0){
            doc_id_ver_vec.assign(complete_vecs.begin(), complete_vecs.end());
        } else {
            doc_id_ver_vec = vec_intersection(doc_id_ver_vec, complete_vecs);
        }
    }

    // key_invert，多个字段的结果先求并集，最后一起求差集
    vector<IndexInfo> invert_vecs;
    logical_operate->SetFunc(vec_union);
    ret = logical_operate->Process(component->InvertKeys(), invert_vecs, highlightWord, ves, key_in_doc);
    if (ret != 0) {
        return -RT_GET_DOC_ERR;
    }
    doc_id_ver_vec = vec_difference(doc_id_ver_vec, invert_vecs);

    if (doc_id_ver_vec.size() == 0){
        return 0;
    }

    bool bRet = doc_manager->GetDocContent(m_has_gis, doc_id_ver_vec, valid_docs, distances);
    if (false == bRet) {
        log_error("GetDocContent error.");
        return -RT_DTC_ERR;
    }

    return 0;
}

int SearchTask::GetDocScore(map<string, double>& top_doc_score) 
{
    /***
        关键词搜索的策略是：
        1）如果主查询词（包括keys,and_keys,complete_keys等）不为空，但查询结果为空，则表示没有符合查询条件的结果
        2）如果主查询词（包括keys,and_keys,complete_keys等）不为空，查询结果也不为空，则表示有符合查询条件的结果，记为S
        3）若域字段为空，则直接返回S，若域字段不为空，则需将S与域搜索结果F进行AND运算
        4）如果主查询词（包括keys,and_keys,complete_keys等）为空，则直接返回域搜索结果F
    ***/

    map<string, vec> ves; // statistic word information in the latitude of documents
    vector<string> word_vec;
    map<string, uint32_t> key_in_doc; // how many documents contains key
    hash_double_map distances;
    set<string> valid_docs;
    int ret = GetValidDoc(ves, word_vec, key_in_doc, distances, valid_docs);
    if (ret != 0){
        log_error("GetValidDoc error.");
        return -RT_GET_DOC_ERR;
    }
    log_debug("GetValidDoc end: %lld. valid_docs size: %d.", (long long int)GetSysTimeMicros(), (int)valid_docs.size());

    // BM25 algorithm
    uint32_t doc_cnt = DOC_CNT;
    double k1 = 1.2;
    double k2 = 200;
    double K = 1.65;
    string doc_id;
    string keyword;
    uint32_t word_freq = 0;
    uint32_t field = 0;

    if(m_sort_type == SORT_RELEVANCE || m_sort_type == SORT_TIMESTAMP){
        if(m_has_gis){
            hash_double_map::iterator dis_iter = distances.begin();
            for(; dis_iter != distances.end(); dis_iter++){
                doc_id = dis_iter->first;
                double score = dis_iter->second;
                if ((component->Distance() > -0.0001 && component->Distance() < 0.0001) || (score + 1e-6 <= component->Distance())){
                    skipList.InsertNode(score, doc_id.c_str());
                }
            }
        } else {
            map<string, vec>::iterator ves_iter = ves.begin();
            for (; ves_iter != ves.end(); ves_iter++) {
                double score = 0;
                uint32_t key_docs = 0;
                
                doc_id = ves_iter->first;
                vector<KeyInfo> &key_info = ves_iter->second;
                if(valid_docs.find(doc_id) == valid_docs.end()){
                    continue;
                } 

                if (m_sort_type == SORT_TIMESTAMP) {  //按照时间排序
                    score = (double)key_info[0].created_time;
                    skipList.InsertNode(score, doc_id.c_str());
                    continue;
                }

                set<string> word_set;
                map<string, vector<int> > pos_map;
                map<string, vector<int> > title_pos_map;
                for (uint32_t i = 0; i < key_info.size(); i++) {
                    keyword = key_info[i].word;
                    if (word_set.find(keyword) == word_set.end()) {
                        word_set.insert(keyword);
                    }
                    word_freq = key_info[i].word_freq;
                    field = key_info[i].field;
                    if (field == LOCATE_ANY) {
                        pos_map[keyword] = key_info[i].pos_vec;
                    }
                    if (field == LOCATE_TITLE) {
                        title_pos_map[keyword] = key_info[i].pos_vec;
                    }
                    key_docs = key_in_doc[keyword];
                    score += log((doc_cnt - key_docs + 0.5) / (key_docs + 0.5)) * ((k1 + 1)*word_freq) / (K + word_freq) * (k2 + 1) * 1 / (k2 + 1);
                }
                if (!complete_keys.empty()) { // 完全匹配
                    if (word_set.size() != word_vec.size()) { // 文章中出现的词语数量与输入的不一致，则不满足完全匹配
                        continue;
                    }
                    else { // 在标题和正文中都不连续出现，则不满足
                        if (CheckWordContinus(word_vec, pos_map) == false && CheckWordContinus(word_vec, title_pos_map) == false) {
                            continue;
                        }
                    }
                }
                skipList.InsertNode(score, doc_id.c_str());
            }
        }
    } else {
        set<string>::iterator set_iter = valid_docs.begin();
        for(; set_iter != valid_docs.end(); set_iter++){
            doc_id = *set_iter;
            double score = 0;

            if (top_doc_score.find(doc_id) != top_doc_score.end()) {
                continue;
            } 

            if (m_sort_type == SORT_FIELD_ASC || m_sort_type == SORT_FIELD_DESC){
                //if(doc_manager->CheckDocByExtraFilterKey(doc_id) == false){
                //	continue;
                //}
                doc_manager->GetScoreMap(doc_id, m_sort_type, m_sort_field, m_sort_field_type, m_appid);
            } else {
                skipList.InsertNode(1, doc_id.c_str());
            }

            if (m_has_gis) {
                if (distances.find(doc_id) == distances.end())
                    continue;
                score = distances[doc_id];
                if ((component->Distance() > -0.0001 && component->Distance() < 0.0001) || (score <= component->Distance()))
                    skipList.InsertNode(score, doc_id.c_str());
            }
        }
    }
    
    // 范围查的时候如果不指定排序类型，需要在这里对skipList进行赋值
    if (!m_has_gis && ves.size() == 0 && skipList.GetSize() == 0 && m_sort_type == SORT_RELEVANCE) {
        set<string>::iterator iter = valid_docs.begin();
        for(; iter != valid_docs.end(); iter++){
            skipList.InsertNode(1, (*iter).c_str());
        }
    }
    return 0;
}

void SearchTask::AppendHighLightWord(Json::Value& response)
{
    int count = 0;
    set<string>::iterator iter = highlightWord.begin();
    for (; iter != highlightWord.end(); iter++) {
        if (count >= 10)
            break;
        count = count + 1;
        response["hlWord"].append((*iter).c_str());
    }
    return ;
}

int SearchTask::DoJob(CTaskRequest *request) {
    int ret = 0;

    // terminal_tag=1时单独处理
    if(component->TerminalTag() == 1){
        uint32_t count = 0;
        uint32_t N = 2;
        uint32_t limit_start = 0;
        vector<TerminalRes> candidate_doc;
        int try_times = 0;
        while(count < component->PageSize()){
            if(try_times++ > 10){
                log_debug("ProcessTerminal try_times is the max, return");
                break;
            }
            vector<TerminalRes> and_vecs;
            TerminalQryCond query_cond;
            query_cond.sort_type = m_sort_type;
            query_cond.sort_field = m_sort_field;
            query_cond.last_id = component->LastId();
            query_cond.last_score = component->LastScore();
            query_cond.limit_start = limit_start;
            query_cond.page_size = component->PageSize() * N;
            ret = logical_operate->ProcessTerminal(component->AndKeys(), query_cond, and_vecs);
            if(0 != ret){
                log_error("ProcessTerminal error.");
                return -RT_GET_DOC_ERR;
            }
            for(int i = 0; i < (int)and_vecs.size(); i++){
                string doc_id = and_vecs[i].doc_id;
                stringstream ss;
                ss << (int)and_vecs[i].score;
                string ss_key = ss.str();
                log_debug("last_score: %s, ss_key: %s, score: %lf", query_cond.last_score.c_str(), ss_key.c_str(), and_vecs[i].score);
                if(component->LastId() != "" && ss_key == query_cond.last_score){ // 翻页时过滤掉已经返回过的文档编号
                    if(m_sort_type == SORT_FIELD_DESC && doc_id >= component->LastId()){
                        continue;
                    }
                    if(m_sort_type == SORT_FIELD_ASC && doc_id <= component->LastId()){
                        continue;
                    }
                }
                if(doc_manager->CheckDocByExtraFilterKey(doc_id) == true){
                    count++;
                    candidate_doc.push_back(and_vecs[i]);
                }
            }
            limit_start += component->PageSize() * N;
            N *= 2;
        }
        Json::FastWriter writer;
        Json::Value response;
        response["code"] = 0;
        int sequence = -1;
        int rank = 0;
        for (uint32_t i = 0; i < candidate_doc.size(); i++) {
            if(rank >= (int)component->PageSize()){
                break;
            }
            sequence++;
            rank++;
            TerminalRes tmp = candidate_doc[i];
            Json::Value doc_info;
            doc_info["doc_id"] = Json::Value(tmp.doc_id.c_str());
            doc_info["score"] = Json::Value(tmp.score);
            response["result"].append(doc_info);
        }
        response["type"] = 0;
        response["count"] = rank;  // TODO 这里的count并不是实际的总数
        std::string outputConfig = writer.write(response);
        request->setResult(outputConfig);
        return 0;
    }

    map<string, double> top_doc_score;
    if (component->TopSwitch() == 1) {
        ret = GetTopDocScore(top_doc_score);
        if (ret != 0) {
            return -RT_GET_DOC_ERR;
        }
    }
    ret = GetDocScore(top_doc_score);
    if (ret != 0) {
        return -RT_GET_DOC_ERR;
    }
    Json::FastWriter writer;
    Json::Value response;
    response["code"] = 0;
    int sequence = -1;
    int rank = 0;
    int page_size = component->PageSize();
    int limit_start = page_size * (component->PageIndex()-1);
    int limit_end = page_size * (component->PageIndex()-1) + page_size - 1;

    log_debug("search result begin.");
    vector<PAIR> top_vec(top_doc_score.begin(), top_doc_score.end());
    sort(top_vec.begin(), top_vec.end(), CmpByValue());

    for (uint32_t i = 0; i < top_vec.size(); i++) {
        sequence++;
        rank++;
        if(component->ReturnAll() == 0){
            if (sequence < limit_start || sequence > limit_end) {
                continue;
            }
        }
        pair<string, double> tmp = top_vec[i];
        Json::Value doc_info;
        doc_info["doc_id"] = Json::Value(tmp.first.c_str());
        doc_info["score"] = Json::Value(tmp.second);
        response["result"].append(doc_info);
    }

    if((m_sort_type == SORT_FIELD_DESC || m_sort_type == SORT_FIELD_ASC) && skipList.GetSize() == 0){
        OrderOpCond order_op_cond;
        order_op_cond.last_id = component->LastId();
        order_op_cond.limit_start = limit_start;
        order_op_cond.count = page_size;
        order_op_cond.has_extra_filter = false;
        if(component->ExtraFilterKeys().size() != 0 || component->ExtraFilterAndKeys().size() != 0 || component->ExtraFilterInvertKeys().size() != 0){
            order_op_cond.has_extra_filter = true;
        }
        if(m_sort_field_type == FIELDTYPE_INT){
            rank += doc_manager->ScoreIntMap().size();
            COrderOp<int> orderOp(FIELDTYPE_INT, component->SearchAfter(), m_sort_type);
            orderOp.Process(doc_manager->ScoreIntMap(), atoi(component->LastScore().c_str()), order_op_cond, response, doc_manager);
        } else if(m_sort_field_type == FIELDTYPE_DOUBLE) {
            rank += doc_manager->ScoreDoubleMap().size();
            COrderOp<double> orderOp(FIELDTYPE_DOUBLE, component->SearchAfter(), m_sort_type);
            orderOp.Process(doc_manager->ScoreDoubleMap(), atof(component->LastScore().c_str()), order_op_cond, response, doc_manager);
        } else {
            rank += doc_manager->ScoreStrMap().size();
            COrderOp<string> orderOp(FIELDTYPE_STRING, component->SearchAfter(), m_sort_type);
            orderOp.Process(doc_manager->ScoreStrMap(), component->LastScore(), order_op_cond, response, doc_manager);
        }
    } else if (m_has_gis || m_sort_type == SORT_FIELD_ASC) {
        log_debug("m_has_gis, size:%d ", skipList.GetSize());
        SkipListNode *tmp = skipList.GetHeader()->level[0].forward;
        while (tmp->level[0].forward != NULL) {
            // 通过extra_filter_keys进行额外过滤（针对区分度不高的字段）
            if(doc_manager->CheckDocByExtraFilterKey(tmp->value) == false){
                log_debug("CheckDocByExtraFilterKey failed, %s", tmp->value);
                tmp = tmp->level[0].forward;
                continue;
            }
            sequence++;
            rank++;
            if(component->ReturnAll() == 0){
                if (sequence < limit_start || sequence > limit_end) {
                    tmp = tmp->level[0].forward;
                    continue;
                }
            }
            Json::Value doc_info;
            doc_info["doc_id"] = Json::Value(tmp->value);
            doc_info["score"] = Json::Value(tmp->key);
            response["result"].append(doc_info);
            tmp = tmp->level[0].forward;
        }
    } else {
        SkipListNode *tmp = skipList.GetFooter()->backward;
        while(tmp->backward != NULL) {
            if(doc_manager->CheckDocByExtraFilterKey(tmp->value) == false){
                tmp = tmp->backward;
                continue;
            }
            sequence++;
            rank++;
            if (component->ReturnAll() == 0){
                if (sequence < limit_start || sequence > limit_end) {
                    tmp = tmp->backward;
                    continue;
                }
            }
            Json::Value doc_info;
            doc_info["doc_id"] = Json::Value(tmp->value);
            doc_info["score"] = Json::Value(tmp->key);
            response["result"].append(doc_info);
            tmp = tmp->backward;
        }
    }

    if(m_fields.size() > 0){
        doc_manager->AppendFieldsToRes(response, m_fields);
    }

    if (rank > 0)
        AppendHighLightWord(response);
    if (m_has_gis) {
        response["type"] = 1;
    }
    else {
        response["type"] = 0;
    }
    response["count"] = rank;
    if(m_index_set_cnt != 0){
        response["count"] = m_index_set_cnt;
    }
    log_debug("search result end: %lld.", (long long int)GetSysTimeMicros());
    std::string outputConfig = writer.write(response);
    request->setResult(outputConfig);
    if (component->ReturnAll() == 0 && component->CacheSwitch() == 1 && component->PageIndex() == 1 && m_has_gis == 0 
        && rank > 0 && outputConfig.size() < MAX_VALUE_LEN && m_Primary_Data != "") {
        string m_Data_Cache = m_Primary_Data + "|" + component->DataAnd() + "|" + component->DataInvert() + "|" + component->DataComplete() + "|" +
                                ToString(m_sort_type) + "|" + ToString(m_appid);
        unsigned data_size = m_Data_Cache.size();
        int ret = cachelist->add_list(m_Data_Cache.c_str(), outputConfig.c_str(), data_size, outputConfig.size());
        if (ret != 0) {
            log_error("add to cache_list error, ret: %d.", ret);
        }
        else {
            log_debug("add to cache_list: %s.", m_Data_Cache.c_str());
        }
    }
    return 0;
}

int SearchTask::Process(CTaskRequest *request)
{
    log_debug("SearchTask::Process begin: %lld.", (long long int)GetSysTimeMicros());
    common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("searchEngine.searchService.searchTask"));
    Json::Value recv_packet;
    string request_string = request->buildRequsetString();
    if (component->ParseJson(request_string.c_str(), request_string.length(), recv_packet) != 0) {
        string str = GenReplyStr(PARAMETER_ERR);
        request->setResult(str);
        common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
        common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
        return -RT_PARSE_JSON_ERR;
    }
    
    m_Primary_Data = component->Data();
    m_appid = component->Appid();
    m_sort_type = component->SortType();
    m_sort_field = component->SortField();
    if(component->Fields().size() > 0){
        m_fields.assign(component->Fields().begin(), component->Fields().end());
    }

    skipList.InitList();
    component->InitSwitch();
    log_debug("m_Data: %s", m_Primary_Data.c_str());

    m_query_ = component->GetQuery();
    if(m_query_.isObject()){
        if(m_query_.isMember("match")){
            query_process_ = new MatchQueryProcess(m_appid, m_query_["match"], component);
        } else {
			log_error("query type error.");
			return -RT_PARSE_JSON_ERR;
		}
        query_process_->SetSkipList(skipList);
        query_process_->SetRequest(request);
        int ret = query_process_->DoJob();
		if(ret != 0){
			log_error("query_process_ DoJob error, ret: %d", ret);
			return ret;
		}
        return 0;
    }

    string err_msg = "";
    int ret = component->GetQueryWord(m_has_gis, err_msg);
    if (ret != 0) {
        string str = GenReplyStr(PARAMETER_ERR, err_msg);
        request->setResult(str);
        common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
        return ret;
    }
    if(component->TerminalTag() == 1 && component->TerminalTagValid() == false){
        log_error("TerminalTag is 1 and TerminalTagValid is false.");
        common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
        return -RT_PARSE_JSON_ERR;
    }
    doc_manager = new DocManager(component);

    log_debug("cache_switch: %u", component->CacheSwitch());
    if (component->ReturnAll() == 0 && component->CacheSwitch() == 1 && component->PageIndex() == 1 && m_Primary_Data != "" && m_has_gis == 0) {
        string m_Data_Cache = m_Primary_Data + "|" + component->DataAnd() + "|" + component->DataInvert() + "|" + component->DataComplete() + "|" +
                                ToString(m_sort_type) + "|" + ToString(m_appid);
        uint8_t value[MAX_VALUE_LEN] = { 0 };
        unsigned vsize = 0;
        if (cachelist->in_list(m_Data_Cache.c_str(), m_Data_Cache.size(), value, vsize))
        {
            statmgr.GetItemU32(INDEX_SEARCH_HIT_CACHE)++;
            log_debug("hit cache.");
            value[vsize] = '\0';
            std::string outputConfig = (char *)value;
            request->setResult(outputConfig);
            common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
            return 0;
        }
    }

    if (component->DataComplete() != "") {
        FieldInfo fieldInfo;
        string split_data = SplitManager::Instance()->split(component->DataComplete(), m_appid);
        log_debug("complete split_data: %s", split_data.c_str());
        vector<string> split_datas = splitEx(split_data, "|");
        for(size_t i = 0; i < split_datas.size(); i++) {
            fieldInfo.word = split_datas[i];
            complete_keys.push_back(fieldInfo);
        }
    }
    logical_operate = new LogicalOperate(m_appid, m_sort_type, m_has_gis, component->CacheSwitch());
    ret = DoJob(request);
    if (ret != 0) {
        string str = GenReplyStr(PARAMETER_ERR);
        request->setResult(str);
        common::ProfilerMonitor::GetInstance().FunctionError(caller_info);
        common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
        return ret;
    }
    common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
    return 0;
}


SearchTask::~SearchTask() {
    if(component != NULL){
        delete component;
    }
    if(logical_operate != NULL){
        delete logical_operate;
    }
    if(doc_manager != NULL){
        delete doc_manager;
    }
}
