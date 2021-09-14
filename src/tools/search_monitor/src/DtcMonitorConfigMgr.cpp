/////////////////////////////////////////////////////////////////
//
//   Handle All human configurable config
//       created by qiuyu on Nov 26, 2018
////////////////////////////////////////////////////////////////

#include "DtcMonitorConfigMgr.h"
#include "log.h"
#include <sstream>
#include <unistd.h>
#include <fstream>
#include <algorithm>
#include <errno.h>


// variable for gdb debug
int sgPhysicalId = 0;

bool DtcMonitorConfigMgr::init(const std::string& path) 
{
  if (mHasInit)
  {
    monitor_log_info("has init this config yet.");
    return true;
  }

	bool rslt = false;
	Json::Reader reader;
  Json::Value jsonStyleValue;

  std::ifstream conf(path.c_str());
	if (!conf) 
  {
		monitor_log_error("open config file failed. fileName:%s", path.c_str());
		return false;
	}
  
  rslt = reader.parse(conf, jsonStyleValue);
  if (!rslt) 
  {
    monitor_log_error("parse config to json failed!");
    return false;
  }

  rslt = parseLogLevel(jsonStyleValue);
  if (!rslt) return false;
  
  rslt = parseLogFilePath(jsonStyleValue);
  if (!rslt) return false;
  
  rslt = parseGlobalPhysicalId(jsonStyleValue);
  if (!rslt) return false;
  
  rslt = parseInvokeTimeout(jsonStyleValue);
  if (!rslt) return false;
  
  // parse listen addr
  rslt = parseListenAddr(jsonStyleValue);
  if (!rslt) return false;

  // parse cluster host address
  rslt = parseClusterHostsInfo(jsonStyleValue);
  if (!rslt) return false;

  rslt = parseAdminClusterInfo(jsonStyleValue);
  if (!rslt) return false;

  rslt = parseDtcConf(jsonStyleValue);
  if (!rslt) return false;
  
  rslt = parseReportAlarmUrl(jsonStyleValue);
  if (!rslt) return false;
  
  rslt = parseAlarmReceivers(jsonStyleValue);
  if (!rslt) return false;

  rslt = parsePhysicalInfoUrl(jsonStyleValue);
  if (!rslt) return false;

  rslt = parseConfigCenterContext(jsonStyleValue);
  if (!rslt) return false;
  
  mHasInit = true;
  monitor_log_info("load customer config successful.");

  return true;
}

bool DtcMonitorConfigMgr::parseLogLevel(const Json::Value& jsonValue) 
{
	// Json::Value dtc_config;
	if (!jsonValue.isMember("logLevel") || !jsonValue["logLevel"].isInt())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }

  mConfs.sLogLevel = jsonValue["logLevel"].asInt();

	return true;
}

bool DtcMonitorConfigMgr::parseLogFilePath(const Json::Value& jsonValue) 
{
	// Json::Value dtc_config;
	if (!jsonValue.isMember("logFilePath") || !jsonValue["logFilePath"].isString())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }

  mConfs.sLogFilePath = jsonValue["logFilePath"].asString();

	return true;
}

bool DtcMonitorConfigMgr::parseGlobalPhysicalId(const Json::Value& jsonValue) 
{
  // 1.Because of the cluster was started up by website, so user configurable field
  // will not be allowed, remove this field from the config
  // 2.hard code the physical id will cause no bad influence, it only be used for
  //   distinguishing the sequenceId created by which physical, use kernel thread id
  //   to replace it
  sgPhysicalId = syscall(__NR_gettid);
  mConfs.sGlobalPhysicalId = sgPhysicalId % (0x7F); // one byte for physical id
  monitor_log_crit("Only print it out for debugging, physicalId:%d", sgPhysicalId);
	return true;  
  
  if (!jsonValue.isMember("physicalId") || !jsonValue["physicalId"].isInt())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }
  
  int phyId = jsonValue["physicalId"].asInt();
  if (phyId < 0)
  {
    monitor_log_error("physical id can not be negative, physicalId:%d", phyId);
    return false;
  }
  mConfs.sGlobalPhysicalId = phyId;

	return true;
}

// Due to distinguish the the physical zone for diffrent detecting timeout, the invoke
// paramter sames to be useless, we should evaluate it dynamically
bool DtcMonitorConfigMgr::parseInvokeTimeout(const Json::Value& jsonValue) 
{
  return true;
  if (!jsonValue.isMember("invokeTimeout") || !jsonValue["invokeTimeout"].isInt())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }
  
  int timeout = jsonValue["invokeTimeout"].asInt();
  if (timeout < 0)
  {
    monitor_log_error("invokeTimeout can not be negative, invokeTimeout:%d", timeout);
    return false;
  }
  mConfs.sInvokeTimeout = timeout;

	return true;
}

bool DtcMonitorConfigMgr::parseListenAddr(const Json::Value& jsonValue) 
{
  if (!jsonValue.isMember("listenAddr") || !jsonValue["listenAddr"].isObject())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }

  const Json::Value& listenAddr = jsonValue["listenAddr"];
  if (!listenAddr.isMember("ip") || !listenAddr["ip"].isString())
  {
    monitor_log_error("parse listenAddr ip failed.");
    return false;
  }
  std::string ip = listenAddr["ip"].asString();
  if (ip.empty())
  {
    monitor_log_error("ip can not be empty.");
    return false;
  }
  trimBlank(ip);
  if (ip == "*")
  {
    std::vector<std::string> localIps;
    bool rslt = getLocalIps(localIps);
    if (!rslt) return false;
    ip = localIps[0];
  }

  if (!listenAddr.isMember("port") || !listenAddr["port"].isInt())
  {
    monitor_log_error("parse listenAddr port failed.");
    return false;
  }
  int port = listenAddr["port"].asInt();
  if (port <= 0)
  {
    monitor_log_error("parse port failed. port:%d", port);
    return false;
  }

  mConfs.sListenAddr = std::make_pair(ip, port); 

	return true;
}

bool DtcMonitorConfigMgr::parseClusterHostsInfo(const Json::Value& jsonValue) 
{
	if (!jsonValue.isMember("clusterHosts") || !jsonValue["clusterHosts"].isArray())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }
  
  std::vector<std::string> localIps;
  bool rslt = getLocalIps(localIps);
  if (!rslt) return false;

  std::string ip;
  int port;
  const Json::Value& clusterInfo = jsonValue["clusterHosts"];
  for (int idx = 0; idx < (int)clusterInfo.size(); idx++)
  {
    ip = "";
    port = -1;
    const Json::Value& host = clusterInfo[idx];
    if (!host.isMember("ip") || !host["ip"].isString())
    {
      monitor_log_error("parse host ip failed.");
      return false;
    }
    ip = host["ip"].asString();
    if (ip.empty())
    {
      monitor_log_error("ip can not be empty.");
      return false;
    }
    
    // filter the local ip from the cluster config
    if (std::find(localIps.begin(), localIps.end(), ip) != localIps.end())
    {
      monitor_log_info("filter local ip:%s", ip.c_str());
      continue;
    }

    if (!host.isMember("port") || !host["port"].isInt())
    {
      monitor_log_error("parse host port failed.");
      return false;
    }
    port = host["port"].asInt();
    if (port <= 0)
    {
      monitor_log_error("parse port failed. port:%d", port);
      return false;
    }
    
    mConfs.sClusterInfo.push_back(std::make_pair(ip, port));
  }

	return true;
}

bool DtcMonitorConfigMgr::parseAdminClusterInfo(const Json::Value& jsonValue)
{
  if (!jsonValue.isMember("adminHosts") || !jsonValue["adminHosts"].isArray())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }
  const Json::Value& adminHosts = jsonValue["adminHosts"];

  for (int idx = 0; idx < (int)adminHosts.size(); idx++)
  {
    const Json::Value& host = adminHosts[idx];
    if (!host.isMember("ip") || !host["ip"].isString())
    {
      monitor_log_error("parse admin ip failed.");
      return false;
    }
    std::string ip = host["ip"].asString();

    if (!host.isMember("port") || !host["port"].isInt())
    {
      monitor_log_error("parse admin port failed.");
      return false;
    }
    int port = host["port"].asInt();
    if (port <= 0)
    {
      monitor_log_error("parse admin port failed. port:%d", port);
      return false;
    }

    mConfs.sAdminInfo.push_back(std::make_pair(ip, port));
  }

  if (mConfs.sAdminInfo.size() == 0) {
    monitor_log_error("admin info empty.");
    return false;
  }

  return true;
}

bool DtcMonitorConfigMgr::parseDtcConf(const Json::Value& jsonValue) 
{
	if (!jsonValue.isMember("dtcInfo") || !jsonValue["dtcInfo"].isObject())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }
  
  const Json::Value& dtcInfo = jsonValue["dtcInfo"];
  const Json::Value& timeoutSet = dtcInfo["detectTimeoutSet"];
  if (!(timeoutSet.isMember("sameZoneTimeout") && timeoutSet["sameZoneTimeout"].isInt())
      || !(timeoutSet.isMember("domesticZoneTimeout") && timeoutSet["domesticZoneTimeout"].isInt())
      || !(timeoutSet.isMember("abroadZoneTimeout") && timeoutSet["abroadZoneTimeout"].isInt()))
  {
    monitor_log_error("timeoutSet configuration error, check it!");
    return false;
  }

  int& ss = mConfs.sDtcConf.sTimeoutSet.sSameZoneTimeout = timeoutSet["sameZoneTimeout"].asInt();
  int& sd = mConfs.sDtcConf.sTimeoutSet.sDomesticZoneTimeout= timeoutSet["domesticZoneTimeout"].asInt();
  int& sa = mConfs.sDtcConf.sTimeoutSet.sAbroadZoneTimeout = timeoutSet["abroadZoneTimeout"].asInt();
  if (!mConfs.sDtcConf.sTimeoutSet.isValid())
  {
    monitor_log_error("confiuration error, check it!");
    return false;
  }
  // for risk controlling
  ss = ss >= eDtcDefaultTimeout ? ss : eDtcDefaultTimeout;
  sd = sd >= eDtcDefaultTimeout ? sd : eDtcDefaultTimeout;
  sa = sa >= eDtcDefaultTimeout ? sa : eDtcDefaultTimeout;
  
  // event dirver timeout
  if (!(dtcInfo.isMember("detectPeriod") && dtcInfo["detectPeriod"].isInt()))
  {
    monitor_log_error("configuration error, check it!");
    return false;
  }
  int timeout = dtcInfo["detectPeriod"].asInt();
  mConfs.sDtcConf.sEventDriverTimeout = timeout > 0 ? timeout : eDtcDefaultTimeout; 
  
  int step;
  if (!dtcInfo.isMember("detectStep") || !dtcInfo["detectStep"].isInt())
  {
    monitor_log_info("maybe missing dtc field.");
    // return false;
    step = -1;
  }
  else
  {
    step = dtcInfo["detectStep"].asInt();
  }
  step = step > 0 ? step : eDtcDefaultStep;
  mConfs.sDtcConf.sDetectStep = step;

	return true;
}

bool DtcMonitorConfigMgr::parseReportAlarmUrl(const Json::Value& jsonValue)
{
	// Json::Value dtc_config;
	if (!jsonValue.isMember("reportAlarmUrl") || !jsonValue["reportAlarmUrl"].isString())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }
  
  std::string reportAlarmUrl = jsonValue["reportAlarmUrl"].asString();
  if (reportAlarmUrl.empty())
  {
    monitor_log_error("reportAlarmUrl can not be empty.");
    return false;
  }

  mConfs.sReportAlarmUrl= reportAlarmUrl; 

	return true;
}

bool DtcMonitorConfigMgr::parseAlarmReceivers(const Json::Value& jsonValue) 
{
	if (!jsonValue.isMember("alarmReceivers") || !jsonValue["alarmReceivers"].isArray())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }
  
  std::string reci;
  std::string recList = "";
  const Json::Value& recieves = jsonValue["alarmReceivers"];
  for (int idx = 0; idx < (int)recieves.size(); idx++)
  {
    if (!recieves[idx].isString())
    {
      monitor_log_error("parse alarmReceivers failed.");
      return false;
    }
    reci = recieves[idx].asString();
    if (reci.empty())
    {
      monitor_log_error("reciever can not be empty.");
      continue;
    }
    
    recList.append(reci);
    if (idx != (int)recieves.size() - 1)
      recList.append(";");
  }
  
  if (recList.empty())
  {
    monitor_log_error("reciever list can not be empty.");
    return false;
  }
  mConfs.sAlarmReceivers = recList;

	return true;
}

bool DtcMonitorConfigMgr::parsePhysicalInfoUrl(const Json::Value& jsonValue) 
{
	if (!jsonValue.isMember("getPhysicalInfoUrl") || !jsonValue["getPhysicalInfoUrl"].isString())
  {
    monitor_log_error("incorrect field in config.");
    return false;
  }
  
  std::string physicalInfoUrl = jsonValue["getPhysicalInfoUrl"].asString();
  if (physicalInfoUrl.empty())
  {
    monitor_log_error("reportAlarmUrl can not be empty.");
    return false;
  }

  mConfs.sGetPhysicalInfoUrl = physicalInfoUrl; 
  
	return true;
}

bool DtcMonitorConfigMgr::parseConfigCenterContext(const Json::Value& jsonValue)
{
  if (!jsonValue.isMember("Config") || !jsonValue["Config"].isObject())
  {
    monitor_log_error("incorrect centerConfig info in config.");
    return false;
  }

  const Json::Value& oConfigContext = jsonValue["Config"];
  if (!oConfigContext.isMember("CaDir") || !oConfigContext["CaDir"].isString())
  {
    monitor_log_error("parse Config CaDir failed.");
    return false;
  }
  std::string sCaDir = oConfigContext["CaDir"].asString();
  if (sCaDir.empty())
  {
    monitor_log_error("CaDir can not be empty.");
    return false;
  }
  mConfs.oConfigCenterContext.sCaDirPath = sCaDir;

  if (!oConfigContext.isMember("CaPid") || !oConfigContext["CaPid"].isInt())
  {
    monitor_log_error("parse Config CaPid failed.");
    return false;
  }
  int iCaPid = oConfigContext["CaPid"].asInt();
  if (iCaPid <= 0)
  {
    monitor_log_error("parse Config CaPid failed, caPid:%d", iCaPid);
    return false;
  }
  mConfs.oConfigCenterContext.iCaPid = iCaPid;

  std::stringstream sTemp;
  sTemp << iCaPid;
  mConfs.oConfigCenterContext.sValidDir = sCaDir + sTemp.str();

	return true;
}

bool DtcMonitorConfigMgr::getLocalIps(std::vector<std::string>& localIps)
{
#ifdef TEST_MONITOR
  return true;
#else
  localIps.clear();

// this way to get host ip has some issue is the hostname is "localdomain"
//   char hname[128];
//   struct hostent *hent;
// 
//   gethostname(hname, sizeof(hname));
//   monitor_log_info("local hostname:%s", hname);
// 
//   hent = gethostbyname(hname);
//   for(int idx = 0; hent->h_addr_list[idx]; idx++)
//   {
//     std::string ip(inet_ntoa(*(struct in_addr*)(hent->h_addr_list[idx])));
//     monitor_log_info("local host ip:%s", ip.c_str());
//     
//     if (ip.empty()) continue;
//     localIps.push_back(ip);
//   }
//   
//   if (localIps.size() <= 0)
//   {
//     monitor_log_error("get local host ip failed, need to check it.");
//     return false;
//   }
  
  // get hostname from shell
  const char* shell = "ifconfig | grep inet | grep -v inet6 |\
                       grep -v 127 | awk '{print $2}' | awk -F \":\" '{print $2}'";
  
  FILE *fp;
  if ((fp = popen(shell, "r")) == NULL)
  {
    monitor_log_info("open the shell command failed.");
    return false;
  }
  
  // maybe has multiple network card
  char buf[256];
  while (fgets(buf, 255, fp) != NULL)
  {
    monitor_log_info("local ip:%s", buf);

    // the main function has ignored the signal SIGCHLD, so the recycle comand
    // SIGCHLD sent by child process of popen will be ignored, then pclose will
    // return -1 and set errno to ECHILD
    if (pclose(fp) == -1 && errno != ECHILD) 
    {
      monitor_log_info("close the file descriptor failed. errno:%d", errno);
      return false;
    }

    // remove the character '\n' from the tail of the buf because the system call
    // fgets will get it 
    std::string ip(buf);
    if (ip[ip.length() - 1] != '\n')
    {
      monitor_log_error("syntax error for fgets, ip:%s", ip.c_str());
      return false;
    }
    ip.erase(ip.begin() + (ip.length() - 1));
    monitor_log_info("local ip:%s", ip.c_str());
    localIps.push_back(ip);
  }

  if (localIps.empty())
  {
    monitor_log_error("get local ips failed.");
    return false;
  }

  return true;
#endif
}

void DtcMonitorConfigMgr::trimBlank(std::string& src)
{
  for (std::string::iterator itr = src.begin(); itr != src.end();)
  {
    if (*itr == ' ' || *itr == '\t' || *itr == '\r' || *itr == '\n')
      itr = src.erase(itr);
    else
      itr++;
  }
}
