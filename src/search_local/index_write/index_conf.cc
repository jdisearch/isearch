/*
 * =====================================================================================
 *
 *       Filename:  index_conf.cc
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

#include "index_conf.h"
#include "log.h"
#include "comm.h"
#include <fstream>

SGlobalIndexConfig::SGlobalIndexConfig() {
	iTimeout = 300;
	iTimeInterval = 0;
	iLogLevel = 4;
	background = 1;
	service_type = 106;
}

UserTableContent::UserTableContent(uint32_t app_id) {
	appid = app_id;
	weight = 1;
	publish_time = time(NULL);
	top = 0;
	top_start_time = 0;
	top_end_time = 0;
}

int IndexConf::ParseDTCPara(const char *dtc_name,SDTCHost &dtchost) {
	Json::Value dtc_config;
	if (m_value.isMember(dtc_name) && m_value[dtc_name].isObject()) {
		dtc_config = m_value[dtc_name];
		if (dtc_config.isMember("table_name") && dtc_config["table_name"].isString()) {
			dtchost.szTablename = dtc_config["table_name"].asString();
		}else {
			log_error("parse data error!");
			return -RT_PARSE_JSON_ERR;
		}

		if (dtc_config.isMember("accesskey") && dtc_config["accesskey"].isString()) {
			dtchost.szAccesskey = dtc_config["accesskey"].asString();
		}else {
			log_error("parse data error!");
			return -RT_PARSE_JSON_ERR;
		}

		if (dtc_config.isMember("timeout") && dtc_config["timeout"].isInt()) {
			dtchost.uTimeout = dtc_config["timeout"].asInt();
		}else {
			log_error("parse data error!");
			return -RT_PARSE_JSON_ERR;
		}

		if (dtc_config.isMember("keytype") && dtc_config["keytype"].isInt()) {
			dtchost.uKeytype = dtc_config["keytype"].asInt();
		}else {
			log_error("parse data error!");
			return -RT_PARSE_JSON_ERR;
		}

		if (dtc_config.isMember("route") && dtc_config["route"].isArray()) {
			for (int i = 0; i < (int)dtc_config["route"].size(); i++) {
				SDTCroute dtc_route;
				Json::Value route = dtc_config["route"][i];
				if (route.isMember("ip") && route["ip"].isString()) {
					dtc_route.szIpadrr = route["ip"].asString();
				}else {
					log_error("parse data error!");
					return -RT_PARSE_JSON_ERR;
				}

				if (route.isMember("bid") && route["bid"].isInt()) {
					dtc_route.uBid = route["bid"].asInt();
				}else {
					log_error("parse data error!");
					return -RT_PARSE_JSON_ERR;
				}

				if (route.isMember("port") && route["port"].isInt()) {
					dtc_route.uPort = route["port"].asInt();
				}else {
					log_error("parse data error!");
					return -RT_PARSE_JSON_ERR;
				}

				if (route.isMember("weight") && route["weight"].isInt()) {
					dtc_route.uWeight = route["weight"].asInt();
				}else {
					log_error("parse data error!");
					return -RT_PARSE_JSON_ERR;
				}

				if (route.isMember("status") && route["status"].isInt()) {
					dtc_route.uStatus = route["status"].asInt();
				}else {
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
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}

	return 0;
}


int IndexConf::ParseGlobalPara()
{
	if (m_value.isMember("listen_addr") && m_value["listen_addr"].isString()) {
		m_GlobalConf.listen_addr = m_value["listen_addr"].asString();
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
	if (m_value.isMember("timeout") && m_value["timeout"].isInt()) {
		m_GlobalConf.iTimeout = m_value["timeout"].asInt();
	}
	else {
		m_GlobalConf.iTimeout = 5000;
	}
	if (m_value.isMember("words_file") && m_value["words_file"].isString()) {
		m_GlobalConf.sWordsPath = m_value["words_file"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("service_type") && m_value["service_type"].isString()) {
		if(m_value["service_type"].asString() == "top_index"){
			m_GlobalConf.service_type = CMD_TOP_INDEX;//top_index
		}else if(m_value["service_type"].asString() == "snapshot"){
			m_GlobalConf.service_type = CMD_SNAPSHOT;//snapshot
		}else if(m_value["service_type"].asString() == "image"){
			m_GlobalConf.service_type = CMD_IMAGE_REPORT;//image_report
		}
		else
			m_GlobalConf.service_type = CMD_INDEX_GEN;//index_gen
		m_GlobalConf.service_name = m_value["service_type"].asString();

	}
	else {
		m_GlobalConf.service_type = CMD_INDEX_GEN;//index_gen
	}
	if (m_value.isMember("stop_words_path") && m_value["stop_words_path"].isString()) {
		m_GlobalConf.stopWordsPath = m_value["stop_words_path"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("words_base_path") && m_value["words_base_path"].isString()) {
		m_GlobalConf.wordsBasePath = m_value["words_base_path"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("training_path") && m_value["training_path"].isString()) {
		m_GlobalConf.trainingPath = m_value["training_path"].asString();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("daemon") && m_value["daemon"].isBool()) {
		m_GlobalConf.background = m_value["daemon"].asBool();
	}
	else {
		log_error("parse data error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("split_mode") && m_value["split_mode"].isString()) {
		m_GlobalConf.sSplitMode = m_value["split_mode"].asString();
	}
	else
	{
		m_GlobalConf.sSplitMode = "PrePostNGram";
	}
	if (m_value.isMember("phonetic_path") && m_value["phonetic_path"].isString()) {
		m_GlobalConf.sPhoneticPath = m_value["phonetic_path"].asString();
	}
	else {
		log_error("parse data[phonetic_path] error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("character_path") && m_value["character_path"].isString()) {
		m_GlobalConf.sCharacterPath = m_value["character_path"].asString();
	}
	else {
		log_error("parse data[character_path] error!");
		return -RT_PARSE_JSON_ERR;
	}
	if (m_value.isMember("phonetic_base_file") && m_value["phonetic_base_file"].isString()) {
		m_GlobalConf.sPhoneticBasePath = m_value["phonetic_base_file"].asString();
	}
	else {
		log_error("parse data[phonetic_base_file] error!");
		return -RT_PARSE_JSON_ERR;
	}
	return 0;
}

bool IndexConf::ParseConf(string path) {
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
		 if (ParseDTCPara("dtc_index_config",m_DTCIndexHost) != 0) {
			 log_error("parse json error!");
			 return false;
		 }
		 if (ParseDTCPara("dtc_intelligent_config", m_DTCIntelligentHost) != 0) {
			 log_error("parse json error!");
			 return false;
		 }
		 if (ParseDTCPara("dtc_original_config", m_DTCOriginalHost) != 0) {
			 log_error("parse json error , in field \"dtc_original_config\" .");
			 return false;
		 }
	}
	else {
		log_error("open file error!");
		return false;
	}

	return true;
}
