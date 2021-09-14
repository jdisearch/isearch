///////////////////////////////////////////////////
//
// the dtc detector
//     create by qiuyu on Nov 27, 2018
//     Modify by chenyujie
//////////////////////////////////////////////////
#include <iostream>
#include <stdio.h>
#include "DtcDetectHandler.h"
#include "InvokeMgr.h"
#include "DBInstance.h"
#include "MysqlErrno.h"
#include "json/json.h"
#include "json/reader.h"
#include "json/writer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocal.h"
#include "MarkupSTL.h"
#include "http_helper.h"

#include <memory>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <assert.h>
//#include <sstream>

int64_t DtcDetectHandler::adminInfoIdx = 0;

static int sgTotalFailedNum = 0;

extern std::string gTableName;
extern int sgPhysicalId;
extern int sgLockExpiredTime;

#define AdminOk 200
#define RemoveCacheServer 2
const uint32_t ADMIN_PROTOCOL_MAGIC = 0x54544341;

#ifdef TEST_MONITOR
const std::string sAddOneKvInfoUrl = "http://test-ccweb.jd.com/api/addOneKvInfo2";
const std::string sUpConfigUrl = "http://test-ccweb.jd.com/api/upConfig2";
#else
const std::string sAddOneKvInfoUrl = "http://ccweb.jd.com/api/addOneKvInfo2";
const std::string sUpConfigUrl = "http://ccweb.jd.com/api/upConfig2";
#endif

DtcDetectHandler::DtcDetectHandler(CPollThread* poll)
  : DetectHandlerBase(poll)
  , mPrevDetectedIndex(-1)
  , m_pCurrentParser(ConfigCenterParser::Instance()->CreateInstance(E_SEARCH_MONITOR_JSON_PARSER))
{
  initHandler();
}

DtcDetectHandler::~DtcDetectHandler()
{
  DELETE(m_pCurrentParser);
}

void DtcDetectHandler::addTimerEvent()
{
  if (!mDetectPoll)
  {
    monitor_log_error("can not attach empty poll.");
    return;
  }
  
  // random time to trigger start for fast convergence
  int offset = rand() % (sEventTimerOffset * 2);
  int actuallyTimeout = mDriverTimeout + (offset - sEventTimerOffset < -mDriverTimeout ? 0 : offset - sEventTimerOffset); 
  return CTimerObject::AttachTimer(mDetectPoll->GetTimerListByMSeconds(actuallyTimeout));
}

// load config from dtc cluster and random to detect
// a range of dtc instance, check it.
// Every time go through here, wo need to reload the dtc 
// cluster info for avoiding the manual modify, such as web client
void DtcDetectHandler::TimerNotify(void)
{
   monitor_log_error("start to detect dtc cluster. startIdx:%d",\
      mPrevDetectedIndex >= 0 ? mPrevDetectedIndex : 0); 

  // check expired before
  verifyAllExpired(__LINE__);

  bool rslt = loadDtcClusterInfo();
  if (!rslt)
  {
    mPrevDetectedIndex = -1;
    mDtcList.clear();
    mDtcMap.clear();
    monitor_log_error("load dtc cluster info failed.");
  }
  else
  {
    // [start, end)
    int startIndex = mPrevDetectedIndex; 
    int endIndex = mPrevDetectedIndex + mDetectStep;
    endIndex = (endIndex > (int)mDtcList.size() ? (int)mDtcList.size() : endIndex);
    monitor_log_error("startIdx:%d , endIdx:%d", startIndex , endIndex);

    rslt = batchDetect(startIndex, endIndex);
    mPrevDetectedIndex = (endIndex == (int)mDtcList.size()) ? -1 : endIndex;

    if (mPrevDetectedIndex == -1)
    {
      // reach the end of the mDtcList, generate summary
      mDetectSummary << "]";
      mDetectSummary << "\n*\t\t" << "totalFailed:" << sgTotalFailedNum;
      mDetectSummary << "\n*\t";
      mDetectSummary << "\n********************************************************\
************************************************************";
      monitor_log_error("%s", mDetectSummary.str().c_str());
      sgTotalFailedNum = 0;
    }
  }
  
  // check after
  verifyAllExpired(__LINE__);
  
  //restart the dtc detect timer
  addTimerEvent();
  
  monitor_log_error("finished to detect dtc cluster.");

  return;
}

bool DtcDetectHandler::batchDetect(
    const int startIndex,
    const int endIndex)
{
  bool rslt;
  bool isAlive;
  for (int idx = startIndex; idx < endIndex; idx++)
  {
    int errCode = 0;
    std::string addr = mDtcList[idx].second;
    rslt = procSingleDetect(addr, isAlive, errCode);
    if (!isAlive)
    {
      bool needSwitch;
      // broadcast check transation for confirm
      rslt = broadCastConfirm(addr, needSwitch);
      if (needSwitch)
      {
        // verify this detect
        monitor_log_error("verify dtc instance, addr:%s", addr.c_str());
        verifySingleExpired(mDtcList[idx].first, addr);
        continue;
      }
    }
    
    // try to remove from the remaining trouble cache for subjectively alive
    InstanceInfo_t infoData("", addr, 0);
    size_t eraseNum = mTroubleInstances.erase(infoData);
    if (eraseNum > 0)
    {
      // hit the cache, print log for debugging
      monitor_log_error("dtc instance still alive for subjectively detect,\
          release it. addr:%s", addr.c_str());
    }

    std::set<std::string>::iterator iter = m_TroubleHandledIpSet.find(addr);
    if(iter != m_TroubleHandledIpSet.end())
    {
      m_TroubleHandledIpSet.erase(iter);
      monitor_log_debug("dtc instance is alive, addr:%s" , (*iter).c_str());
    }
  }
  
  return true;
}

bool DtcDetectHandler::procSingleDetect(
    const std::string& addr,
    bool& isAlive,
    int& errCode)
{
  monitor_log_info("start to detect dtc instance, dtcInfo:%s", addr.c_str());
  
  // get personal timeout for specific address
  static int timeout = 0;
  timeout = getInstanceTimeout(DetectHandlerBase::eDtcDetect, mSelfAddr, addr);
  if (timeout <= 0) timeout = DtcMonitorConfigMgr::eDtcDefaultTimeout;

  isAlive = true;
  // detect dtc 
  DetectUtil::detectDtcInstance(addr, timeout, isAlive, errCode);

  return true;
}

bool DtcDetectHandler::broadCastConfirm(
    const std::string& addr,
    bool& needSwitch)
{
  monitor_log_error("start to check dtc status. addr:%s", addr.c_str());
  
  static int timeout;
  timeout = getInstanceTimeout(DetectHandlerBase::eDtcDetect, mSelfAddr, addr);
  if (timeout <= 0) timeout = DtcMonitorConfigMgr::eDtcDefaultTimeout;

  needSwitch = false;
  bool rslt = InvokeMgr::getInstance()->invokeVoteSync(
      DetectHandlerBase::eDtcDetect, addr, addr, timeout, needSwitch);
  
  return rslt;
}

void DtcDetectHandler::verifyAllExpired(const int line)
{
  if (mTroubleInstances.size() <= 0) return;

  monitor_log_error("recheck all trouble instance in cache start, line:%d,\
      size:%lu", line, mTroubleInstances.size());
  std::set<InstanceInfo_t>::iterator itr = mTroubleInstances.begin();
  while (itr != mTroubleInstances.end())
  {
    monitor_log_error("verify dtc instance, sharding:%s addr:%s", (*itr).sAccessKey.c_str(), (*itr).sIpWithPort.c_str());
    if (1 == verifySingleExpired((*itr).sAccessKey, (*itr).sIpWithPort)) {
      mTroubleInstances.erase(itr++);
    } else {
      itr++;
    }
  }
  monitor_log_error("recheck all trouble instance in cache finished, size:%lu", mTroubleInstances.size());
}

// Return Value:
//  1 : the instance has expired and removed from the set
//  0 : the instance not expired
//  -1 : processing error
int DtcDetectHandler::verifySingleExpired(const std::string& sIP, const std::string& addr)
{
  int errCode;
  bool rslt, isAlive;
  int64_t nowTimestame = GET_TIMESTAMP();
  
  InstanceInfo_t infoData(sIP, addr, nowTimestame);
  std::set<InstanceInfo_t>::iterator itr = mTroubleInstances.find(infoData);
  if (itr == mTroubleInstances.end())
  {
    mTroubleInstances.insert(infoData);
    monitor_log_error("dtc instance maybe not alive, caching it. addr:%s,\
        cacheSize:%lu", addr.c_str(), mTroubleInstances.size());
    return 0;
  }
  else
  {
    rslt = (*itr).isExpired(nowTimestame);
    if (!rslt)
    {
      monitor_log_error("not expire time, diffTimeStamp:%ld" , (*itr).sExpiredTimestamp - nowTimestame);
      return 0;
    } 
  }
  
  // double check
  bool needSwitch = false;
  rslt = procSingleDetect(addr, isAlive, errCode);
  if (!isAlive)
  {
    // broadcast check transation for confirm
    rslt = broadCastConfirm(addr, needSwitch);
    // here can confirm the ip has problems
    if (needSwitch)
    {
      std::set<std::string>::iterator iter = m_TroubleHandledIpSet.find(addr);
      if(iter == m_TroubleHandledIpSet.end())
      {
        m_TroubleHandledIpSet.insert(addr);
        monitor_log_debug("insert one bad dtc add:%s" , addr.c_str());
      }
      doDtcSwitch(sIP, addr, errCode);
    }
  }

  if (!needSwitch)
  {
    monitor_log_error("dtc instance still alive, releasing it. addr:%s,\
        cacheSize:%lu", addr.c_str(), mTroubleInstances.size());
  }
  else
  {
    monitor_log_error("dtc instance has been dead, releasing it. addr:%s,\
        cacheSize:%lu", addr.c_str(), mTroubleInstances.size());
  }

  return 1;
}

bool DtcDetectHandler::reportDtcAlarm(
    const DetectHandlerBase::ReportType type,
    const std::string& addr,
    const int errCode)
{
#ifdef TEST_MONITOR
  monitor_log_error("print for check build with TEST_MONITOR flag.");
  static int reportNum = 0;
  if (reportNum > 7) return true;
  reportNum++;
#endif

  std::stringstream oss;
  switch (type)
  {
  case eDtcDetectFailed:
    oss << "detect dtc failed! addr:" << addr << " errCode:" << errCode;
    break;
  case eSwitchDtcFailed:
    oss << "offline dtc failed! addr:" << addr << " errCode:" << errCode;
    break;
  case eSwitchDtcSuccess:
    oss << "offline dtc successfully. addr:" << addr;
    break;
  case eAccessDBFailed:
    oss << "get distributed cluster lock failed for consistency. failed dtc addr:"\
      << addr << " errCode:" << errCode;
    break;
  case eAccessCCFailed:
    oss << "notify config center failed! failed dtc addr:"<< addr;
    break;
  case eSwitchSlaveDtc:
    oss << "no need to switch slave dtc! addr:" << addr;
    // break;
    // just log it, no need to report alarm!
    monitor_log_error("%s", oss.str().c_str());
    return true;
  case eReplicateDtcFailed:
    oss<< "replicate dtc is offline, please check! addr:" << addr;
    break;
  case eNoneUsefulDtc:
    oss<< " addr:" << addr << "'s shardingName has no useful dtc.";
    break;
  default:
    monitor_log_error("unkonwn report type, type:%d", (int)type);
    return false;
  }

  Json::Value reportContents;

  char formatTime[255];
  time_t currentTime;
  time(&currentTime);
  struct tm* localTime = localtime(&currentTime);
  strftime(formatTime, 255, "%Y-%m-%d %H:%M:%S", localTime);
  std::string strLocalTime(formatTime);

  reportContents["appName"] = "da_search";
  reportContents["alarmTitle"] = "检索存储服务告警";
  reportContents["key"] = "searchMonitor";
  reportContents["service"] = "searchMonitor";
  reportContents["sendTime"] = strLocalTime;
  reportContents["alarmList"] = DtcMonitorConfigMgr::getInstance()->getReceiverList();
  reportContents["erp"] = "chenyujie28";
  reportContents["content"] = oss.str();
  reportContents["devList"] = "chenyujie28";
  reportContents["dept"] = "技术保障组";
  
  Json::FastWriter writer;
  std::string strOut = writer.write(reportContents);
  
  monitor_log_info("report dtc alarm:%s", strOut.c_str());
  reportAlarm(strOut);
  
  return true;
}

// response from fronted with the following format:
// {
//   "status":200,
//   "msg":"xxxx",
//   "data":"xxx"
// }
bool DtcDetectHandler::doDtcSwitch(
    const std::string& sIP,
    const std::string& addr,
    const int errCode)
{
  if (mDtcMap.find(sIP) == mDtcMap.end()) {
    return true;
  }

  // nite, before do switch, wo need to recheck the instance realtimely 
  // in case of the regularly deletion
  // load the instances from config central realtimely
  bool rslt = loadDtcClusterInfoNoOpr();
  if (!rslt) return true;

  std::pair<std::string, std::string> tmp(sIP, addr);
  if (std::find(mCheckDtcList.begin(), mCheckDtcList.end(), tmp) == mCheckDtcList.end())
  {
    monitor_log_error("this instance maybe has removed from the cluster. addr:%s", addr.c_str());
    return true;
  }

  std::vector<std::string> subStrs;
  stringSplit(addr, ":", subStrs);
  if (subStrs.size() != 2)
  {
    monitor_log_error("invalid dtc address. addr:%s", addr.c_str());
    return false;
  }
  
  int ret = getDistributedLockForConsistency(DISTRIBUTE_LOCK_SOLE_IP, DISTRIBUTE_LOCK_SOLE_PORT);
  if (ret < 0)
  {
    // get the cluster consistency lock failed, only report alarm, do not switch
    monitor_log_error("access the database for consistent switching failed. addr:%s", addr.c_str());
    reportDtcAlarm(DetectHandlerBase::eAccessDBFailed, addr, errCode);
    return true; 
  }
  else if (ret == 0)
  {
    // lock has been gotten by other peer, that peer would to switch the failed dtc,
    // so need to do nothing
    monitor_log_error("consistent lock has been gotten by the peer. addr:%s", addr.c_str());
    return true;
  }

  // come here, got the cluster lock and become to leader role, do switch
  monitor_log_error("get consistent lock successful. addr:%s", addr.c_str());
  
  if (!loadSearchCoreClusterInfo())
  {
    monitor_log_error("load searchCoreCluster fail.");
    releaseDistributedLock(DISTRIBUTE_LOCK_SOLE_IP, DISTRIBUTE_LOCK_SOLE_PORT);
    return false;
  }

  sgTotalFailedNum++;
  mDetectSummary << sIP << " " << addr << " ";

  // double check sIP is master or not
  // Need Check sIP'index_Gen is master or replicate
  // if master, then should remove sc and switch master --> Replicate
  // else if replicate, then should should remove sc and reportDtcAlarm
  if (!CheckIndexGenIsMaster(sIP))
  {
    std::vector<ServerNode>& serverNodes = mDtcMap[sIP];
    std::vector<ServerNode>::iterator itr;
    bool bHasScConfig = false;
    for (itr = serverNodes.begin(); itr != serverNodes.end(); )
    {
      // hit replicate DTC corresponding SC
      if (itr->ip == subStrs[0]
      && SEARCH_ROLE == (itr->role))
      {
        itr = serverNodes.erase(itr);
        bHasScConfig = true;
        continue;
      }
      ++itr;
    }
    
    static uint32_t iReportConut = 1; 
    static const uint32_t iReportTimeOut = 5; // 5 min
    if (!bHasScConfig)
    {
      if (0 == (iReportConut++)% iReportTimeOut)
      {
        reportDtcAlarm(DetectHandlerBase::eReplicateDtcFailed, addr, errCode);
      }
      releaseDistributedLock(DISTRIBUTE_LOCK_SOLE_IP, DISTRIBUTE_LOCK_SOLE_PORT);
      return true;
    }
  }
  else
  {
    std::vector<ServerNode>& serverNodes = mDtcMap[sIP];
    std::vector<ServerNode>::iterator itr;
    for (itr = serverNodes.begin(); itr != serverNodes.end(); )
    {
      // hit master DTC corresponding SC and Gen
      if (itr->ip == subStrs[0])
      {
        if( INDEX_GEN_ROLE == (itr->role)
        && (DISASTER_ROLE_MASTER == itr->sDisasterRole))
        {
          itr->sDisasterRole = DISASTER_ROLE_REPLICATE;
        }
        else if(SEARCH_ROLE == (itr->role))
        {
          itr = serverNodes.erase(itr);
          continue;
        }
      }
      ++itr;
    }
  
    bool bHasChange = false;
    SearchCoreClusterCtxIter iter = mDtcMap.begin();
    for( ; iter != mDtcMap.end() && (!bHasChange); ++iter)
    {
      //handle in the same shardingName
      if(mDtcMap[iter->first][0].sShardingName != mDtcMap[sIP][0].sShardingName)
      {
        continue;
      }
    
      std::set<std::string>::iterator oTroubleHandledIter = m_TroubleHandledIpSet.find(addr);
      // find one vaild dtc ip
      if (oTroubleHandledIter == m_TroubleHandledIpSet.end())
      {
        std::vector<ServerNode>::iterator oServerNodeIter = (iter->second).begin();
        for( ; oServerNodeIter != (iter->second.end()) ; ++oServerNodeIter)
        {
          if( DISASTER_ROLE_REPLICATE == oServerNodeIter->sDisasterRole)
          {
            oServerNodeIter->sDisasterRole = DISASTER_ROLE_MASTER;
            bHasChange = true;
            break;
          }
        }
      }
    }
  
    if(!bHasChange)
    {
      reportDtcAlarm(DetectHandlerBase::eNoneUsefulDtc, addr, errCode);
    }
  }

  std::string sVaildDir = DtcMonitorConfigMgr::getInstance()->getValidDir();
  long iPreModifyTime = ConfigCenterParser::Instance()->GetConfigCurrentTime(sVaildDir);

  ret = NotifyConfigCenter();

  if (ret < 0) {
    monitor_log_error("notify config center failed. ip:%s", subStrs[0].c_str());
    reportDtcAlarm(DetectHandlerBase::eAccessCCFailed, addr, ret);
    releaseDistributedLock(DISTRIBUTE_LOCK_SOLE_IP, DISTRIBUTE_LOCK_SOLE_PORT);
    return false;
  }

  // need check ca config is modify or not before release distributed lock
  static const uint16_t iCheckConfigTimeOutCount = 30; // (5 * 6) * 10s == 5 min
  uint16_t uiLoopCount = 1;
  while(!ConfigCenterParser::Instance()->CheckConfigModifyOrNot(iPreModifyTime)
  && (0 != (uiLoopCount++ % iCheckConfigTimeOutCount)) )
  {
    monitor_log_debug("config center is handling...");
    sleep(10); // 10s
    monitor_log_debug("waiting... step into next loop.");
  }

  monitor_log_error("dtc offline successful. ipWithPort:%s", addr.c_str());
  reportDtcAlarm(DetectHandlerBase::eSwitchDtcSuccess, addr, -1);
  releaseDistributedLock(DISTRIBUTE_LOCK_SOLE_IP, DISTRIBUTE_LOCK_SOLE_PORT);
 
  return true;
}

int DtcDetectHandler::NotifyConfigCenter()
{
  SearchCoreClusterCtxIter iter = mDtcMap.begin();
  SearchCoreClusterContextType oOriginalSCMap;
  for( ;iter != mDtcMap.end(); ++iter)
  {
    std::vector<ServerNode>::iterator oServerNodeIter = (iter->second).begin();
    for( ; oServerNodeIter != (iter->second).end() ; ++oServerNodeIter)
    {
      ServerNode oTempServerNode;
      oTempServerNode(oServerNodeIter->ip,
      oServerNodeIter->port,
      oServerNodeIter->weight,
      oServerNodeIter->role,
      oServerNodeIter->sDisasterRole,
      "");

      oOriginalSCMap[oServerNodeIter->sShardingName].push_back(oTempServerNode);
    }
  }

  Json::Value oShardingArray;
  iter = oOriginalSCMap.begin();
  for( ;iter != oOriginalSCMap.end(); ++iter)
  {
    Json::Value oShardingObject;
    std::vector<ServerNode>::iterator oServerNodeIter = (iter->second).begin();
    for( ; oServerNodeIter != (iter->second).end() ; ++oServerNodeIter)
    {
        Json::Value oInstanceValue;
        oInstanceValue["ip"] = oServerNodeIter->ip;
        oInstanceValue["port"] = oServerNodeIter->port;
        oInstanceValue["weight"] = oServerNodeIter->weight;
        oInstanceValue["role"] = oServerNodeIter->role;
       
        if (!oServerNodeIter->sDisasterRole.empty()
          && INDEX_GEN_ROLE == oServerNodeIter->role)
        {
          oInstanceValue["disasterRole"] = oServerNodeIter->sDisasterRole;
        }

        if(!oShardingObject["ShardingName"].isString())
        {
          oShardingObject["ShardingName"] = iter->first;
        }

        oShardingObject["INSTANCE"].append(oInstanceValue);
    }

    if(!oShardingObject.isNull())
    {
      oShardingArray["SERVERSHARDING"].append(oShardingObject);
    }
  }
  Json::Value oModuleObject;
  oModuleObject["MODULE"] = oShardingArray;

  Json::Value oContext;
  oContext["info"] = oModuleObject.toStyledString();
  oContext["appid"] = DtcMonitorConfigMgr::getInstance()->getCaPid();

  Json::FastWriter writer;
  std::string strWrite = writer.write(oContext);
  log_debug("send request context:%s start", strWrite.c_str());

  // addOneKvInfo
  HttpHelper oHttpHelper;
  HttpResponse oHttpResponse;
  oHttpHelper.SetAttribute(CURLOPT_TIMEOUT, 30);
  oHttpHelper.SetAttribute(CURLOPT_NOSIGNAL, 1L);
  
  log_debug("addOneKvInfo request of url:%s start", sAddOneKvInfoUrl.c_str());
  bool http_result = oHttpHelper.Post(sAddOneKvInfoUrl, strWrite, oHttpResponse);
		if (http_result) {
			if (CURLE_OK == oHttpResponse.GetCurlCode() && OK == oHttpResponse.GetHttpCode()) {
				log_debug("request of url:%s success , response:%s", sAddOneKvInfoUrl.c_str() , oHttpResponse.GetBody().c_str());
			}
			else {
				log_error("curl error. url:%s, curl:%d, http:%d.",
					sAddOneKvInfoUrl.c_str(), oHttpResponse.GetCurlCode(), oHttpResponse.GetHttpCode());
				return -RT_HTTP_ERR;
			}
		}
		else {
			log_error("http error.");
			return -RT_HTTP_ERR;
		}
  
  // parse response by json
  Json::Reader response_reader(Json::Features::strictMode());
  Json::Value oResponseContext;
  bool bRet = response_reader.parse(oHttpResponse.GetBody() , oResponseContext);
  int iData = 0;
  if(bRet && oResponseContext.isMember("data") && oResponseContext["data"].isInt())
  {
    iData = oResponseContext["data"].asInt();
  }
  else
  {
    log_error("no data in response , response data is invaild, %s" , oHttpResponse.GetBody().c_str());
    return -RT_PARSE_CONF_ERR;
  }

  // upConfig
  Json::Value oUpConfigContext;
  oUpConfigContext["id"] = iData;
  strWrite = writer.write(oUpConfigContext);

  log_debug("upConfig request of url:%s start , configId:%d", sUpConfigUrl.c_str() , iData);
  http_result = oHttpHelper.Post(sUpConfigUrl, strWrite, oHttpResponse);
		if (http_result) {
			if (CURLE_OK == oHttpResponse.GetCurlCode() && OK == oHttpResponse.GetHttpCode()) {
				log_debug("request of url:%s success", sUpConfigUrl.c_str());
			}
			else {
				log_error("curl error. url:%s, curl:%d, http:%d.",
					sUpConfigUrl.c_str(), oHttpResponse.GetCurlCode(), oHttpResponse.GetHttpCode());
				return -RT_HTTP_ERR;
			}
		}
		else {
			log_error("http error.");
			return -RT_HTTP_ERR;
		}

  return 0;
}

bool DtcDetectHandler::CheckIndexGenIsMaster(const std::string& sIP)
{
  if(mDtcMap.find(sIP) != mDtcMap.end())
  {
    std::vector<ServerNode> oSeverNodes = mDtcMap[sIP];
    std::vector<ServerNode>::iterator iter = oSeverNodes.begin();
    for ( ; iter != oSeverNodes.end(); ++iter)
    {
      if(sIP == (iter->ip)
      &&(INDEX_GEN_ROLE == iter->role))
      {
        return (DISASTER_ROLE_MASTER == iter->sDisasterRole);
      }
    }
  }

  return false;
}

int DtcDetectHandler::NotifyRouteAdmin(const std::string& ip, 
  const uint32_t port, 
  const std::string& sharding, 
  const std::string& localip,
  const std::string& localport)
{
  int sockfd = Connect(ip, port);
  if (sockfd == -1)
    return -1;

  int msgsize = 0;
  char* sendbuf = MakeAdminPackage(sharding, localip, localport, msgsize);
  int rtn = send(sockfd, sendbuf, msgsize, 0);
  if (rtn <= 0) {
    monitor_log_error("send packet to admin [%s-%d] server error", ip.c_str(), port);
    free(sendbuf); sendbuf = NULL;
    close(sockfd);
    return -1;
  }
  free(sendbuf); sendbuf = NULL;
  monitor_log_debug("send packet to admin [%s-%d] server success", ip.c_str(), port);

  msgsize = 0;
  char header[sizeof(ProtocalHeader)];
  while (1) {
    rtn = recv(sockfd, header + msgsize, sizeof(ProtocalHeader) - msgsize, 0);
    if (rtn <= 0 ) {
      close(sockfd);
      monitor_log_error("recv header from admin [%s-%d] server error", ip.c_str(), port);
      return -1;
    }
    msgsize += rtn;
    if (msgsize == sizeof(ProtocalHeader)) {
      monitor_log_debug("recv header from admin [%s-%d] server success", ip.c_str(), port);
      break;
    }
  }

  msgsize = 0;
  ProtocalHeader* packetHeader = reinterpret_cast<ProtocalHeader*>(header);

  int pktlen = packetHeader->length;
  if (ADMIN_PROTOCOL_MAGIC != packetHeader->magic || pktlen <= 0 || pktlen >= PACKET_MAX_LENGTH) {
    monitor_log_error("header info error");
    close(sockfd);
    return -1;
  }

  char *recvbuf = (char*)malloc(pktlen+1);
  while (1) {
    rtn = recv(sockfd, recvbuf + msgsize, pktlen - msgsize, 0);
    if (rtn <= 0) {
      free(recvbuf); recvbuf = NULL;
      close(sockfd);
      monitor_log_error("recv packet from admin [%s-%d] server error", ip.c_str(), port);
      return -1;
    }
    msgsize += rtn;
    if (msgsize == pktlen) {
      monitor_log_debug("recv packet from admin [%s-%d] server success", ip.c_str(), port);
      break;
    }
  }
  close(sockfd);
  recvbuf[pktlen] = 0;

  int code;
  Json::Value recv_packet;
  Json::Reader r(Json::Features::strictMode());
  int ret = r.parse(recvbuf, recvbuf + pktlen, recv_packet);
  if (0 == ret)
  {
    free(recvbuf); recvbuf = NULL;
    monitor_log_error("the err json string is : %s", recvbuf);
    monitor_log_error("parse json error , errmsg : %s", r.getFormattedErrorMessages().c_str());
    return -1;
  }

  if (recv_packet.isMember("code") && recv_packet["code"].isInt())
  {
    code = recv_packet["code"].asInt();
  } else {
    monitor_log_error("the code in json string [%s] is incorrect", recvbuf);
    free(recvbuf); recvbuf = NULL;
    return -1;
  }

  if (code != AdminOk) {
    free(recvbuf); recvbuf = NULL;
    monitor_log_error("the code is %d", code);
    return -1;
  }

  free(recvbuf); recvbuf = NULL;
  return 0;
}

int DtcDetectHandler::Connect(const std::string& ip, const uint32_t port)
{
  struct sockaddr_in inaddr;
  bzero(&inaddr, sizeof(struct sockaddr_in));
  inaddr.sin_family = AF_INET;
  inaddr.sin_port   = htons(port);
  if(inet_pton(AF_INET, ip.c_str(), &inaddr.sin_addr) <= 0)
  {
    log_error("invalid admin address %s-%d", ip.c_str(), port);
    return -1;
  }

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    log_error("%s", "socket error,%m");
    return -1;
  }

  int ret = connect(fd, (const struct sockaddr *)&inaddr, sizeof(inaddr));
  if (ret < 0) {
    log_error("connect admin address %s-%d failed", ip.c_str(), port);
    return -1;
  }

  struct timeval timeout={5,0};
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  return fd;
}

char* DtcDetectHandler::MakeAdminPackage(const std::string& shardingName, const std::string& ip, const std::string& port, int& msgsize)
{
  Json::FastWriter writer;
  Json::Value request;
  request["shardingName"] = Json::Value(shardingName.c_str());
  request["ip"] = Json::Value(ip.c_str());
  request["port"] = Json::Value(port.c_str());
  std::string req = writer.write(request);

  msgsize = sizeof(ProtocalHeader) + req.size();
  char *buffer = (char*)malloc(msgsize);
  ProtocalHeader* header = (ProtocalHeader*)buffer;
  header->magic = ADMIN_PROTOCOL_MAGIC;
  header->cmd = RemoveCacheServer;
  header->length = req.size();

  memcpy(buffer+sizeof(ProtocalHeader), req.c_str(), req.size());
  return buffer;
}

bool DtcDetectHandler::loadDtcClusterInfo()
{
  if (mPrevDetectedIndex >= 0)
  {
    // the previous load has not completely finished. no need to reload the dtcInfo
    monitor_log_info("re-run the previous loop.");
    return true;
  }

  sgTotalFailedNum = 0;
  mPrevDetectedIndex = 0;
  mDtcMap.clear();
  mDtcList.clear();

  if(NULL == m_pCurrentParser){ return false; }

  bool bRet = ConfigCenterParser::Instance()->UpdateParser(m_pCurrentParser);
  if(!bRet) { return false;}
  
  mDtcMap = m_pCurrentParser->GetSearchCoreClusterContext();
  mDtcList = m_pCurrentParser->GetDtcClusterContext();

  mDetectSummary.str("");
  // append information into summary statictic
  mDetectSummary << "\n";
  mDetectSummary << "***********************************************************\
***************************************";
  mDetectSummary << "\n*\t\t\t\t\t\t\tDtcDetectSummary:";
  mDetectSummary << "\n*\t\t" << "totalSize:" << mDtcList.size();
  mDetectSummary << "\n*\t\t" << "failedList:[";

  return true;
}

// just load cluster info for recheck, no need to do extra operation
bool DtcDetectHandler::loadDtcClusterInfoNoOpr()
{
  if(NULL == m_pCurrentParser){ return false; }
  bool bRet = ConfigCenterParser::Instance()->UpdateParser(m_pCurrentParser);
  if(!bRet) { return false;}
  mCheckDtcList.clear();
  mCheckDtcList = m_pCurrentParser->GetDtcClusterContext();
  return true;
}

bool DtcDetectHandler::loadSearchCoreClusterInfo()
{
  if(NULL == m_pCurrentParser){ return false; }
  bool bRet = ConfigCenterParser::Instance()->UpdateParser(m_pCurrentParser);
  if(!bRet) { return false;}
  mDtcMap.clear();
  mDtcMap = m_pCurrentParser->GetSearchCoreClusterContext();
  return true;
}

void DtcDetectHandler::initHandler()
{
  bool rslt = DetectHandlerBase::initHandler();
  if (!rslt) return;

  mReportUrl = DtcMonitorConfigMgr::getInstance()->getReportAlarmUrl();
  mDriverTimeout = DtcMonitorConfigMgr::getInstance()->getDtcDriverTimeout();
  mDetectStep = DtcMonitorConfigMgr::getInstance()->getDtcDetectStep();

  return;
}

// get the cluster distributed lock for consistency, only the leader can do switch
// return value:
//  1. > 0 : get lock successful
//  2. = 0 : lock has been gotten by other peers
//  3. < 0 : do database related operation failed
int DtcDetectHandler::getDistributedLockForConsistency(
    const std::string& ip,
    const std::string& port)
{
  // std::string ip = "192.168.7.26";
  // int port = 16877;

  if (!mDBInstance)
  {
    bool rslt = DetectHandlerBase::initHandler();
    if (!rslt) return -1;
  }
  
  // instance_type: 0(agent), 1(dtc)
  std::stringstream insertSql;
  insertSql << "insert into " << gTableName << "(ip, port, instance_type,\
       machine_id) values(\"" << ip << "\"," << port << ",1," << sgPhysicalId << ");";
  int ret = mDBInstance->execSQL(insertSql.str().c_str());
  // when insert an duplicate key into table which attribute has been setted to
  // be unique key, errno 'ER_DUP_ENTRY' will returned
  if (ret == -ER_DUP_ENTRY)
  {
    // need to recheck whether the exist unique key is experid or not, if yes,
    // delete the key and insert it for one more time
    mDBInstance->freeResult();
    
    std::stringstream qSql;
    qSql << "select unix_timestamp(create_time), unix_timestamp(now()) from "\
      << gTableName << " where ip =\"" << ip << "\" and port = " << port << ";";
    ret = mDBInstance->execSQL(qSql.str().c_str());
    if (ret < 0)
    {
      std::stringstream ss;
      ss << ip << ":" << port;
      monitor_log_error("get lock created time failed. addr:%s", ss.str().c_str());
      mDBInstance->freeResult();
      return -1;
    }
    
    // if the record has not exist in the table, it means that one peer had make the
    // switching done, return directly
    ret = mDBInstance->getNumRows();
    if (ret <= 0)
    {
      std::stringstream ss;
      ss << ip << ":" << port;
      monitor_log_error("peer had make switching done. addr:%s", ss.str().c_str());
      return 0;
    }

    // check expired
    ret = mDBInstance->fetchRow();
    if (ret < 0)
    {
      std::stringstream ss;
      ss << ip << ":" << port;
      monitor_log_error("sys error. addr:%s", ss.str().c_str());
      return 0;
    }

    char *fieldValue1 = mDBInstance->getFieldValue(0);
    char *fieldValue2 = mDBInstance->getFieldValue(1);
    if (NULL == fieldValue1 || NULL == fieldValue2)
    {
      monitor_log_error("get field value failed.");
      mDBInstance->freeResult();
      return -1;
    }
    int createTime = atoi(fieldValue1);
    int nowTime = atoi(fieldValue2);
    if (createTime <= 0 || nowTime <= 0)
    {
      monitor_log_error("get field value failed.");
      mDBInstance->freeResult();
      return -1;
    }
    monitor_log_error("create_time:%d, now_time:%d", createTime, nowTime);

    if (nowTime - createTime > sgLockExpiredTime)
    {
      // lock is expired, delete it
      mDBInstance->freeResult();
      std::stringstream dSql;
      dSql << "delete from " << gTableName << " where ip =\"" << ip << "\" and port = " << port << ";";
      ret = mDBInstance->execSQL(dSql.str().c_str());
      if (ret < 0)
      {
        monitor_log_error("delete record failed, ip:%s, port:%s, errno:%d", ip.c_str(), port.c_str(), ret);
        return -1;
      }

      // insert again
      ret = mDBInstance->execSQL(insertSql.str().c_str());
      mDBInstance->freeResult();
    }
    else
    {
      // valid lock
      return 0;
    }
  }
  
  return ret == 0 ? 1 : -1;
}

void DtcDetectHandler::releaseDistributedLock(
    const std::string& ip,
    const std::string& port)
{
  // std::string ip = "192.168.7.26";
  // int port = 16877;
  
  // release the distributed lock avoid dead lock
  std::stringstream dSql;
  dSql << "delete from " << gTableName << " where ip =\"" << ip << "\" and port = " << port << ";";

  int ret = mDBInstance->execSQL(dSql.str().c_str());
  if (ret < 0)
  {
    monitor_log_error("release distributed lock failed. ip:%s, port:%s, errno:%d"\
        , ip.c_str(), port.c_str(), ret);
  }
  
  // maybe has some issue, fix it later
  return;
}
