///////////////////////////////////////////////////
//
// the foundation class for agent and DTC detector
//     create by qiuyu on Nov 26, 2018
//
//////////////////////////////////////////////////

#ifndef __DETECT_HANDLER_BASE_H__
#define __DETECT_HANDLER_BASE_H__

#include "DtcMonitorConfigMgr.h"
#include "DetectUtil.h"
// #include "InvokeMgr.h"
#include "curl_http.h"
#include "poll_thread.h"
#include "log.h"
#include "LirsCache.h"

#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <bitset>

#define REGION_CODE_SHIFT 10
#define REGION_CODE_MASK (~(-1LL << REGION_CODE_SHIFT))

class DBInstance;

class DetectHandlerBase : public CTimerObject
{
private:
  static const int sExpiredTimeoffset = 60 * 1000 * 1000; // 60s
  static const int64_t sCacheExpiredInterval = 1LL * 24 * 3600 * 1000 * 1000; // one day 

protected:
  static const int sEventTimerOffset = 300; // ms [-300, 300]

public:
  enum DetectType
  {
    eAgentDetect = 1,
    eDtcDetect
  };

  enum ReportType
  {
    eAgentDetectFailed = 0,
    eOfflineAgentFailed = 1,
    eOfflineAgentSuccess = 2,
    eDtcDetectFailed,
    eSwitchDtcFailed,
    eSwitchDtcSuccess,
    eAccessDBFailed,
    eAccessCCFailed,
    eSwitchSlaveDtc,
    eReplicateDtcFailed,
    eNoneUsefulDtc,
 
    eParseJsonResponseFailed = 7000,
    eResponseMissDefaultField,
    eInvalidReportType = -1
  };

  enum ResponseCode
  {
    eROperateSuccess = 200,
    eRSwitchSlaveDtc = 201
  };
  
  enum ZoneCode
  {
    eMinZoneCode = 0,
    
    // demestic region
    eLF1ZoneCode = 3, // LF1
    eMJQZoneCode = 6, // MJQ data center
    eYNZoneCode = 7, // YN
    eMJQ3ZoneCode = 11, // MJQ3
    eLF3ZoneCode = 12, // LF3
    eHKTHZoneCode = 13, // THA
    eGZ1ZoneCode = 14, // GZ1
    eLF4ZoneCode = 15, // LF4
    eLF5ZoneCode = 17, // LF4
    eKEPLERZoneCode = 21, // kepler
    eHT3ZoneCode = 28, // HT3
    
    eMaxZoneCode
  };

  typedef struct InstanceInfo
  {
    std::string sAccessKey; // used by IP
    std::string sIpWithPort;
    int64_t sExpiredTimestamp;
    
    InstanceInfo(
        const std::string& key, 
        const std::string& addr,
        const int64_t expiredWhen)
    :
    sAccessKey(key),
    sIpWithPort(addr),
    sExpiredTimestamp(expiredWhen + sExpiredTimeoffset)
    {
    }
    
    bool isExpired(const int64_t nowTimestamp) const
    {
      return sExpiredTimestamp <= nowTimestamp;
    }

    bool operator < (const InstanceInfo& other) const
    {
      if ((sAccessKey != "" && other.sAccessKey != "") 
        && (sAccessKey != other.sAccessKey))
      {
        return sAccessKey < other.sAccessKey;
      }
      return sIpWithPort < other.sIpWithPort;
    }
  }InstanceInfo_t;

  struct ProtocalHeader {
    uint32_t magic;
    uint32_t length;
    uint32_t cmd;
  };

protected:
  CPollThread*  mDetectPoll;
  std::string mReportUrl;
  int mDriverTimeout;   // ms
  int mDetectStep;
  int mPrevDetectIndex;
  std::set<InstanceInfo_t> mTroubleInstances;
  std::set<std::string> m_TroubleHandledIpSet;

  // db instance
  static DBInstance* mDBInstance;
  
  // region control relevant
  static std::string mGetPhysicalInfoUrl;
  static std::string mSelfAddr;
  static LirsCache* mPhysicalRegionCode; // mapping physicalId to region code
  static std::bitset<DetectHandlerBase::eMaxZoneCode> mRegionCodeSet; // region codes
  static int64_t mCacheExpiredWhen;

  // statistic
  std::stringstream mDetectSummary;

public:
  DetectHandlerBase(CPollThread* poll) 
  { 
    mDetectPoll = poll;
    mCacheExpiredWhen = GET_TIMESTAMP() + sCacheExpiredInterval; 
    mDetectSummary.str(""); // clear streaming
    srand(time(NULL));
  }

  virtual ~DetectHandlerBase(); 
  virtual void addTimerEvent();
  virtual void TimerNotify(void);
  void reportAlarm(const std::string& errMessage);

  static int getInstanceTimeout(
      const DetectHandlerBase::DetectType type,
      const std::string& detectorAddr,
      const std::string& nodeAddr);

  template<typename T>
  static std::string toString(const T& val)
  {
    std::stringstream ss;
    ss << val;
    return ss.str();
  }

protected:
  static int selectTimeoutWithZone(
    const DetectHandlerBase::DetectType type, 
    const std::string& selfZoneCode,
    const std::string& peerZoneCode);

  static void getNodeInfo(
    const std::string& phyAddr,
    std::string& zoneCode);

  static void stringSplit(
      const std::string& src, 
      const std::string& delim, 
      std::vector<std::string>& dst);

  static bool initHandler();
  static void registerRegionCode();
  static bool isNodeExpired(const std::string& nodeInfo);
};
#endif // __DETECT_HANDLER_BASE_H__
