/*
 * =====================================================================================
 *
 *       Filename:  stat_alarm_reporter.h
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
#ifndef DTC_STAT_ALARM_REPORTER_H
#define DTC_STAT_ALARM_REPORTER_H
#include <vector>
#include <string>
#include "stat_client.h"
#include "singleton.h"
typedef struct AlarmCfg
{
	uint64_t ddwStatItemId;		 /*统计项对应的统计项id*/
	uint64_t ddwThresholdValue;	 /*统计项对应的告警阈值*/
	unsigned char cat;			 /*统计项告警时依据的数据，分别为cur，10s、10m、all，默认为10s*/
	StatClient::iterator info;	 /*告警项在统计文件中的位置*/
	std::string strAlarmContent; /*告警内容字符串*/
} ALARM_CONF;
typedef std::vector<ALARM_CONF> AlarmCfgInfo;
typedef long File_Last_Modify_Time_T;

class DtcStatAlarmReporter
{
public:
	DtcStatAlarmReporter()
	{
		m_ModifyTime = 0;
		m_PostTimeOut = 1;
		init_module_id();
		init_local_id();
	}
	~DtcStatAlarmReporter()
	{
	}
	/*统计进程上报告警接口，通过配置文件的阈值和告警内容上报*/
	void report_alarm();
	/*其他进程上报告警的接口，直接上报告警内容*/
	void report_alarm(const std::string &strAlarmContent);
	bool set_stat_client(StatClient *stc);
	void set_time_out(int iTimeOut);
	/****************************************************************************************
		初始化配置的时候需要考虑两种场景：
		1、dtcserver，只需要url和cellphonenum，对于dtcserver来说是直接触发告警的
		2、stattool，即需要url和cellphonenum，又需要各个统计项的阈值 告警内容等		
		*****************************************************************************************/
	bool init_alarm_cfg(const std::string &strAlarmConfFilePath, bool IsDtcServer = false);

private:
	void post_alarm(const std::string &strAlarmContent);
	void init_module_id();
	void init_local_id();
	bool parse_alarm_cfg(uint64_t ddwStatId, const std::string &strCfgItem, AlarmCfg &cfg);
	uint64_t parse_module_id(const std::string &strCurWorkPath);
	void do_singe_stat_item_alm_report(const ALARM_CONF &almCfg);
	bool is_alarm_cfg_file_modify(const std::string &strAlarmConfFilePath);

private:
	std::string m_strReportURL;
	AlarmCfgInfo m_AlarmCfg;
	uint64_t m_ddwModuleId; /*本业务对应的ModuleId*/
	StatClient *m_stc;
	std::string m_CellPhoneList;
	std::string m_DtcIp;
	int m_PostTimeOut;
	File_Last_Modify_Time_T m_ModifyTime;
};
#define ALARM_REPORTER Singleton<DtcStatAlarmReporter>::Instance()
#endif