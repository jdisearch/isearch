/*
 * =====================================================================================
 *
 *       Filename:  index_conf.h
 *
 *    Description:  IndexConf class definition.
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

#ifndef __INDEX_CONF_H__
#define __INDEX_CONF_H__

#include <vector>
#include <string>
#include <stdint.h>
#include "singleton.h"
#include "json/json.h"
#include <stdint.h>
#include "split_tool.h"
using namespace std;

class SGlobalIndexConfig {
public:
	SGlobalIndexConfig();
	~SGlobalIndexConfig(){}
	int iTimeout;
	int iTimeInterval;
	int iLogLevel;
	int service_type;
	string programName;
	string listen_addr;
	string pid_file;
	string logPath;
	string sWordsPath;
	string sEnWordsPath;
	string sCharacterPath;
	string sPhoneticPath;
	string sPhoneticBasePath;
	string stopWordsPath;
	string wordsBasePath;
	string trainingPath;
	string service_name;
	bool background;
	string sSplitMode;
};

class UserTableContent{
public:
	UserTableContent(uint32_t app_id);
	~UserTableContent(){}
	uint32_t appid;
	string doc_id;
	string title;
	string content;
	string author;
	string description;
	string sp_words;
	int weight;
	int publish_time;
	int top;
	int top_start_time;
	int top_end_time;
};

class IndexConf {

public:
	IndexConf() {
	}
	static IndexConf *Instance()
	{
		return CSingleton<IndexConf>::Instance();
	}

	static void Destroy()
	{
		CSingleton<IndexConf>::Destroy();
	}

	bool ParseConf(string path);

	SGlobalIndexConfig &GetGlobalConfig(){
		return m_GlobalConf;
	}

	SDTCHost &GetDTCIndexConfig(){
		return m_DTCIndexHost;
	}

	SDTCHost &GetDTCIntelligentConfig() {
		return m_DTCIntelligentHost;
	}

private:
	int ParseDTCPara(const char *dtc_name,SDTCHost &dtchost) ;
	int ParseGlobalPara();
	int ParseMYSQLPara();
private:
	SGlobalIndexConfig m_GlobalConf;
	SDTCHost m_DTCIndexHost;
	SDTCHost m_DTCIntelligentHost;
	Json::Value m_value;
};

#endif
