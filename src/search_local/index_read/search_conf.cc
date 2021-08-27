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

#include <iostream>
#include "search_conf.h"
#include "log.h"
#include "comm.h"
#include <fstream>
using namespace std;
SGlobalConfig::SGlobalConfig() {
	iPort = 0;
	iTimeout = 0;
	iExpireTime = 0;
	iUpdateInterval = 0;
	iTimeInterval = 0;
	iLogLevel = 0;
	iVagueSwitch = 0;
	iMaxCacheSlot = 0;
	sCaDir = "";
	iCaPid = 0;
}

bool SearchConf::ParseConf(string path) {
	bool ret = false;
	Json::Reader reader;
	ifstream file(path.c_str());
	if (file) {
		 ret = reader.parse(file, m_value);
		 if (ret == false) {
			 log_error("parse json error!");
			 return false;
		 }
		 if (ParseGlobalPara() != 0) {
			 log_error("parse json error!");
			 return false;
		 }
		 if (ParseDTCPara(m_DTCHost, "dtc_config") != 0) {
			 log_error("parse json error!");
			 return false;
		 }
		 if (ParseDTCPara(m_IndexDTCHost, "hanpin_index_config") != 0) {
			 log_error("parse json error!");
			 return false;
		 }
		 if (ParseAppFeildDBPara() != 0) {
		 	log_error("parse json error!");
		 	return false;
		 }
	}
	else {
		log_error("open file error!");
		return false;
	}

	if (m_value.isMember("app_info") && m_value["app_info"].isArray()) {
		for (int i = 0; i < (int)m_value["app_info"].size(); i++) {
			AppInfo app_info;
			uint32_t app_id = 0;
			Json::Value info = m_value["app_info"][i];
			if (info.isMember("app_id") && info["app_id"].isInt()) {
				app_id = info["app_id"].asInt();
			}
			else {
				log_error("parse data error!");
				return false;
			}
			if (info.isMember("cache_switch") && info["cache_switch"].isInt()) {
				app_info.cache_switch = info["cache_switch"].asInt();
			}
			else {
				app_info.cache_switch = 0;
			}
			if (info.isMember("en_query_switch") && info["en_query_switch"].isInt()) {
				app_info.en_query_switch = info["en_query_switch"].asInt();
			}
			else {
				app_info.en_query_switch = 0;
			}
			if (info.isMember("snapshot_switch") && info["snapshot_switch"].isInt()) {
				app_info.snapshot_switch = info["snapshot_switch"].asInt();
			}
			else {
				app_info.snapshot_switch = 0;
			}
			if (info.isMember("top_switch") && info["top_switch"].isInt()) {
				app_info.top_switch = info["top_switch"].asInt();
			}
			else {
				app_info.top_switch = 0;
			}

			m_AppInfo.insert(make_pair(app_id, app_info));
		}
	}

	return true;
}

int SearchConf::ParseDTCPara(SDTCHost &dtchost, string keyword) {
	Json::Value dtc_config;
	if (m_value.isMember(keyword) && m_value[keyword].isObject()) {
		dtc_config = m_value[keyword];
		int ret = CommonParse(dtc_config, dtchost);
		if (ret != 0) {
			return ret;
		}
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}

	return 0;
}

int SearchConf::CommonParse(const Json::Value &dtc_config, SDTCHost &dtchost) {
	if (dtc_config.isMember("table_name") && dtc_config["table_name"].isString()) {
		dtchost.szTablename = dtc_config["table_name"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}

	if (dtc_config.isMember("accesskey") && dtc_config["accesskey"].isString()) {
		dtchost.szAccesskey = dtc_config["accesskey"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}

	if (dtc_config.isMember("timeout") && dtc_config["timeout"].isInt()) {
		dtchost.uTimeout = dtc_config["timeout"].asInt();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}

	if (dtc_config.isMember("keytype") && dtc_config["keytype"].isInt()) {
		dtchost.uKeytype = dtc_config["keytype"].asInt();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}

	if (dtc_config.isMember("route") && dtc_config["route"].isArray()) {
		for (int i = 0; i < (int)dtc_config["route"].size(); i++) {
			SDTCroute dtc_route;
			Json::Value route = dtc_config["route"][i];
			if (route.isMember("ip") && route["ip"].isString()) {
				dtc_route.szIpadrr = route["ip"].asString();
			}
			else {
				log_error("parse data error!");
				return -RT_PARSE_JSON_ERR;
			}

			if (route.isMember("bid") && route["bid"].isInt()) {
				dtc_route.uBid = route["bid"].asInt();
			}
			else {
				log_error("parse data error!");
				return -RT_PARSE_JSON_ERR;
			}

			if (route.isMember("port") && route["port"].isInt()) {
				dtc_route.uPort = route["port"].asInt();
			}
			else {
				log_error("parse data error!");
				return -RT_PARSE_JSON_ERR;
			}

			if (route.isMember("weight") && route["weight"].isInt()) {
				dtc_route.uWeight = route["weight"].asInt();
			}
			else {
				log_error("parse data error!");
				return -RT_PARSE_JSON_ERR;
			}

			if (route.isMember("status") && route["status"].isInt()) {
				dtc_route.uStatus = route["status"].asInt();
			}
			else {
				log_error("parse data error!");
				return -RT_PARSE_JSON_ERR;
			}
			dtchost.vecRoute.push_back(dtc_route);
		}
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}

	return 0;
}

int SearchConf::ParseGlobalPara()
{
	if (m_value.isMember("daemon") && m_value["daemon"].isBool()) {
		m_GlobalConf.daemon = m_value["daemon"].asBool();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("listen_port") && m_value["listen_port"].isInt()) {
		m_GlobalConf.iPort = m_value["listen_port"].asInt();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("log") && m_value["log"].isString()) {
		m_GlobalConf.logPath = m_value["log"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("log_level") && m_value["log_level"].isInt()) {
		m_GlobalConf.iLogLevel = m_value["log_level"].asInt();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("pid_file") && m_value["pid_file"].isString()) {
		m_GlobalConf.pid_file = m_value["pid_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("time_interval") && m_value["time_interval"].isInt()) {
		m_GlobalConf.iTimeInterval = m_value["time_interval"].asInt();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("update_interval") && m_value["update_interval"].isInt()) {
		m_GlobalConf.iUpdateInterval = m_value["update_interval"].asInt();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("cache_expire_time") && m_value["cache_expire_time"].isInt()) {
		m_GlobalConf.iExpireTime = m_value["cache_expire_time"].asInt();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("cache_max_slot") && m_value["cache_max_slot"].isInt()) {
		m_GlobalConf.iMaxCacheSlot = m_value["cache_max_slot"].asInt();
	}
	else {
		m_GlobalConf.iMaxCacheSlot = 100000;
	}
	if (m_value.isMember("index_cache_max_slot") && m_value["index_cache_max_slot"].isInt()) {
		m_GlobalConf.iIndexMaxCacheSlot = m_value["index_cache_max_slot"].asInt();
	}
	else {
		m_GlobalConf.iIndexMaxCacheSlot = 1000;
	}
	if (m_value.isMember("vague_switch") && m_value["vague_switch"].isInt()) {
		m_GlobalConf.iVagueSwitch = m_value["vague_switch"].asInt();
	}
	else {
		m_GlobalConf.iVagueSwitch = 1;
	}
	if (m_value.isMember("jdq_switch") && m_value["jdq_switch"].isInt()) {
		m_GlobalConf.iJdqSwitch = m_value["jdq_switch"].asInt();
	}
	else {
		m_GlobalConf.iJdqSwitch = 0;
	}
	if (m_value.isMember("phonetic_file") && m_value["phonetic_file"].isString()) {
		m_GlobalConf.sPhoneticPath = m_value["phonetic_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("phonetic_base_file") && m_value["phonetic_base_file"].isString()) {
		m_GlobalConf.sPhoneticBasePath = m_value["phonetic_base_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("character_file") && m_value["character_file"].isString()) {
		m_GlobalConf.sCharacterPath = m_value["character_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("intelligent_file") && m_value["intelligent_file"].isString()) {
		m_GlobalConf.sIntelligentPath = m_value["intelligent_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("en_intelligent_file") && m_value["en_intelligent_file"].isString()) {
		m_GlobalConf.sEnIntelligentPath = m_value["en_intelligent_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("words_file") && m_value["words_file"].isString()) {
		m_GlobalConf.sWordsPath = m_value["words_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("en_words_file") && m_value["en_words_file"].isString()) {
		m_GlobalConf.sEnWordsPath = m_value["en_words_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("relate_file") && m_value["relate_file"].isString()) {
		m_GlobalConf.sRelatePath = m_value["relate_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("synonym_file") && m_value["synonym_file"].isString()) {
		m_GlobalConf.sSynonymPath = m_value["synonym_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("sensitive_file") && m_value["sensitive_file"].isString()) {
		m_GlobalConf.sSensitivePath = m_value["sensitive_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("analyze_file") && m_value["analyze_file"].isString()) {
		m_GlobalConf.sAnalyzePath = m_value["analyze_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("training_file") && m_value["training_file"].isString()) {
		m_GlobalConf.sTrainPath = m_value["training_file"].asString();
	}
	else {
		log_error("parse data error! no training_file.");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("default_query") && m_value["default_query"].isString()) {
		m_GlobalConf.sDefaultQuery = m_value["default_query"].asString();
	}
	else {
		m_GlobalConf.sDefaultQuery = "京东";
	}
	if (m_value.isMember("service_name") && m_value["service_name"].isString()) {
		m_GlobalConf.sServerName = m_value["service_name"].asString();
	}
	else {
		m_GlobalConf.sServerName = "search_core";//search_core
	}
	if (m_value.isMember("suggest_file") && m_value["suggest_file"].isString()) {
		m_GlobalConf.suggestPath = m_value["suggest_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("app_url") && m_value["app_url"].isString()) {
		m_GlobalConf.sAppUrl = m_value["app_url"].asString();
	}
	else {
		m_GlobalConf.sAppUrl = "";
	}
	if (m_value.isMember("app_filed_file") && m_value["app_filed_file"].isString()) {
		m_GlobalConf.sAppFieldPath = m_value["app_filed_file"].asString();
	}
	else {
		m_GlobalConf.sAppFieldPath = "app_field_define.txt";
	}
	if (m_value.isMember("lbs_url") && m_value["lbs_url"].isString()) {
		m_GlobalConf.sLbsUrl = m_value["lbs_url"].asString();
	}
	else {
		m_GlobalConf.sLbsUrl = "";
	}
	if (m_value.isMember("split_mode") && m_value["split_mode"].isString()) {
		m_GlobalConf.sSplitMode = m_value["split_mode"].asString();
	}
	else {
		m_GlobalConf.sSplitMode = "Cache";
	}
	if (m_value.isMember("ca_dir") && m_value["ca_dir"].isString()) {
		m_GlobalConf.sCaDir = m_value["ca_dir"].asString();
	}
	else {
		m_GlobalConf.sCaDir = "/export/servers/dtcadmin/ca/conf/";
	}
	if (m_value.isMember("ca_pid") && m_value["ca_pid"].isInt()) {
		m_GlobalConf.iCaPid = m_value["ca_pid"].asInt();
	}
	else {
		m_GlobalConf.iCaPid = 1595901961;
	}

	return 0;
}

int SearchConf::ParseAppFeildDBPara()
{
	Json::Value db_config;
	if (m_value.isMember("appfield_db_config") && m_value["appfield_db_config"].isObject()) {
		db_config = m_value["appfield_db_config"];
		if (db_config.isMember("charset") && db_config["charset"].isString()) {
			m_AppFeildDBInfo.charset = db_config["charset"].asString();
		} else {
			m_AppFeildDBInfo.charset = "";
		}

		if (db_config.isMember("database") && db_config["database"].isString()) {
			m_AppFeildDBInfo.database = db_config["database"].asString();
		} else {
			log_error("parse data error!");
			return -RT_PARSE_JSON_ERR;
		}

		if (db_config.isMember("ip") && db_config["ip"].isString()) {
			m_AppFeildDBInfo.ip = db_config["ip"].asString();
		} else {
			log_error("parse data error!");
			return -RT_PARSE_JSON_ERR;
		}

		if (db_config.isMember("password") && db_config["password"].isString()) {
			m_AppFeildDBInfo.password = db_config["password"].asString();
		} else {
			log_error("parse data error!");
			return -RT_PARSE_JSON_ERR;
		}

		if (db_config.isMember("port") && db_config["port"].isInt()) {
			m_AppFeildDBInfo.port = db_config["port"].asInt();
		} else {
			log_error("parse data error!");
			return -RT_PARSE_JSON_ERR;
		}

		if (db_config.isMember("timeout") && db_config["timeout"].isInt()) {
			m_AppFeildDBInfo.timeout = db_config["timeout"].asInt();
		} else {
			m_AppFeildDBInfo.timeout = 0;
		}

		if (db_config.isMember("user") && db_config["user"].isString()) {
			m_AppFeildDBInfo.user = db_config["user"].asString();
		} else {
			log_error("parse data error!");
			return -RT_PARSE_JSON_ERR;
		}
	}

	return 0;
}

bool SearchConf::GetAppInfo(uint32_t appid, AppInfo &app_info) {
	if (m_AppInfo.find(appid) == m_AppInfo.end()) {
		return false;
	}
	app_info = m_AppInfo[appid];
	return true;
}

int SearchConf::UpdateAppInfo(uint32_t appid, const AppInfo &app_info) {
	m_AppInfo[appid] = app_info;
	return 0;
}