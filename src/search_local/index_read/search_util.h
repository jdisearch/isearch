/*
 * =====================================================================================
 *
 *       Filename:  search_util.h
 *
 *    Description:  search util class definition.
 *
 *        Version:  1.0
 *        Created:  22/03/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __SEARCH_UTIL_H__
#define __SEARCH_UTIL_H__
#include <string>
#include <vector>
#include <stdint.h>
#include <map>
#include <set>
#include <time.h>
#include "utf8_str.h"
#include "comm.h"
#include "search_conf.h"
#include "index_tbl_op.h"
#include "result_context.h"
using namespace std;
struct GeoPointContext;

vector<int> splitInt(const string& src, string separate_character);
set<string> splitStr(const string& src, string separate_character);
bool CheckWordContinus(const vector<string> &word_vec, map<string, vector<int> > &pos_map);
void split_func(string pinyin, string &split_str, int type = 1);
set<uint32_t> sets_intersection(set<uint32_t> v1, set<uint32_t> v2);
vector<Content> GetSingleWord(string m_Data);
int GetSuggestWord(string m_Data, vector<string> &word_vec, uint32_t suggest_cnt);
int GetEnSuggestWord(string m_Data, vector<string> &word_vec, uint32_t suggest_cnt);
bool isAllAlpha(string str);
bool isAllNumber(string str);
bool isAllChinese(string str);
bool isContainCharacter(string str);
int judgeDataType(string str);
int Convert2Phonetic(string str, string &phonetic);
string readFileIntoString(char * filename);
int JudgeWord(uint32_t appid,string str, bool &is_correct, string &probably_key);
int GetMultipleWords(string m_Data, set<vector<Content> >& cvset);
int GetInitialVec(IntelligentInfo &info, int len);
int ShiftIntelligentInfoWithoutCharacter(IntelligentInfo &info, int len);
const char *GetFormatTimeStr(uint32_t ulTime);
string ToString(uint32_t appid);
void ToLower(string &str);
string CharToString(char c);
set<string> sets_intersection(set<string> v1, set<string> v2); // 集合求交集
set<string> sets_union(set<string> v1, set<string> v2); // 集合求并集
set<string> sets_difference(set<string> v1, set<string> v2); // 集合求差集
double strToDouble(const string& str);
bool GetGisDistance(uint32_t appid, const GeoPointContext& geo_point, const hash_string_map& doc_content , hash_double_map& distances);
void ConvertCharIntelligent(const string word, IntelligentInfo &info, int &len);
void ConvertIntelligent(const vector<Content> &result, IntelligentInfo &info, bool &flag);
bool GetGisCode(string lng, string lat, string ip, double distance, vector<string>& gisCode);
bool GetGisCode(const vector<string>& lng_arr, const vector<string>& lat_arr, vector<string>& gisCode);
uint32_t GetIpNum(string ip);
int ShiftIntelligentInfo(IntelligentInfo &info, int len);
bool GetSuggestDoc(FieldInfo& fieldInfo, uint32_t len, const IntelligentInfo &info, vector<IndexInfo> &doc_id_set, uint32_t appid);
bool GetSuggestDocWithoutCharacter(FieldInfo& fieldInfo, uint32_t len, const IntelligentInfo &info, vector<IndexInfo> &doc_id_set);
int GetDocByShiftWord(FieldInfo fieldInfo, vector<IndexInfo> &doc_id_set, uint32_t appid);
int GetDocByShiftEnWord(FieldInfo fieldInfo, vector<IndexInfo> &doc_id_set, uint32_t appid);
uint64_t GetSysTimeMicros();
string trim(string& str);
string delPrefix(string& str);
#endif
