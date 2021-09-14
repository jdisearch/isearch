/*
 * =====================================================================================
 *
 *       Filename:  index_tbl_op.h
 *
 *    Description:  index tbl op class definition.
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

#include "index_tbl_op.h"
#include "timemanager.h"
#include "comm.h"
#include <ctime>
#include <string>
#include <set>
#include <sstream>
using namespace std;
#define FOREACH(type,elem,arr)  for(type::const_iterator elem \
                                        =  arr.begin(); elem != arr.end(); ++elem )
CIndexTableManager g_IndexInstance;
CIndexTableManager g_hanpinIndexInstance;

bool CIndexTableManager::GetDocInfo(uint32_t appid, string word, uint32_t field_id, vector<IndexInfo> &doc_info) {
    int ret;
    DTC::Server* dtcServer = &server;

    if (NULL == dtcServer) {
        log_error("dtcServer is null !");
        return false;
    }
    
    DTC::GetRequest getReq(dtcServer);

    ret = getReq.SetKey(word.c_str() , word.length());
    if (field_id != 0 && field_id != INT_MAX)
        ret = getReq.EQ("field", field_id);
    ret = getReq.EQ("start_time", 0);
    ret = getReq.EQ("end_time", 0);
    ret = getReq.Need("doc_id");
    ret = getReq.Need("doc_version");
    ret = getReq.Need("field");
    ret = getReq.Need("word_freq");
    ret = getReq.Need("location");
    ret = getReq.Need("created_time");
    ret = getReq.Need("extend");

    DTC::Result rst;
    ret = getReq.Execute(rst);
    if (ret != 0) {
        if (ret == -110) {
            rst.Reset();
            ret = getReq.Execute(rst);
            if (ret != 0) {
                log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
                return false;
            }
        }
        else {
            log_error("get request[%s] error! errcode %d,errmsg %s, errfrom %s", word.c_str(), ret, rst.ErrorMessage(), rst.ErrorFrom());
            return false;
        }
    }
    int cnt = rst.NumRows();

    string doc_id;
    if (cnt <= 0) {
        log_debug("not find in index, key[%s]", word.c_str());
        doc_info.clear();
    }
    else {
        for (int i = 0; i < cnt; i++) {
            rst.FetchRow();
            IndexInfo info;
            info.appid = appid;
            info.doc_id = rst.StringValue("doc_id");
            info.doc_version = rst.IntValue("doc_version");
            info.word_freq = rst.IntValue("word_freq");
            info.field = rst.IntValue("field");
            info.pos = rst.StringValue("location");
            info.created_time = rst.IntValue("created_time");
            info.extend = rst.StringValue("extend");

            if(doc_info.size() == 0 || info.doc_id.compare(doc_info.back().doc_id) != 0){
                doc_info.push_back(info);
            } else if(info.doc_version > doc_info.back().doc_version){
                doc_info.back().doc_version = info.doc_version;
                doc_info.back().word_freq = info.word_freq;
                doc_info.back().field = info.field;
                doc_info.back().pos = info.pos;
                doc_info.back().created_time = info.created_time;
            }
        }

    }
    return true;
}

bool CIndexTableManager::GetTopDocInfo(uint32_t appid, string word, vector<TopDocInfo>& doc_info)
{
    log_debug("appid [%d], word[%s]", appid, word.c_str());
    int ret;
    DTC::Server* dtcServer = &server;

    if (NULL == dtcServer) {
        log_error("dtcServer is null !");
        return false;
    }

    stringstream ss_key;
    ss_key<<appid;
    ss_key<<"#01#";
    ss_key<<word;

    time_t tm = time(0);
    DTC::GetRequest getReq(dtcServer);

    ret = getReq.SetKey(ss_key.str().c_str());
    ret = getReq.LE("start_time", tm);
    ret = getReq.GE("end_time", tm);
    ret = getReq.Need("doc_id");
    ret = getReq.Need("doc_version");
    ret = getReq.Need("created_time");
    ret = getReq.Need("weight");

    DTC::Result rst;
    ret = getReq.Execute(rst);
    if (ret != 0) {
        if (ret == -110) {
            rst.Reset();
            ret = getReq.Execute(rst);
            if (ret != 0) {
                log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
                return false;
            }
        }
        else {
            log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
            return false;
        }
    }
    int cnt = rst.NumRows();
    string doc_id;
    if (rst.NumRows() <= 0) {
        log_debug("not find in index, key[%s]", word.c_str());
        doc_info.clear();
    }
    else {
        for (int i = 0; i < cnt; i++) {
            rst.FetchRow();
            TopDocInfo info;
            info.appid = appid;
            info.doc_id = rst.StringValue("doc_id");
            info.doc_version = rst.IntValue("doc_version");
            info.created_time = rst.IntValue("created_time");
            info.weight = rst.IntValue("weight");
            doc_info.push_back(info);
        }

    }
    return true;
}

bool CIndexTableManager::DocValid(uint32_t appid, string doc_id, bool &is_valid){
    int ret = 0;
    DTC::Server* dtc_server = &server;
    if (NULL == dtc_server) {
        log_error("dtc_server is null !");
        return false;
    }
    DTC::GetRequest getReq(dtc_server);
    stringstream ss_key;
    ss_key << appid;
    ss_key << "#10#";
    ss_key << doc_id;
    ret = getReq.SetKey(ss_key.str().c_str());
    ret |= getReq.Need("doc_id");
    ret |= getReq.Need("doc_version");

    if(0 != ret)
    {
        log_error("set need field failed. appid:%d, doc_id:%s", appid, doc_id.c_str());
        return false;
    }

    DTC::Result rst;
    ret = getReq.Execute(rst);
    if(0 != ret)
    {
        log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
        return false;
     }
    int cnt = rst.NumRows();
    if(cnt <= 0)
    {
        log_debug("can not find any result. appid:%d, doc_id:%s", appid, doc_id.c_str());
        is_valid = false;
    } else {
        is_valid = true;
    }
    return true;
}

bool CIndexTableManager::get_snapshot_execute(int left, 
                                              int right, 
                                              uint32_t appid, 
                                              const vector<IndexInfo>& no_filter_docs, 
                                              vector<DocVersionInfo>& docVersionInfo) 
{
    int ret = 0;
    if (left == right) {
        return true;
    }
    DTC::Server* dtc_server = &server;
    if (NULL == dtc_server) {
        log_error("dtc_server is null !");
        return false;
    }
    dtc_server->AddKey("key", DTC::KeyTypeString);
    DTC::GetRequest getReq(dtc_server);

    string snapKeys;
    for (int i = left ; i < right; i++) 
    {
        stringstream ss_key;
        ss_key << appid;
        ss_key << "#10#";
        ss_key << no_filter_docs[i].doc_id;
        snapKeys += ss_key.str();
        ret = getReq.AddKeyValue("key", ss_key.str().c_str());
    }

    if(0 != ret)
    {
        log_error("AddKeyValue failed");
        return false;
    }
    ret |= getReq.Need("doc_id");
    ret |= getReq.Need("doc_version");
    ret |= getReq.Need("extend");

    if(0 != ret)
    {
        log_error("set need field failed. appid:%d, doc_id:%s", appid, snapKeys.c_str());
        return false;
    }
    DTC::Result rst;
    ret = getReq.Execute(rst);
    if(ret != 0)
    {
        log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
        return false;
     }
    int cnt = rst.NumRows();
    if(cnt <= 0)
    {
        log_debug("can not find any result. appid:%d, doc_id:%s", appid, snapKeys.c_str());
        return true;
    }

    for(int i = 0; i < cnt; ++i)
    {
        DocVersionInfo info;
        rst.FetchRow();
        info.doc_id = rst.StringValue("doc_id");
        info.doc_version = rst.IntValue("doc_version");
        info.content = rst.StringValue("extend");
        docVersionInfo.push_back(info);
    }

    return true;
}

bool CIndexTableManager::DocValid(uint32_t appid, const vector<IndexInfo>& vecs, bool need_version, map<string, uint32_t>& valid_version, hash_string_map& doc_content_map){
    int numbers = 32;
    int docSize = vecs.size();
    int count = docSize / numbers;
    int remain = docSize % numbers;
    vector<DocVersionInfo> docVersionInfo;

    for (int index = 0; index < count; index++) 
    {
        int left = index * numbers;
        int right = (index + 1) * numbers;
        if (!get_snapshot_execute(left, right, appid, vecs, docVersionInfo))
            return false;
    }

    if (!get_snapshot_execute(docSize-remain, docSize, appid, vecs, docVersionInfo)) {
        return false;
    }

    for (size_t i = 0; i < vecs.size(); i++)
    {
        string doc_id = vecs[i].doc_id;
        uint32_t doc_version = vecs[i].doc_version;
        for (size_t j = 0; j < docVersionInfo.size(); j++)
        {
            if ((doc_id == docVersionInfo[j].doc_id) && (doc_version == docVersionInfo[j].doc_version)){
                ResultContext::Instance()->SetValidDocs(doc_id);
                doc_content_map.insert(make_pair(doc_id, docVersionInfo[j].content));
                if(need_version){
                    valid_version.insert(make_pair(doc_id, doc_version));
                }
                break;
            }
        }
    }
    return true;
}

bool CIndexTableManager::get_top_snapshot_execute(int left, 
                                                   int right, 
                                                  uint32_t appid, 
                                                  vector<TopDocInfo>& no_filter_docs, 
                                                  vector<DocVersionInfo>& docVersionInfo) 
{
    int ret = 0;
    if (left == right) {
        return true;
    }
    DTC::Server* dtc_server = &server;
    if (NULL == dtc_server) {
        log_error("dtc_server is null !");
        return false;
    }
    dtc_server->AddKey("key", DTC::KeyTypeString);
    DTC::GetRequest getReq(dtc_server);

    string snapKeys;
    for (int i = left ; i < right; i++) 
    {
        stringstream ss_key;
        ss_key << appid;
        ss_key << "#11#";
        ss_key << no_filter_docs[i].doc_id;
        snapKeys += ss_key.str();
        ret = getReq.AddKeyValue("key", ss_key.str().c_str());
    }

    if(0 != ret)
    {
        log_error("AddKeyValue failed");
        return false;
    }
    ret |= getReq.Need("doc_id");
    ret |= getReq.Need("doc_version");

    if(0 != ret)
    {
        log_error("set need field failed. appid:%d, key:%s", appid, snapKeys.c_str());
        return false;
    }
    DTC::Result rst;
    ret = getReq.Execute(rst);
    if(ret != 0)
    {
        log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
        return false;
     }
    int cnt = rst.NumRows();
    if(cnt <= 0)
    {
        log_debug("can not find any result. appid:%d, key:%s", appid, snapKeys.c_str());
        return true;
    }

    for(int i = 0; i < cnt; ++i)
    {
        DocVersionInfo info;
        rst.FetchRow();
        info.doc_id = rst.StringValue("doc_id");
        info.doc_version = rst.IntValue("doc_version");
        docVersionInfo.push_back(info);
    }

    return true;
}

bool CIndexTableManager::TopDocValid(uint32_t appid, vector<TopDocInfo>& no_filter_docs, vector<TopDocInfo>& doc_info)
{
    int numbers = 32; //DTC批量查找的上限为32个
    int docSize = no_filter_docs.size();
    int count = docSize / numbers;
    int remain = docSize % numbers;
    vector<DocVersionInfo> docVersionInfo;

    for (int index = 0; index < count; index++) 
    {
        int left = index * numbers;
        int right = (index + 1) * numbers;
        if (!get_top_snapshot_execute(left, right, appid, no_filter_docs, docVersionInfo))
            return false;
    }

    if (!get_top_snapshot_execute(docSize-remain, docSize, appid, no_filter_docs, docVersionInfo)) {
        return false;
    }

    for (size_t i = 0; i < no_filter_docs.size(); i++)
    {
        TopDocInfo info = no_filter_docs[i];
        for (size_t j = 0; j < docVersionInfo.size(); j++)
        {
            if ((info.doc_id == docVersionInfo[j].doc_id) && (info.doc_version == docVersionInfo[j].doc_version)) {
                doc_info.push_back(info);
                break;
            }
        }
    }

    return true;
}

bool CIndexTableManager::GetDocContent(
    uint32_t appid, 
    const std::vector<IndexInfo>& index_infos,
    hash_string_map& doc_content)
{
    int numbers = 32; //DTC批量查找的上限为32个
    int doc_size = index_infos.size();
    int count = doc_size / numbers;
    int remain = doc_size % numbers;

    for (int index = 0; index < count; index++) 
    {
        int left = index * numbers;
        int right = (index + 1) * numbers;
        if (!GetSnapshotContent(left, right, appid, index_infos ,doc_content))
            return false;
    }

    if (!GetSnapshotContent(doc_size-remain, doc_size, appid, index_infos ,doc_content)) {
        return false;
    }

    return true;
}

bool CIndexTableManager::GetSnapshotContent(
    int left,
    int right,
    uint32_t appid,
    const std::vector<IndexInfo>& index_infos,
    hash_string_map& doc_content)
{
    int ret = 0;
    if (left == right) {
        return true;
    }
    DTC::Server* dtc_server = &server;
    if (NULL == dtc_server) {
        log_error("dtc_server is null !");
        return false;
    }
    dtc_server->AddKey("key", DTC::KeyTypeString);
    DTC::GetRequest getReq(dtc_server);

    string docKeys;
    for (int i = left ; i < right; i++) 
    {
        stringstream ss_key;
        ss_key << appid;
        ss_key << "#10#";
        ss_key << index_infos[i].doc_id;
        docKeys += ss_key.str();
        ret = getReq.AddKeyValue("key", ss_key.str().c_str());
    }

    if(0 != ret)
    {
        log_error("AddKeyValue failed");
        return false;
    }
    ret |= getReq.Need("doc_id");
    ret |= getReq.Need("extend");

    if(0 != ret)
    {
        log_error("set need field failed. appid:%d, key:%s", appid, docKeys.c_str());
        return false;
    }
    DTC::Result rst;
    ret = getReq.Execute(rst);
    if(ret != 0)
    {
        log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
        return false;
     }
    int cnt = rst.NumRows();
    if(cnt <= 0)
    {
        log_debug("can not find any result. appid:%d, key:%s", appid, docKeys.c_str());
        return true;
    }

    for(int i = 0; i < cnt; ++i)
    {
        rst.FetchRow();
        string doc_id = rst.StringValue("doc_id");
        string content = rst.StringValue("extend");
        doc_content[doc_id] = content;
    }

    return true;
}

int CIndexTableManager::InitServer(const SDTCHost &dtchost, string bindAddr) {
    string _MasterAddress = bindAddr;
    log_info("master address is  [%s]", _MasterAddress.c_str());

    server.StringKey();
    server.SetTableName(dtchost.szTablename.c_str());
    server.SetAddress(_MasterAddress.c_str());
    server.SetMTimeout(300);

    int ret;
    if ((ret = server.Ping()) != 0 && ret != -DTC::EC_TABLE_MISMATCH) {
        log_error("ping slave[%s] failed, err: %d", _MasterAddress.c_str(), ret);
        return -1;
    }

    return ret;
}

int CIndexTableManager::InitServer2(const SDTCHost &dtchost) {
    string _MasterAddress = "127.0.0.1";
    stringstream ss;
    uint32_t port = 0;
    if (dtchost.vecRoute.size() > 0) {
            SDTCroute route = dtchost.vecRoute[0];
            port = route.uPort;
            _MasterAddress = route.szIpadrr;
    }
    ss << ":" << port << "/tcp";
    string master_bind_port = ss.str();
    _MasterAddress.append(master_bind_port);
    
    log_info("master address is  [%s]", _MasterAddress.c_str());

    server.StringKey();
    server.SetTableName(dtchost.szTablename.c_str());
    server.SetAddress(_MasterAddress.c_str());
    server.SetMTimeout(300);

    int ret;
    if ((ret = server.Ping()) != 0 && ret != -DTC::EC_TABLE_MISMATCH) {
        log_error("ping slave[%s] failed, err: %d", _MasterAddress.c_str(), ret);
        return -1;
    }

    return ret;
}


int CIndexTableManager::GetDocCnt(uint32_t appid) {
    int doc_cnt = 10000;
    int ret;
    DTC::Server* dtcServer = &server;

    if (NULL == dtcServer) {
        log_error("dtcServer is null !");
        return doc_cnt;
    }

    DTC::GetRequest getReq(dtcServer);

    long long search_id = appid;
    search_id <<= 32;
    search_id += 0x7FFFFFFF;
    ret = getReq.SetKey(search_id);
    ret = getReq.Need("extend");

    DTC::Result rst;
    ret = getReq.Execute(rst);
    if (ret != 0) {
        if (ret == -110) {
            ret = getReq.Execute(rst);
            if (ret != 0) {
                log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
                return doc_cnt;
            }
        }
        else {
            log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
            return doc_cnt;
        }
    }
    if (rst.NumRows() > 0) {
        rst.FetchRow();
        doc_cnt = atoi(rst.StringValue("extend"));
    }
    log_debug("doc count: %d.", doc_cnt);

    return doc_cnt;
}

bool CIndexTableManager::GetSuggestDoc(uint32_t appid, int index, uint32_t len,  uint32_t field, const IntelligentInfo &info, vector<IndexInfo> &doc_id_set)
{
    int ret;
    DTC::Server* dtcServer = &server;

    if (NULL == dtcServer) {
        log_error("dtcServer is null !");
        return false;
    }

    string initial;
    stringstream condition;
    stringstream ss_key;
    ss_key << appid;
    ss_key << "#";
    ss_key << field;

    DTC::GetRequest getReq(dtcServer);
    ret = getReq.SetKey(ss_key.str().c_str());
    ret = getReq.Need("doc_id");
    ret = getReq.Need("word");
    ret = getReq.Need("doc_version");

    for(int i = 0; i < 8; i++){
        if (info.charact_id[i] != 0) {
            stringstream ss_charact;
            ss_charact << "charact_id_0" << i+1;
            condition << ss_charact.str() << info.charact_id[i] << " ";
            ret = getReq.EQ(ss_charact.str().c_str(), info.charact_id[i]);
        }
    }

    for(int i = 0; i < 8; i++){
        if (info.phonetic_id[i] != 0) {
            stringstream ss_phonetic;
            ss_phonetic << "phonetic_id_0" << i+1;
            condition << ss_phonetic.str() << info.phonetic_id[i] << " ";
            ret = getReq.EQ(ss_phonetic.str().c_str(), info.phonetic_id[i]);
        }
    }

    for(int i = 0; i < 8; i++){
        if (info.initial_char[i] != "") {
            stringstream ss_initial;
            ss_initial << "initial_char_0" << i+1;
            condition << ss_initial.str() << info.initial_char[i] << " ";
            ret = getReq.EQ(ss_initial.str().c_str(), info.initial_char[i].c_str());
        }
    }

    DTC::Result rst;
    ret = getReq.Execute(rst);
    if (ret != 0) {
        if (ret == -110) {
            rst.Reset();
            ret = getReq.Execute(rst);
            if (ret != 0) {
                log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
                return false;
            }
        }
        else {
            log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
            return false;
        }
    }

    int cnt = rst.NumRows();
    if (rst.NumRows() <= 0) {
        log_debug("not find in index, key[%s],condition[%s]", ss_key.str().c_str(), condition.str().c_str());
    }
    else {
        for (int i = 0; i < cnt; i++) {
            rst.FetchRow();
            IndexInfo info;
            info.doc_id = rst.StringValue("doc_id");
            info.doc_version = rst.IntValue("doc_version");
            doc_id_set.push_back(info);
            string word = rst.StringValue("word");
            if (isAllChinese(word)) {
                ResultContext::Instance()->SetHighLightWordSet(word.substr(index*3, len*3));
            } else {
                ResultContext::Instance()->SetHighLightWordSet(word.substr(index, len));
            }
        }

    }

    return true;
}

bool CIndexTableManager::GetSuggestDocWithoutCharacter(uint32_t appid, int index, uint32_t len,  uint32_t field, const IntelligentInfo &info, vector<IndexInfo> &doc_id_set)
{
    int ret;
    DTC::Server* dtcServer = &server;

    if (NULL == dtcServer) {
        log_error("dtcServer is null !");
        return false;
    }

    stringstream ss_key;
    ss_key << appid;
    ss_key << "#";
    ss_key << field;

    string initial;
    stringstream condition;
    DTC::GetRequest getReq(dtcServer);
    ret = getReq.SetKey(ss_key.str().c_str());
    ret = getReq.Need("doc_id");
    ret = getReq.Need("word");
    ret = getReq.Need("doc_version");

    for(int i = 0; i < 16; i++){
        if (info.initial_char[i] != "") {
            stringstream ss_initial;
            if(i + 1 < 10){
                ss_initial << "initial_char_0" << i+1;
            }else {
                ss_initial << "initial_char_" << i+1;
            }
            condition << ss_initial.str() << info.initial_char[i] << " ";
            ret = getReq.EQ(ss_initial.str().c_str(), info.initial_char[i].c_str());
        }
    }

    DTC::Result rst;
    ret = getReq.Execute(rst);
    if (ret != 0) {
        if (ret == -110) {
            rst.Reset();
            ret = getReq.Execute(rst);
            if (ret != 0) {
                log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
                return false;
            }
        }
        else {
            log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
            return false;
        }
    }

    int cnt = rst.NumRows();
    if (rst.NumRows() <= 0) {
        log_debug("not find in index, key[%s], condition[%s]", ss_key.str().c_str(), condition.str().c_str());
    }
    else {
        for (int i = 0; i < cnt; i++) {
            rst.FetchRow();
            IndexInfo info;
            info.doc_id = rst.StringValue("doc_id");
            info.doc_version = rst.IntValue("doc_version");
            doc_id_set.push_back(info);
            string word = rst.StringValue("word");
            if (isAllChinese(word)) {
                ResultContext::Instance()->SetHighLightWordSet(word.substr(index*3, len*3));
            } else {
                ResultContext::Instance()->SetHighLightWordSet(word.substr(index, len));
            }
        }

    }

    return true;
}

bool CIndexTableManager::GetScoreByField(uint32_t appid, string doc_id, string sort_field, uint32_t sort_type, ScoreInfo &score_info){
    int ret = 0;
    DTC::Server* dtcServer = &server;

    if (NULL == dtcServer) {
        log_error("dtcServer is null !");
        return false;
    }

    stringstream ss_key;
    ss_key << appid;
    ss_key << "#10#";
    ss_key << doc_id;

    DTC::GetRequest getReq(dtcServer);
    ret = getReq.SetKey(ss_key.str().c_str());
    ret = getReq.Need("extend");

    DTC::Result rst;
    ret = getReq.Execute(rst);
    if (ret != 0) {
        if (ret == -110) {
            rst.Reset();
            ret = getReq.Execute(rst);
            if (ret != 0) {
                log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
                return false;
            }
        }
        else {
            log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
            return false;
        }
    }

    if (rst.NumRows() <= 0) {
        log_debug("not find in index, key[%s]", ss_key.str().c_str());
    }
    else {
        rst.FetchRow();
        string extend = rst.StringValue("extend");
        Json::Reader r(Json::Features::strictMode());
        Json::Value recv_packet;
        int ret2 = 0;
        ret2 = r.parse(extend.c_str(), extend.c_str() + extend.length(), recv_packet);
        if (0 == ret2)
        {
            log_error("the err json is %s", extend.c_str());
            log_error("parse json error , errmsg : %s", r.getFormattedErrorMessages().c_str());
            return false;
        }

        if (recv_packet.isMember(sort_field.c_str()))
        {
            if(recv_packet[sort_field.c_str()].isUInt()){
                score_info.type = FIELDTYPE_INT;
                score_info.i = recv_packet[sort_field.c_str()].asUInt();
                score_info.score = recv_packet[sort_field.c_str()].asUInt();
            } else if(recv_packet[sort_field.c_str()].isString()){
                score_info.type = FIELDTYPE_STRING;
                score_info.str = recv_packet[sort_field.c_str()].asString();
                score_info.score = atoi(recv_packet[sort_field.c_str()].asString().c_str());
            } else if(recv_packet[sort_field.c_str()].isDouble()){
                score_info.type = FIELDTYPE_DOUBLE;
                score_info.d = recv_packet[sort_field.c_str()].asDouble();
                score_info.score = recv_packet[sort_field.c_str()].asDouble();
            } else {
                log_error("sort_field[%s] data type error.", sort_field.c_str());
                return false;
            }
        } else {
            log_error("appid[%u] sort_field[%s] invalid.", appid, sort_field.c_str());
            return false;
        }

    }

    return true;
}

bool CIndexTableManager::GetContentByField(uint32_t appid, string doc_id, uint32_t doc_version, const vector<string>& fields, Json::Value &value){
    int ret = 0;
    DTC::Server* dtcServer = &server;

    if (NULL == dtcServer) {
        log_error("dtcServer is null !");
        return false;
    }

    stringstream ss_key;
    ss_key << appid;
    ss_key << "#10#";
    ss_key << doc_id;

    DTC::GetRequest getReq(dtcServer);
    ret = getReq.SetKey(ss_key.str().c_str());
    if(doc_version != 0){
        getReq.EQ("doc_version", doc_version);
    }
    ret = getReq.Need("extend");

    DTC::Result rst;
    ret = getReq.Execute(rst);
    if (ret != 0) {
        if (ret == -110) {
            rst.Reset();
            ret = getReq.Execute(rst);
            if (ret != 0) {
                log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
                return false;
            }
        }
        else {
            log_error("get request error! errcode %d,errmsg %s, errfrom %s", ret, rst.ErrorMessage(), rst.ErrorFrom());
            return false;
        }
    }

    if (rst.NumRows() <= 0) {
        log_debug("not find in index, key[%s]", ss_key.str().c_str());
    }
    else {
        rst.FetchRow();
        string extend = rst.StringValue("extend");
        Json::Reader r(Json::Features::strictMode());
        Json::Value recv_packet;
        int ret2 = 0;
        ret2 = r.parse(extend.c_str(), extend.c_str() + extend.length(), recv_packet);
        if (0 == ret2)
        {
            log_error("parse json error [%s], errmsg : %s", extend.c_str(), r.getFormattedErrorMessages().c_str());
            return false;
        }

        for(int i = 0; i < (int)fields.size(); i++){
            if (recv_packet.isMember(fields[i].c_str()))
            {
                if(recv_packet[fields[i].c_str()].isUInt()){
                    value[fields[i].c_str()] = recv_packet[fields[i].c_str()].asUInt();
                } else if(recv_packet[fields[i].c_str()].isString()){
                    value[fields[i].c_str()] = recv_packet[fields[i].c_str()].asString();
                } else if(recv_packet[fields[i].c_str()].isDouble()){
                    value[fields[i].c_str()] = recv_packet[fields[i].c_str()].asDouble();
                } else {
                    log_error("field[%s] data type error.", fields[i].c_str());
                }
            } else {
                log_info("appid[%u] field[%s] invalid.", appid, fields[i].c_str());
            }
        }
    }
    return true;
}

vector<IndexInfo> vec_intersection(vector<IndexInfo> &a, vector<IndexInfo> &b) {
    vector<IndexInfo> intersection;
    vector<IndexInfo>::iterator ai = a.begin();
    vector<IndexInfo>::iterator bi = b.begin();
    while(ai != a.end() && bi != b.end()) {
        if(*bi < *ai) {
            bi ++;
        } else if(*ai < *bi) {
            ai ++;
        } else {
            // 求交集时优先选择extend字段包含快照的那个
            if((*ai).extend != ""){
                intersection.push_back(*ai);
            } else {
                intersection.push_back(*bi);
            }
            ai ++;
            bi ++;
        }
    }
    return intersection;
}

vector<IndexInfo> vec_union(vector<IndexInfo> &a, vector<IndexInfo> &b){
    vector<IndexInfo> union_vec;
    vector<IndexInfo>::iterator ai = a.begin();
    vector<IndexInfo>::iterator bi = b.begin();
    while(ai != a.end() && bi != b.end()) {
        if(*bi < *ai) {
            union_vec.push_back(*bi);
            bi ++;
        } else if(*ai < *bi) {
            union_vec.push_back(*ai);
            ai ++;
        } else {
            union_vec.push_back(*ai);
            ai ++;
            bi ++;
        }
    }
    while(ai != a.end()){
        union_vec.push_back(*ai);
        ai ++;
    }
    while(bi != b.end()){
        union_vec.push_back(*bi);
        bi ++;
    }
    return union_vec;
}

vector<IndexInfo> vec_difference(vector<IndexInfo> &a, vector<IndexInfo> &b){
    vector<IndexInfo> diff_vec;
    vector<IndexInfo>::iterator ai = a.begin();
    vector<IndexInfo>::iterator bi = b.begin();
    while(ai != a.end() && bi != b.end()) {
        if(*bi < *ai) {
            bi ++;
        } else if(*ai < *bi) {
            diff_vec.push_back(*ai);
            ai ++;
        } else {
            ai ++;
            bi ++;
        }
    }
    while(ai != a.end()){
        diff_vec.push_back(*ai);
        ai ++;
    }
    return diff_vec;
}

int ShiftIntelligentInfo(IntelligentInfo &info, int len)
{
    uint16_t *p = &info.charact_id[0];

    if (len >= 8) {
        return -1;
    }

    if (len < 1) {
        return -2;
    }

    memmove(&p[len], &p[0], sizeof(uint16_t) * (8 - len));

    for (int i = 0; i < len; i++) {
        p[i] = 0;
    }

    p = &info.phonetic_id[0];

    memmove(&p[len], &p[0], sizeof(uint16_t) * (8 - len));

    for (int i = 0; i < len; i++) {
        p[i] = 0;
    }

    GetInitialVec(info, len);

    return 0;
}

bool GetSuggestDoc(FieldInfo& fieldInfo, uint32_t len, const IntelligentInfo &info, vector<IndexInfo> &doc_id_set, uint32_t appid)
{
    bool bRet;
    int index = 0;
    uint32_t field = fieldInfo.field;
    uint32_t segment_feature = fieldInfo.segment_feature;
    bRet = g_hanpinIndexInstance.GetSuggestDoc(appid, index, len, field, info, doc_id_set);
    if (bRet == false)
        goto resError;

    if (segment_feature == 0) {
        return true;
    }

    for (uint32_t i = 1; i <= 8 - len; i++) {
        index = index + 1;
        IntelligentInfo t2 = info;
        if (ShiftIntelligentInfo(t2, i) < 0) {
            continue;
        }
        bRet = g_hanpinIndexInstance.GetSuggestDoc(appid, index, len, field, t2, doc_id_set);
        if (bRet == false)
            goto resError;
    }
    return true;

resError:
    log_error("GetSuggestDoc invalid.");
    return false;
}

bool GetSuggestDocWithoutCharacter(FieldInfo& fieldInfo, uint32_t len, const IntelligentInfo &info, vector<IndexInfo> &doc_id_set, uint32_t appid)
{
    bool bRet;
    int index = 0;
    uint32_t field = fieldInfo.field;
    uint32_t segment_feature = fieldInfo.segment_feature;
    bRet = g_hanpinIndexInstance.GetSuggestDocWithoutCharacter(appid, index, len, field, info, doc_id_set);
    if (bRet == false)
        goto resError;

    if (segment_feature == 0) {
        return true;
    }

    for (uint32_t i = 1; i <= 16 - len; i++) {
        index = index + 1;
        IntelligentInfo t2 = info;
        if (ShiftIntelligentInfoWithoutCharacter(t2, i) < 0) {
            continue;
        }
        bRet = g_hanpinIndexInstance.GetSuggestDocWithoutCharacter(appid, index, len, field, t2, doc_id_set);
        if (bRet == false)
            goto resError;
    }
    return true;

resError:
    log_error("GetSuggestDoc invalid.");
    return false;
}

int GetDocByShiftWord(FieldInfo fieldInfo, vector<IndexInfo> &doc_id_set, uint32_t appid)
{
    log_debug("GetDocByShiftWord start");
    bool bRet = true;
    set<vector<Content> > result;
    GetMultipleWords(fieldInfo.word, result);

    int len = result.size();
    if (len <= 0) {
        log_error("get shift word error.");
        return -RT_GET_SUGGEST_ERR;
    }

    set<vector<Content> >::iterator iter;
    for (iter = result.begin(); iter != result.end(); iter++) 
    {
        IntelligentInfo info;
        vector<Content> tmp = *iter;
        ConvertIntelligent(tmp, info, bRet);
        int length = tmp.size();
        if (bRet) {
            bRet = GetSuggestDoc(fieldInfo, length, info, doc_id_set, appid);
            if (!bRet) {
                log_error("GetSuggestDocInfo error.");
                return -RT_DTC_ERR;
            }
        }
    }

    return 0;
}

int GetDocByShiftEnWord(FieldInfo fieldInfo, vector<IndexInfo> &doc_id_set, uint32_t appid)
{
    log_debug("GetDocByShiftEnWord start");
    bool bRet = true;

    int length = 0;
    IntelligentInfo enInfo;
    ConvertCharIntelligent(fieldInfo.word, enInfo, length);
    bRet = GetSuggestDocWithoutCharacter(fieldInfo, length, enInfo, doc_id_set, appid);
    if (!bRet) {
        log_error("GetEnSuggestDocInfo error.");
        return -RT_DTC_ERR;
    }
    return 0;
}
