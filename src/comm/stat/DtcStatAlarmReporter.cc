#include "DtcStatAlarmReporter.h"
#include<unistd.h>
#include<iostream>
#include <stdlib.h> 
#include <fstream>
#include "StatInfo.h"
#include <string.h>
#include "json/json.h"
#include <sstream>
#include "dtcutils.h"
#include "log.h"
#include <sys/stat.h>
void DtcStatAlarmReporter::SetTimeOut(int iTimeOut)
{
	m_PostTimeOut = iTimeOut;
}
/*当前运行路径的第四个位置是accesskey，access的前八个字节代表moduleid*/
uint64_t DtcStatAlarmReporter::ParseModuleId(const std::string& strCurWorkPath)
{
	
	log_debug("CurWorkingPath is %s", strCurWorkPath.c_str());
	std::vector<std::string> pathVec;
	dtc::utils::SplitStr(strCurWorkPath, '/', pathVec);
	if (pathVec.size() < 5)
	{
		log_error("error working path : %s", strCurWorkPath.c_str());		
		return 0;
	}
	std::string strAccessKey = pathVec[4];
	std::string strModuleId = strAccessKey.substr(0,8);
	return atoi(strModuleId.c_str());
}
bool DtcStatAlarmReporter::SetStatClient(CStatClient* stc)
{

	if (NULL == stc)
	{
		return false;
	}
	this->m_stc = stc;
	return true;
}

void  DtcStatAlarmReporter::InitModuleId()
{
	char buf[1024];
	getcwd(buf,sizeof(buf));
	m_ddwModuleId = ParseModuleId(std::string(buf));
	
}
/*优先尝试从/usr/local/dtc/ip读取本机的ip，如果该文件没有ip，则在使用GetLocalIp函数获取本机Ip*/
void DtcStatAlarmReporter::InitLocalId()
{
	std::string strIpFilePath("/usr/local/dtc/ip");
	
	std::ifstream file;	
	file.open(strIpFilePath.c_str(), std::ios::in);
	if (file.fail())
	{	
		log_error("open file %s fail", strIpFilePath.c_str());
		m_DtcIp = dtc::utils::GetLocalIP();
		return;
	}
	std::string strLine;		
	while(std::getline(file, strLine))
	{
		if (!strLine.empty())
		{
			m_DtcIp = strLine;
			log_debug("dtc ip is %s", m_DtcIp.c_str());
			return;
		}
		
	}
	
	m_DtcIp = dtc::utils::GetLocalIP();
}
bool DtcStatAlarmReporter::parseAlarmCfg(uint64_t ddwStatId, const std::string& strCfgItem, AlarmCfg& cfg)
{
	
	cfg.ddwStatItemId = ddwStatId;
	std::vector<std::string> itemVec;
	dtc::utils::SplitStr(strCfgItem, ';', itemVec);
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
	CStatClient& stc = *m_stc;
	CStatClient::iterator si = stc[(unsigned int)ddwStatId];
	if(si==NULL)
	{
		log_error("warning: si is null");		
		return false;
	}
	if (si->iscounter()  
	   || si->isvalue() )
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
bool DtcStatAlarmReporter::IsAlarmCfgFileModify(const std::string& strAlarmConfFilePath)
{
	struct stat st;

	if(stat(strAlarmConfFilePath.c_str(), &st) != 0)
	{
		log_error("warning: stat cfg file faile");	
		return true;
	}
	
	if(st.st_mtime == m_ModifyTime)
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
bool DtcStatAlarmReporter::InitAlarmCfg(const std::string& strAlarmConfFilePath, bool IsDtcServer)
{
	/*如果配置文件没有被修改过，就不再加载配置文件*/
	if (!IsAlarmCfgFileModify(strAlarmConfFilePath))
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
	while(std::getline(file, strLine))
	{
		log_debug("Cfg Line is %s", strLine.c_str());
		std::vector<std::string> cfgVec;
		dtc::utils::SplitStr(strLine, '=', cfgVec);
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
				if (parseAlarmCfg(ddwStatId, strCfgItem, cfg))
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
void DtcStatAlarmReporter::DoSingeStatItemAlmReport(const ALARM_CONF& almCfg)
{
	if (NULL == m_stc)
	{
	
		log_error("m_stc is null");
		return;
	}
	
	uint64_t ddwStatValue = m_stc->ReadCounterValue(almCfg.info, almCfg.cat);
	log_debug("RealStatValue is %lu, ThreSholdValue is %lu", ddwStatValue, almCfg.ddwThresholdValue);
	
	if (ddwStatValue < almCfg.ddwThresholdValue)
	{
		log_debug("RealStatValue le than ThreSholdValue ");		
		return;
	}
	log_debug("post alarm ,realstatvalue %lu", ddwStatValue);
	PostAlarm(almCfg.strAlarmContent);
	
}
void DtcStatAlarmReporter::PostAlarm(const std::string& strAlarmContent)
{
	Json::Value innerBody;
	innerBody["alarm_list"] = m_CellPhoneList;	
	innerBody["alarm_title"] = std::string("dtcalarm");
	std::stringstream alarmStream;
	alarmStream << "Machine IP[" << m_DtcIp << "] ,BusinessId[" << m_ddwModuleId << "], " << strAlarmContent;
	innerBody["alarm_content"] = alarmStream.str().c_str();
	
	Json::Value body;
	body["app_name"] =  "dtc_alarm";
	body["alarm_type"] =  "sms";
	body["data"] = innerBody;
	
	Json::FastWriter writer;
	std::string strBody = writer.write(body);
	std::stringstream reqStream;
	reqStream << " req= " << strBody;
	log_info("url: %s  ,reqStream = [%s]", m_strReportURL.c_str(),reqStream.str().c_str());
	/*CurlHttp curlHttp;
	curlHttp.SetHttpParams("%s", reqStream.str().c_str());
	curlHttp.SetTimeout(m_PostTimeOut);
	BuffV buf;	
	int ret = curlHttp.HttpRequest(m_strReportURL, &buf, false, "application/x-www-form-urlencoded");
	if(ret != 0) 
	{
		log_error("curlHttp.HttpRequest Error! ret = %d", ret);
		return;
	}
	
	std::string strResp = buf.Ptr();
	log_debug("curl return rsp is %s", strResp.c_str());

	
	Json::Reader reader;
	Json::Value root;
	
	if (!reader.parse(strResp.c_str(), root, false)) 
	{		
		log_error("parse http rsp fail, rsp is %s", strResp.c_str());
		return;
	}
	std::string result = root["status"].asString();
	if ("success" != result)
	{
		log_error("http rsp result is fail, rsp is %s", strResp.c_str());	
		return;
	}
	log_debug("report success !");*/
}
void DtcStatAlarmReporter::ReportAlarm(const std::string& strAlarmContent)
{
	PostAlarm(strAlarmContent);
}
void DtcStatAlarmReporter::ReportAlarm()
{
	static int chkpt = 0;
	int curcptpoint = m_stc->CheckPoint();
	if(curcptpoint == chkpt) 
	{
		log_debug("curcptpoint[%d] is equal chkptr [%d]", curcptpoint, chkpt);
		return;
	}
	chkpt = curcptpoint;
	
	log_debug("m_AlarmCfg size is %ld ", m_AlarmCfg.size());
	for (AlarmCfgInfo::const_iterator iter = m_AlarmCfg.begin(); iter != m_AlarmCfg.end(); iter++)
	{
		
		DoSingeStatItemAlmReport(*iter);
	}
}
