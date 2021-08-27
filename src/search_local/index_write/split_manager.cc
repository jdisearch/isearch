/*
 * =====================================================================================
 *
 *       Filename:  split_manager.cc
 *
 *    Description:  SplitManager class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  shrewdlin, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "split_manager.h"
#include <string>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <math.h>
#include <sys/time.h>
#include "log.h"
#include "stem.h"
#include "comm.h"
#include "json/json.h"

using namespace std;

typedef pair<string, int> PAIR;
struct CmpByValue {
    bool operator()(const PAIR& lhs, const PAIR& rhs) {
        return lhs.second > rhs.second;
    }
};

SplitManager::SplitManager() {
    stop_word_set.clear();
}

SplitManager::~SplitManager() {
}

bool SplitManager::fetch_tbinfo_from_mysql_to_map(){
    std::string sPath = "../conf/app_field_define.json";
    std::ifstream iStream(sPath.c_str());
    if (!iStream.is_open()){
        log_error("load %s failed.", sPath.c_str());
        return false;
    }

    Json::Reader reader;
    Json::Value oTableDefine;
    if (reader.parse(iStream, oTableDefine)
        && oTableDefine.isMember("tableDefine"))
    {
        Json::Value oTableDefineCont = oTableDefine["tableDefine"];
        if (oTableDefineCont.isArray()){
            for (int i = 0; i < (int)oTableDefineCont.size(); ++i){
                uint32_t uiAppid = 0;
                if (oTableDefineCont[i].isMember("appId")
                    && oTableDefineCont[i]["appId"].isInt()){
                    uiAppid = oTableDefineCont[i]["appId"].asInt();
                }else{
                    log_error("appId is incorrect");
                    return false;
                }
                
                std::string sFieldName = "";
                if (oTableDefineCont[i].isMember("fieldName")
                    && oTableDefineCont[i]["fieldName"].isString()){
                    sFieldName = oTableDefineCont[i]["fieldName"].asString();
                }else{
                    log_error("fieldName is incorrect");
                    return false;
                }

                table_info oTabInfo;
                if (oTableDefineCont[i].isMember("fieldType")
                    && oTableDefineCont[i]["fieldType"].isInt()){
                    oTabInfo.field_type = oTableDefineCont[i]["fieldType"].asInt();
                }else{
                    log_error("fieldType is incorrect");
                    return false;
                }

                if (oTableDefineCont[i].isMember("fieldId")
                    && oTableDefineCont[i]["fieldId"].isInt()){
                    oTabInfo.field_id = oTableDefineCont[i]["fieldId"].asInt();
                }else{
                    log_error("fieldId is incorrect");
                    return false;
                }

                if (oTableDefineCont[i].isMember("IsPrimaryKey")
                    && oTableDefineCont[i]["IsPrimaryKey"].isInt()){
                    oTabInfo.is_primary_key = oTableDefineCont[i]["IsPrimaryKey"].asInt();
                }else{
                    log_error("IsPrimaryKey is incorrect");
                    return false;
                }

                if (oTableDefineCont[i].isMember("indexTag")
                    && oTableDefineCont[i]["indexTag"].isInt()){
                    oTabInfo.index_tag = oTableDefineCont[i]["indexTag"].asInt();
                }else{
                    log_error("indexTag is incorrect");
                    return false;
                }

                if (oTableDefineCont[i].isMember("snapshotTag")
                    && oTableDefineCont[i]["snapshotTag"].isInt()){
                    oTabInfo.snapshot_tag = oTableDefineCont[i]["snapshotTag"].asInt();
                }else{
                    log_error("snapshotTag is incorrect");
                    return false;
                }

                if (oTableDefineCont[i].isMember("segmentTag")
                    && oTableDefineCont[i]["segmentTag"].isInt()){
                    oTabInfo.segment_tag = oTableDefineCont[i]["segmentTag"].asInt();
                }else{
                    log_error("segmentTag is incorrect");
                    return false;
                }

                if (oTableDefineCont[i].isMember("segmentFeature")
                    && oTableDefineCont[i]["segmentFeature"].isInt()){
                    oTabInfo.segment_feature = oTableDefineCont[i]["segmentFeature"].asInt();
                }else{
                    log_error("segmentFeature is incorrect");
                    return false;
                }

                if (oTableDefineCont[i].isMember("unionField")
                    && oTableDefineCont[i]["unionField"].isString()){
                    // unionField的格式是：27,1,26，数字代表的是field对应的id值
                    oTabInfo.index_info = oTableDefineCont[i]["unionField"].asString();
                }

                tableDefine[uiAppid][sFieldName] = oTabInfo;
                log_debug("appid:%d , fieldName:%s , fieldType:%d , fieldId:%d, IsPrimaryKey:%d , indexTag:%d, snapshotTag:%d , segmentTag:%d, segmentFeature:%d , unionField:%s"
                    , uiAppid , sFieldName.c_str() , oTabInfo.field_type , oTabInfo.field_id 
                    , oTabInfo.is_primary_key , oTabInfo.index_tag , oTabInfo.snapshot_tag
                    , oTabInfo.segment_tag , oTabInfo.segment_feature , oTabInfo.index_info.c_str());
            }
            log_debug("tableDefine size: %d", (int)tableDefine.size());
        }else{
            log_error("tableDefine is incorrect");
            return false;
        }
    }else{
        log_error("config file: %s has error format" , sPath.c_str());
        iStream.close();
        return false;
    }
    return true;
}

bool SplitManager::Init(const SGlobalIndexConfig &global_cfg) {

    bool ret = seg.Init3(global_cfg.trainingPath,global_cfg.sWordsPath);
    if (ret == false) {
        log_error("seg init error.");
        return false;
    }

    ifstream inf;
    string s;
    string word;
    split_mode = global_cfg.sSplitMode;
    //load stop words
    inf.open(global_cfg.stopWordsPath.c_str());
    if (inf.is_open() == false) {
        printf("open file error: %s.\n", "./stop_words.dict");
        return false;
    }
    while (getline(inf, s)) {
        stop_word_set.insert(s);
    }
    inf.close();
    log_info("load %d words from stop_words.dict",(int)stop_word_set.size());

    string str;
    ifstream phonetic_infile;
    uint32_t phonetic_id = 0;
    uint32_t character_id = 0;
    string phonetic;
    string charact;
    phonetic_infile.open(global_cfg.sPhoneticPath.c_str());
    if (phonetic_infile.is_open() == false) {
        log_error("open file error: %s.", global_cfg.sPhoneticPath.c_str());
        return false;
    }

    while (getline(phonetic_infile, str))
    {
        vector<string> str_vec = splitEx(str, "\t");
        if (str_vec.size() == 2) {
            phonetic_id = atoi(str_vec[0].c_str());
            phonetic = str_vec[1];
            phonetic_map[phonetic] = phonetic_id;
        }
    }
    phonetic_infile.close();

    ifstream phonetic_base_infile;
    phonetic_base_infile.open(global_cfg.sPhoneticBasePath.c_str());
    if (phonetic_base_infile.is_open() == false) {
        log_error("open file error: %s.", global_cfg.sPhoneticBasePath.c_str());
        return false;
    }

    while (getline(phonetic_base_infile, str))
    {
        vector<string> str_vec = splitEx(str, "\t");
        if (str_vec.size() == 2) {
            charact = str_vec[0];
            phonetic = str_vec[1];
            charact_phonetic_map.insert(make_pair(charact, phonetic));
        }
    }
    phonetic_base_infile.close();

    ifstream character_infile;
    character_infile.open(global_cfg.sCharacterPath.c_str());
    if (character_infile.is_open() == false) {
        log_error("open file error: %s.", global_cfg.sCharacterPath.c_str());
        return false;
    }

    while (getline(character_infile, str))
    {
        vector<string> str_vec = splitEx(str, "\t");
        if (str_vec.size() == 2) {
            character_id = atoi(str_vec[0].c_str());
            charact = str_vec[1];
            charact_map[charact] = character_id;
        }
    }
    character_infile.close();
    log_info("load %d words from phonetic_map, %d words from charact_map", (int)phonetic_map.size(), (int)charact_map.size());

    return fetch_tbinfo_from_mysql_to_map();
}

bool SplitManager::wordValid(string word, uint32_t appid, uint32_t &id) {

    if(stop_word_set.find(word) != stop_word_set.end()){
        log_debug("word:%s invalid,in the stop.dict",word.c_str());
        return false;
    }
    uint64_t int_word;
    string output_word = word;
    WordInfo wordinfo;
    if((word[0]>='a' && word[0]<='z')||(word[0]>='A' && word[0]<='Z')){
        output_word = stem(word);
        seg.GetWordInfo(output_word, appid, wordinfo);//bug fixed ,English need to call dtc to search the wordinfo first
    }

    if(0 == wordinfo.word_id && !GetWordInfo(output_word,appid,wordinfo)){
        int_word = strtoull(word.c_str(),NULL,10);
        if(int_word != 0){
            while(int_word > MAXNUMBER){
                int_word = int_word/10;
            }
            id = NUMBER_ID + int_word;
            return true;
        }
        log_debug("word:%s invalid,not in the wordbase",output_word.c_str());
        return false;
    }else{
        id = wordinfo.word_id;
    }
    return true;
}

bool SplitManager::GetCharactId(string charact, uint32_t &id) {
    id = 0;
    if (charact_map.find(charact) != charact_map.end()) {
        id = charact_map[charact];
    }
    return true;
}

bool SplitManager::GetPhoneticId(string phonetic, uint32_t &id) {
    id = 0;
    if (phonetic_map.find(phonetic) != phonetic_map.end()) {
        id = phonetic_map[phonetic];
    }
    return true;
}

vector<string> SplitManager::GetPhonetic(string charact) {
    vector<string> vec;
    multimap<string, string>::iterator iter;
    iter = charact_phonetic_map.find(charact);
    int k = 0;
    for (; k < (int)charact_phonetic_map.count(charact); k++, iter++) {
        vec.push_back(iter->second);
    }

    return vec;
}

struct table_info *SplitManager::get_table_info(uint32_t appid, string field_name){
    if(tableDefine.find(appid) != tableDefine.end()){
        if(tableDefine[appid].find(field_name) != tableDefine[appid].end()){
            return &(tableDefine[appid][field_name]);
        }
    }
    return NULL;
}

bool SplitManager::getHanpinField(uint32_t appid, vector<uint32_t> & field_vec) {
    if (tableDefine.find(appid) != tableDefine.end()) {
        map<string, table_info> stMap = tableDefine[appid];
        map<string, table_info>::iterator iter = stMap.begin();
        for (; iter != stMap.end(); iter++) {
            table_info tInfo = iter->second;
            if (tInfo.segment_tag == 3 || tInfo.segment_tag == 4) {
                field_vec.push_back(tInfo.field_id);
            }
        }
        return true;
    }
    return false;
}

bool SplitManager::getUnionKeyField(uint32_t appid, vector<string> & field_vec){
    if (tableDefine.find(appid) != tableDefine.end()) {
        map<string, table_info> stMap = tableDefine[appid];
        map<string, table_info>::iterator iter = stMap.begin();
        for (; iter != stMap.end(); iter++) {
            table_info tInfo = iter->second;
            if(tInfo.field_type == FIELD_INDEX){
                field_vec.push_back(tInfo.index_info);
            }
        }
        return true;
    }
    return false;
}

bool SplitManager::is_effective_appid(uint32_t appid){
    if(tableDefine.find(appid) != tableDefine.end()){
        return true;
    }
    return true;
}

void SplitManager::DeInit() {
    stop_word_set.clear();
}

vector<vector<string> > SplitManager::split(string str,uint32_t appid) {
    iutf8string test(str);
    unsigned int t1;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    t1 = tv.tv_sec * 1000000 + tv.tv_usec;
    vector<vector<string> > res_all;
    seg.cut_for_search(test,appid,res_all,split_mode);
    unsigned int t2;
    gettimeofday(&tv, NULL);
    t2 = tv.tv_sec * 1000000 + tv.tv_usec;

    log_debug("split time:%u ms",(t2-t1)/1000);
    return res_all;
}

vector<string> SplitManager::split(string str) {
    vector<string> vec;
    iutf8string utf8_str(str);
    seg.cut_ngram(utf8_str, vec, utf8_str.length());
    return vec;
}

string trim(string& str)
{
    str.erase(0, str.find_first_not_of(" ")); // 去掉头部空格
    str.erase(str.find_last_not_of(" ") + 1); // 去掉尾部空格
    return str;
}

string delPrefix(string& str){
    size_t pos1 = str.find_first_of("((");
    size_t pos2 = str.find_last_of("))");
    string res = str;
    if(pos1 != string::npos && pos2 != string::npos){
        res = str.substr(pos1+2, pos2-pos1-3);
    }
    return res;
}
