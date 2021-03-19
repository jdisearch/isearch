/*
 * =====================================================================================
 *
 *       Filename:  stat_alarm_reporter.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Norton, yangshuang68@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include "stat_alarm_reporter.h"
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include "stat_info.h"
#include <string.h>
#include <sstream>
#include "dtcutils.h"
#include "log.h"
#include <sys/stat.h>
void DtcStatAlarmReporter::set_time_out(int iTimeOut)
{
	m_PostTimeOut = iTimeOut;
}
/*当前运行路径的第四个位置是accesskey，access的前八个字节代表moduleid*/
uint64_t DtcStatAlarmReporter::parse_module_id(const std::string &strCurWorkPath)
{

	log_debug("CurWorkingPath is %s", strCurWorkPath.c_str());
	std::vector<std::string> pathVec;
	dtc::utils::split_str(strCurWorkPath, '/', pathVec);
	if (pathVec.size() < 5)
	{
		log_error("error working path : %s", strCurWorkPath.c_str());
		return 0;
	}
	std::string strAccessKey = pathVec[4];
	std::string strModuleId = strAccessKey.substr(0, 8);
	return atoi(strModuleId.c_str());
}
bool DtcStatAlarmReporter::set_stat_client(StatClient *stc)
{

	if (NULL == stc)
	{
		return false;
	}
	this->m_stc = stc;
	return true;
}

void DtcStatAlarmReporter::init_module_id()
{
	char buf[1024];
	getcwd(buf, sizeof(buf));
	m_ddwModuleId = parse_module_id(std::string(buf));
}
/*优先尝试从/usr/local/dtc/ip读取本机的ip，如果该文件没有ip，则在使用GetLocalIp函数获取本机Ip*/
void DtcStatAlarmReporter::init_local_id()
{
	std::string strIpFilePath("/usr/local/dtc/ip");

	std::ifstream file;
	file.open(strIpFilePath.c_str(), std::ios::in);
	if (file.fail())
	{
		log_error("open file %s fail", strIpFilePath.c_str());
		m_DtcIp = dtc::utils::get_local_ip();
		return;
	}
	std::string strLine;
	while (std::getline(file, strLine))
	{
		if (!strLine.empty())
		{
			m_DtcIp = strLine;
			log_debug("dtc ip is %s", m_DtcIp.c_str());
			return;
		}
	}

	m_DtcIp = dtc::utils::get_local_ip();
}
bool DtcStatAlarmReporter::parse_alarm_cfg(uint64_t ddwStatId, const std::string &strCfgItem, AlarmCfg &cfg)
{

	cfg.ddwStatItemId = ddwStatId;
	std::vector<std::string> itemVec;
	dtc::utils::split_str(strCfgItem, ';', itemVec);
	if (itemVec.size() < 3)
	{
		log_error("warning: bad cfg item  : %s", strCfgItem.c_str());
		return false;
	}
	cfg.ddwThresholdValue = atol(itemVec[0].c_str());
	if ("cur" == itemVec[1])
	{
		cfg.cat = SC_CUR;
	}
	else if ("10s" == itemVec[1])
	{
		cfg.cat = SCC_10S;
	}
	else if ("10m" == itemVec[1])
	{
		cfg.cat = SCC_10M;
	}
	else if ("all" == itemVec[1])
	{
		cfg.cat = SCC_ALL;
	}
	else
	{
		log_error("warning: bad cat cfg : %s", itemVec[1].c_str());
		return false;
	}

	cfg.strAlarmContent = itemVec[2];
	log_debug("strAlarmContent is %s", cfg.strAlarmContent.c_str());

	if (NULL == m_stc)
	{
		log_error("warning: m_stc is null:");
		return false;
	}
	StatClient &stc = *m_stc;
	StatClient::iterator si = stc[(unsigned int)ddwStatId];
	if (si == NULL)
	{
		log_error("warning: si is null");
		return false;
	}
	if (si->is_counter() || si->is_value())
	{
		cfg.info = si;
	}
	else
	{
		log_error("warning: cfg[%lu] type is not value or counter", ddwStatId);
		return false;
	}
	return true;
}
/***************************************************************
看配置文件是否被修改了，如果被修改需要重新加载配置
如果stat文件失败也认为需要重新加载配置
***************************************************************/
bool DtcStatAlarmReporter::is_alarm_cfg_file_modify(const std::string &strAlarmConfFilePath)
{
	struct stat st;

	if (stat(strAlarmConfFilePath.c_str(), &st) != 0)
	{
		log_error("warning: stat cfg file faile");
		return true;
	}

	if (st.st_mtime == m_ModifyTime)
	{
		return false;
	}
	/*配置文件被修改了，更新文件修改时间，同时打error日志（此类操作较少）记录该文件修改事件*/
	log_error("alarm cfg file has been modified ,the last modify time is %lu, this modify time is %lu", m_ModifyTime, st.st_mtime);
	m_ModifyTime = st.st_mtime;
	return true;
}
/***************************************************************
告警的配置项分为如下三种：
1、 上报的url(结束处不加?号)
url=http://192.168.214.62/api/dtc_sms_alarm_sender.php
2、短信通知的手机号码,中间以英文分号分开(结束处加分号)
cellphone=1283930303;1020123123;1212312312;
3、配置的告警项(结束处不加分号)
statItemId=thresholdValue;cat;alarmContent
其中cat分为10s 、cur、10m、all，如下以inc0的cpu占用率为例,
20000=8000;10s;inc0 thread cpu overload(80%)
这个配置项的含义就是接入线程cpu使用率的统计值（从10s文件中取值）大
小超过了80%，发短信通知cpu超载
所有配置的设置都采用小写英文
***************************************************************/
bool DtcStatAlarmReporter::init_alarm_cfg(const std::string &strAlarmConfFilePath, bool IsDtcServer)
{
	/*如果配置文件没有被修改过，就不再加载配置文件*/
	if (!is_alarm_cfg_file_modify(strAlarmConfFilePath))
	{
		return true;
	}
	std::ifstream file;
	file.open(strAlarmConfFilePath.c_str(), std::ios::in);
	if (file.fail())
	{
		return false;
	}

	m_AlarmCfg.clear();
	std::string strLine;
	while (std::getline(file, strLine))
	{
		log_debug("Cfg Line is %s", strLine.c_str());
		std::vector<std::string> cfgVec;
		dtc::utils::split_str(strLine, '=', cfgVec);
		if (cfgVec.size() < 2)
		{

			continue;
		}

		std::string strCfgKey = cfgVec[0];
		if ("url" == strCfgKey)
		{
			m_strReportURL = cfgVec[1];
		}
		else if ("cellphone" == strCfgKey)
		{
			m_CellPhoneList = cfgVec[1];
		}
		else
		{

			if (IsDtcServer)
			{
				continue;
			}
			uint64_t ddwStatId = atoi(strCfgKey.c_str());
			if (ddwStatId > 0)
			{

				std::string strCfgItem = cfgVec[1];
				ALARM_CONF cfg;
				if (parse_alarm_cfg(ddwStatId, strCfgItem, cfg))
				{

					log_debug("push back to alarmcfg, statid %lu ", cfg.ddwStatItemId);
					m_AlarmCfg.push_back(cfg);
				}
			}
			else
			{
				log_error("warning: bad cfg key :%s", strCfgKey.c_str());
			}
		}
	}
	file.close();
	return true;
}
void DtcStatAlarmReporter::do_singe_stat_item_alm_report(const ALARM_CONF &almCfg)
{
	if (NULL == m_stc)
	{

		log_error("m_stc is null");
		return;
	}

	uint64_t ddwStatValue = m_stc->read_counter_value(almCfg.info, almCfg.cat);
	log_debug("RealStatValue is %lu, ThreSholdValue is %lu", ddwStatValue, almCfg.ddwThresholdValue);

	if (ddwStatValue < almCfg.ddwThresholdValue)
	{
		log_debug("RealStatValue le than ThreSholdValue ");
		return;
	}
	log_debug("post alarm ,realstatvalue %lu", ddwStatValue);
	post_alarm(almCfg.strAlarmContent);
}
void DtcStatAlarmReporter::post_alarm(const std::string &strAlarmContent)
{
	return;
}
void DtcStatAlarmReporter::report_alarm(const std::string &strAlarmContent)
{
	post_alarm(strAlarmContent);
}
void DtcStatAlarmReporter::report_alarm()
{
	static int chkpt = 0;
	int curcptpoint = m_stc->check_point();
	if (curcptpoint == chkpt)
	{
		log_debug("curcptpoint[%d] is equal chkptr [%d]", curcptpoint, chkpt);
		return;
	}
	chkpt = curcptpoint;

	log_debug("m_AlarmCfg size is %ld ", m_AlarmCfg.size());
	for (AlarmCfgInfo::const_iterator iter = m_AlarmCfg.begin(); iter != m_AlarmCfg.end(); iter++)
	{

		do_singe_stat_item_alm_report(*iter);
	}
}
