/////////////////////////////////////////////////////////////////
//
//   Handle All human configurable config
//       created by qiuyu on Nov 26, 2018
////////////////////////////////////////////////////////////////
#ifndef __DTC_MONITOR_CONFIG_MGR__ 
#define __DTC_MONITOR_CONFIG_MGR__ 

#include "singleton.h"
#include "json/json.h"

#include <vector>
#include <utility>
#include <string>
#include <stdint.h>

//TEST_MONITOR参数用来区分测试(open) / 预发环境与正式环境的mysql信息(close)
//#define TEST_MONITOR

#ifndef monitor_log_error
#define monitor_log_error(format, args...)     \
  log_error("<%ld>" format, pthread_self(),##args)
#endif

#ifndef monitor_log_info
#define monitor_log_info(format, args...) \
  log_info("<%ld>" format, pthread_self(),##args)
#endif

#ifndef monitor_log_crit
#define monitor_log_crit(format, args...) \
  log_crit("<%ld>" format, pthread_self(),##args)
#endif

#ifndef monitor_log_warning
#define monitor_log_warning(format, args...) \
  log_warning("<%ld>" format, pthread_self(),##args)
#endif

#ifndef monitor_log_debug
#define monitor_log_debug(format, args...) \
  log_debug("<%ld>" format, pthread_self(),##args)
#endif

class DtcMonitorConfigMgr 
{
public:
  typedef std::pair<std::string, int> Pair_t;
  typedef std::vector<std::pair<std::string, int> > PairVector_t;

  enum
  {
    eAgentDefaultTimeout = 2000, //ms
    eAgentDefaultStep = 50,
    eDtcDefaultTimeout = 2000, // ms
    eDtcDefaultStep = 70
  };

  typedef struct TimeoutSet
  {
    int sSameZoneTimeout; // ms
    int sDomesticZoneTimeout;
    int sAbroadZoneTimeout;

    bool isValid()
    {
      return sSameZoneTimeout > 0 && sDomesticZoneTimeout > 0 && sAbroadZoneTimeout > 0;
    }
  }TimeoutSet_t;

private:
  typedef struct DtcConfig
  {
    TimeoutSet_t sTimeoutSet; // ms
    int sEventDriverTimeout; // ms
    int sDetectStep;
  }DtcConf_t;

  typedef struct ConfigCenter
  {
    std::string sCaDirPath;
    int iCaPid;
    std::string sValidDir;
  }ConfigCenterContext;

  typedef struct ConfigList
  {
    int sLogLevel;
    std::string sLogFilePath;
    int sGlobalPhysicalId; // for creating global sequence id
    int sInvokeTimeout;
    std::pair<std::string, int> sListenAddr;
    PairVector_t sAdminInfo;
    PairVector_t sClusterInfo;
    DtcConf_t sDtcConf;
    std::string sReportAlarmUrl;
    std::string sAlarmReceivers;
    std::string sGetPhysicalInfoUrl;
    ConfigCenterContext oConfigCenterContext;
  }ConfList_t;

  bool mHasInit;
	ConfList_t mConfs;

public:
	DtcMonitorConfigMgr() { mHasInit = false;}

	static DtcMonitorConfigMgr* getInstance()
	{
		return CSingleton<DtcMonitorConfigMgr>::Instance();
	}

	static void Destroy()
	{
		CSingleton<DtcMonitorConfigMgr>::Destroy();
	}

	bool init(const std::string& path);
  
  inline int getLogLevel() { return mConfs.sLogLevel; }
  inline const std::string& getLogFilePath() { return mConfs.sLogFilePath; }

  inline const std::pair<std::string, int>& getListenAddr() { return mConfs.sListenAddr; }

  inline int getDtcDriverTimeout() { return mConfs.sDtcConf.sEventDriverTimeout; }
  inline int getDtcDetectStep() { return mConfs.sDtcConf.sDetectStep; }
  inline const TimeoutSet_t getDtcTimeoutSet() { return mConfs.sDtcConf.sTimeoutSet; } 

  const PairVector_t& getAdminInfo() { return mConfs.sAdminInfo; }
  const PairVector_t& getClusterInfo() { return mConfs.sClusterInfo; }

  inline const std::string& getReportAlarmUrl() {return mConfs.sReportAlarmUrl; }

  inline const std::string& getReceiverList() { return mConfs.sAlarmReceivers; }

  inline int getInvokeTimeout() { return mConfs.sInvokeTimeout; }
  inline int getPhysicalId() { return mConfs.sGlobalPhysicalId; }
  
  inline std::string getPhysicalInfoUrl() { return mConfs.sGetPhysicalInfoUrl; }

  inline const std::string& getCaDirPath() const { return mConfs.oConfigCenterContext.sCaDirPath;}
  inline int getCaPid() { return mConfs.oConfigCenterContext.iCaPid;}
  inline const std::string& getValidDir() const { return mConfs.oConfigCenterContext.sValidDir;}

private:
  bool parseLogLevel(const Json::Value& jsonValue); 
  bool parseLogFilePath(const Json::Value& jsonValue); 
  bool parseGlobalPhysicalId(const Json::Value& jsonValue);
  bool parseInvokeTimeout(const Json::Value& jsonValue);
  bool parseListenAddr(const Json::Value& jsonValue); 
  bool parseClusterHostsInfo(const Json::Value& jsonValue); 
  bool parseAdminClusterInfo(const Json::Value& jsonValue);
  bool parseAgentConf(const Json::Value& jsonValue); 
  bool parseDtcConf(const Json::Value& jsonValue); 
  bool parseReportAlarmUrl(const Json::Value& jsonValue);
  bool parseAlarmReceivers(const Json::Value& jsonValue); 
  bool parsePhysicalInfoUrl(const Json::Value& jsonValue); 
  bool parseConfigCenterContext(const Json::Value& jsonValue);
  bool getLocalIps(std::vector<std::string>& localIps);
  void trimBlank(std::string& src);

};

#endif  // __DTC_MONITOR_CONFIG_MGR__
