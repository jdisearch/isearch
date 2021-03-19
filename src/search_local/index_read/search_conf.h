/*
 * =====================================================================================
 *
 *       Filename:  search_conf.h
 *
 *    Description:  search conf class definition.
 *
 *        Version:  1.0
 *        Created:  16/03/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef __SEARCH_CONF_H__
#define __SEARCH_CONF_H__

#include <vector>
#include <map>
#include <string>
#include "singleton.h"
#include "json/json.h"
#include "split_tool.h"
#include <stdint.h>
using namespace std;

typedef struct
{
	string broker;
	string clientid;
	string topic;
} ClickInfo;

typedef struct
{
	string broker;
	string clientid;
	string topic;
} QueryInfo;

struct AppInfo
{
	AppInfo() {
		cache_switch = 0;
		en_query_switch = 0;
		snapshot_switch = 0;
		top_switch = 0;
	}
	uint32_t cache_switch;
	uint32_t en_query_switch;
	uint32_t snapshot_switch;
	uint32_t top_switch;
} ;

typedef struct  
{
	string charset;
	string database;
	string ip;
	string password;
	int port;
	int timeout;
	string user;
}AppFeildDBInfo;

class SGlobalConfig {
public:
	SGlobalConfig();
	~SGlobalConfig(){}
	bool daemon;
	int iPort;
	int iTimeout;
	int iExpireTime;
	int iUpdateInterval; //词语热加载时间间隔
	int iTimeInterval;
	int iLogLevel;
	int iMaxCacheSlot; // 最大缓存条数
	int iIndexMaxCacheSlot; //倒排索引最大缓存条数
	int iVagueSwitch; // 是否模糊匹配
	int iJdqSwitch; // 是否上报jdq
	string pid_file;
	string sWordsPath;
	string sPhoneticPath;
	string sPhoneticBasePath;
	string sCharacterPath;
	string sIntelligentPath;
	string sEnIntelligentPath;
	string sEnWordsPath;
	string sRelatePath;
	string sSynonymPath;
	string sSensitivePath;
	string sAnalyzePath;
	string sTrainPath;
	string sDefaultQuery;
	string sServerName;
	string suggestPath;
	string sAppUrl;
	string sAppFieldPath;
	string sLbsUrl;
	string sSplitMode;
	string sCaDir;
	int iCaPid;
	string logPath;
};



struct IntelligentInfo {
	IntelligentInfo() {
		charact_id_01 = 0;
		charact_id_02 = 0;
		charact_id_03 = 0;
		charact_id_04 = 0;
		charact_id_05 = 0;
		charact_id_06 = 0;
		charact_id_07 = 0;
		charact_id_08 = 0;
		phonetic_id_01 = 0;
		phonetic_id_02 = 0;
		phonetic_id_03 = 0;
		phonetic_id_04 = 0;
		phonetic_id_05 = 0;
		phonetic_id_06 = 0;
		phonetic_id_07 = 0;
		phonetic_id_08 = 0;
		initial_char_01 = "";
		initial_char_02 = "";
		initial_char_03 = "";
		initial_char_04 = "";
		initial_char_05 = "";
		initial_char_06 = "";
		initial_char_07 = "";
		initial_char_08 = "";
		initial_char_09 = "";
		initial_char_10 = "";
		initial_char_11 = "";
		initial_char_12 = "";
		initial_char_13 = "";
		initial_char_14 = "";
		initial_char_15 = "";
		initial_char_16 = "";
	}
	uint16_t charact_id_01;
	uint16_t charact_id_02;
	uint16_t charact_id_03;
	uint16_t charact_id_04;
	uint16_t charact_id_05;
	uint16_t charact_id_06;
	uint16_t charact_id_07; 
	uint16_t charact_id_08;
	uint16_t phonetic_id_01;
	uint16_t phonetic_id_02;
	uint16_t phonetic_id_03;
	uint16_t phonetic_id_04;
	uint16_t phonetic_id_05;
	uint16_t phonetic_id_06;
	uint16_t phonetic_id_07;
	uint16_t phonetic_id_08;
	string initial_char_01;
	string initial_char_02;
	string initial_char_03;
	string initial_char_04;
	string initial_char_05;
	string initial_char_06;
	string initial_char_07;
	string initial_char_08;
	string initial_char_09;
	string initial_char_10;
	string initial_char_11;
	string initial_char_12;
	string initial_char_13;
	string initial_char_14;
	string initial_char_15;
	string initial_char_16;
};

class SearchConf {

public:
	SearchConf() {
	}
	static SearchConf *Instance()
	{
		return CSingleton<SearchConf>::Instance();
	}

	static void Destroy()
	{
		CSingleton<SearchConf>::Destroy();
	}

	bool ParseConf(string path);

	SGlobalConfig &GetGlobalConfig() {
		return m_GlobalConf;
	}

	SDTCHost &GetDTCConfig() {
		return m_DTCHost;
	}

	SDTCHost &GetIndexDTCConfig() {
		return m_IndexDTCHost;
	}


	ClickInfo &GetClickConfig() {
		return m_ClickInfo;
	}

	QueryInfo &GetQueryConfig() {
		return m_QueryInfo;
	}

	AppFeildDBInfo &GetAppFeildDBConfig() {
		return m_AppFeildDBInfo;
	}

	bool GetAppInfo(uint32_t appid, AppInfo &app_info);
	int UpdateAppInfo(uint32_t appid, const AppInfo &app_info);

private:
	int CommonParse(const Json::Value &dtc_config, SDTCHost &dtchost);
	int ParseDTCPara(SDTCHost &dtchost, string keyword);
	int ParseGlobalPara();
	int ParseClickPara();
	int ParseQueryPara();
	int ParseAppFeildDBPara();
private:
	SGlobalConfig m_GlobalConf;
	SDTCHost m_DTCHost;
	SDTCHost m_IndexDTCHost;
	Json::Value m_value;
	ClickInfo m_ClickInfo;
	QueryInfo m_QueryInfo;
	AppFeildDBInfo m_AppFeildDBInfo;
	map<uint32_t, AppInfo> m_AppInfo;
};

#endif
