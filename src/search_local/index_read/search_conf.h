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
#include <stdint.h>
using namespace std;

typedef struct
{
	string szIpadrr;
	unsigned uBid;
	unsigned uPort;
	unsigned uWeight;
	unsigned uStatus;
}SDTCroute;

typedef struct
{
	string szTablename;
	string szAccesskey;
	unsigned  uTimeout;
	unsigned  uKeytype;
	std::vector<SDTCroute> vecRoute;
}SDTCHost;

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
		int i = 0;
		for (; i < 8; i++) {
			charact_id[i] = 0;
		}
		for (i = 0; i < 8; i++) {
			phonetic_id[i] = 0;
		}
		for (i = 0; i < 16; i++) {
			initial_char[i] = "";
		}
	}
	uint16_t charact_id[8];
	uint16_t phonetic_id[8];
	string initial_char[16];
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

	AppFeildDBInfo &GetAppFeildDBConfig() {
		return m_AppFeildDBInfo;
	}

	bool GetAppInfo(uint32_t appid, AppInfo &app_info);
	int UpdateAppInfo(uint32_t appid, const AppInfo &app_info);

private:
	int CommonParse(const Json::Value &dtc_config, SDTCHost &dtchost);
	int ParseDTCPara(SDTCHost &dtchost, string keyword);
	int ParseGlobalPara();
	int ParseAppFeildDBPara();
private:
	SGlobalConfig m_GlobalConf;
	SDTCHost m_DTCHost;
	SDTCHost m_IndexDTCHost;
	Json::Value m_value;
	AppFeildDBInfo m_AppFeildDBInfo;
	map<uint32_t, AppInfo> m_AppInfo;
};

#endif
