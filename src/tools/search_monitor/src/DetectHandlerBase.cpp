///////////////////////////////////////////////////
//
// the foundation class for agent and DTC detector
//     create by qiuyu on Dec 10, 2018
//
//////////////////////////////////////////////////

#include "DetectHandlerBase.h"
#include "DBInstance.h"

#include <fstream>

#ifdef TEST_MONITOR 
// static const std::string sgDBHostName = "10.181.174.20";
// static const int sgDBPort = 3306;
// static const std::string sgDBUserName = "dtcadmin";
// static const std::string sgUserPassword = "dtcadmin@jd2015";

static const std::string sgDBHostName = "11.80.17.227";
static const int sgDBPort = 3306;
static const std::string sgDBUserName = "root";
static const std::string sgUserPassword = "root";

#else
static const std::string sgDBHostName = "my12115m.mysql.jddb.com";
static const int sgDBPort = 3358;
static const std::string sgDBUserName = "dtcadmin";
static const std::string sgUserPassword = "nbGFzcy9qb2luOztHRVQ=";
#endif
static const std::string sgDBName = "search_monitor_cluster";
extern const std::string gTableName = "distributed_monitor_cluster_lock";
static const int sgConnectTimeout = 10; // second
static const int sgReadTimeout = 5; // second
static const int sgWriteTimeout = 5; // second
// use sLockExpiredTime to delete the unexpected dead-lock,
// all timestampe base on MYSQL
extern const int sgLockExpiredTime = 10 * 60; // 10 min

DBInstance* DetectHandlerBase::mDBInstance = NULL;
std::string DetectHandlerBase::mGetPhysicalInfoUrl = "";
std::string DetectHandlerBase::mSelfAddr = "";
LirsCache*  DetectHandlerBase::mPhysicalRegionCode = NULL;
int64_t DetectHandlerBase::mCacheExpiredWhen = 0;
std::bitset<DetectHandlerBase::eMaxZoneCode> DetectHandlerBase::mRegionCodeSet;

DetectHandlerBase::~DetectHandlerBase()
{
  if (mDBInstance) delete mDBInstance;
  if (mPhysicalRegionCode) delete mPhysicalRegionCode;
}

void DetectHandlerBase::addTimerEvent()
{
  monitor_log_error("could never come here!!");
  return;
}

void DetectHandlerBase::TimerNotify(void)
{
  monitor_log_error("could never come here!!");
  return;
}

// init static member data
bool DetectHandlerBase::initHandler()
{
  if (!mDBInstance)
  {
    mDBInstance = new DBInstance();
    if (!mDBInstance)
    {
      monitor_log_error("create database instance failed.");
      return false;
    }

    bool rslt = mDBInstance->initDB(sgDBHostName, sgDBPort, sgDBName, sgDBUserName,\
        sgUserPassword, sgConnectTimeout, sgReadTimeout, sgWriteTimeout);
    if (!rslt) return false;

    rslt = mDBInstance->connectDB();
    if (!rslt) return false;
  }

  if (mGetPhysicalInfoUrl.empty()) mGetPhysicalInfoUrl = DtcMonitorConfigMgr::getInstance()->getPhysicalInfoUrl();
  if (mGetPhysicalInfoUrl.empty())
  {
    monitor_log_error("invalid url.");
    return false;
  }
  
  // No race condition
  if (!mPhysicalRegionCode) mPhysicalRegionCode = new LirsCache(10000);
  if (!mPhysicalRegionCode)
  {
    monitor_log_error("allocate cache failed.");
    return false;
  }
  
  // register region code
  if (mRegionCodeSet.none())
  {
    // need to register
    registerRegionCode();
  }

  if (mSelfAddr.empty())
  {
    const std::pair<std::string, int>& pair= DtcMonitorConfigMgr::getInstance()->getListenAddr();
    mSelfAddr = pair.first + ":" + DetectHandlerBase::toString<int>(pair.second);
  }
  if (mSelfAddr.empty())
  {
    monitor_log_error("invalid instance address!");
    return false;
  }

  return true;
}

void DetectHandlerBase::reportAlarm(const std::string& errMessage)
{
	CurlHttp curlHttp;
	BuffV buff;
	curlHttp.SetHttpParams("%s", errMessage.c_str());
	curlHttp.SetTimeout(mDriverTimeout);

	int http_ret = curlHttp.HttpRequest(mReportUrl.c_str(), &buff, false);
	if(0 != http_ret)
	{
		monitor_log_error("report alarm http error! curlHttp.HttpRequest error ret:[%d]", http_ret);
		return;
	}

	std::string response = ";";
	response= buff.Ptr();
	monitor_log_error("response from report server! ret:[%d], response:[%s]", http_ret, response.c_str());
  
  return;
}

int DetectHandlerBase::getInstanceTimeout(
    const DetectHandlerBase::DetectType type,
    const std::string& detectorAddr,
    const std::string& detectedNodeAddr)
{
  DtcMonitorConfigMgr::TimeoutSet_t tmSet;
  switch (type)
  {
/*  case DetectHandlerBase::eAgentDetect:
    tmSet = DtcMonitorConfigMgr::getInstance()->getAgentTimeoutSet(); 
    break; */
  case DetectHandlerBase::eDtcDetect:
    tmSet = DtcMonitorConfigMgr::getInstance()->getDtcTimeoutSet();
    break;
  default:
    return -1;
  }
  return tmSet.sDomesticZoneTimeout;
}

void DetectHandlerBase::getNodeInfo(
    const std::string& phyAddr,
    std::string& zoneCode)
{
  // in current now, this API is not allowed to be called
  zoneCode = "";
  return;

	CurlHttp curlHttp;
	BuffV buff;
  
  std::stringstream body;
  body << "{\"ip\":\"" << phyAddr << "\"},1,100";
  // body << "{\"ip\":\"" << "10.187.155.131" << "\"},1,100";
  monitor_log_info("body contents:%s", body.str().c_str());

	curlHttp.SetHttpParams("%s", body.str().c_str());
	curlHttp.SetTimeout(10); // 10s hard code
  
  zoneCode = "";
	int http_ret = curlHttp.HttpRequest(mGetPhysicalInfoUrl.c_str(), &buff, false);
	if(0 != http_ret)
	{
		monitor_log_error("get physical info failed! addr:%s, errorCode:%d", phyAddr.c_str(), http_ret);
    return;
	}

	std::string response = ";";
	response= buff.Ptr();
	monitor_log_info("physical info:%s", response.c_str());
	
  Json::Reader infoReader;
	Json::Value infoValue;
	if(!infoReader.parse(response, infoValue))
	{
		monitor_log_error("parse physical info failed, addr:%s, response:%s", phyAddr.c_str(), response.c_str());
		return;
	}
  
  if (!(infoValue.isMember("result") && infoValue["result"].isArray() && infoValue["result"].size() == 1))
  {
		monitor_log_error("invalid physical info, addr:%s, response:%s", phyAddr.c_str(), response.c_str());
		return;
  }
  
  const Json::Value& instanceInfo = infoValue["result"][0];
  if (!(instanceInfo.isMember("dataCenterId") && instanceInfo["dataCenterId"].isInt()))
  {
		monitor_log_error("invalid instance info, addr:%s, response:%s", phyAddr.c_str(), instanceInfo.toStyledString().c_str());
		return;
  }
  
  // a bit duplicate conversion
  long long int centerCode = instanceInfo["dataCenterId"].asInt();
  if (centerCode <= 0 || centerCode >= (1LL << REGION_CODE_SHIFT))
  {
    monitor_log_error("illegal center code, addr:%s, centerCode:%lld", phyAddr.c_str(), centerCode);
    return;
  }

  // random time to prevent cache avalanche 
  int64_t expiredTime = GET_TIMESTAMP();
  expiredTime += (sCacheExpiredInterval + (sCacheExpiredInterval > 24 ? rand() % (sCacheExpiredInterval/24) : 0)); 
  zoneCode = DetectHandlerBase::toString<long long>((expiredTime << REGION_CODE_SHIFT) + centerCode);

  return;
}

// set aboard to 1 and the others to 0
void DetectHandlerBase::registerRegionCode()
{
  // reset all bit to 0
  mRegionCodeSet.reset();

  mRegionCodeSet.set(eYNZoneCode);
  mRegionCodeSet.set(eHKTHZoneCode);
  monitor_log_error("Region code bitset size:%ld", mRegionCodeSet.size());

  return;
}

bool DetectHandlerBase::isNodeExpired(const std::string& nodeInfo)
{
  long long int va = atoll(nodeInfo.c_str());
  int64_t time = GET_TIMESTAMP();
  return (va >> REGION_CODE_SHIFT) <= time; 
}

void DetectHandlerBase::stringSplit(
    const std::string& src, 
    const std::string& delim, 
    std::vector<std::string>& dst)
{
  dst.clear();

	std::string::size_type start_pos = 0;
	std::string::size_type end_pos = 0;
	end_pos = src.find(delim);
	while(std::string::npos != end_pos)
	{
		dst.push_back(src.substr(start_pos, end_pos - start_pos));
		start_pos = end_pos + delim.size();
		end_pos = src.find(delim, start_pos);
	}
	if(src.length() != start_pos)
	{
		dst.push_back(src.substr(start_pos));
	}
}
