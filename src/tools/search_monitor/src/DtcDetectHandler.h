///////////////////////////////////////////////////
//
// the dtc detector
//     create by qiuyu on Nov 27, 2018
//     Modify: chenyujie
//////////////////////////////////////////////////

#ifndef __DTC_DETECT_HANDLER_H__ 
#define __DTC_DETECT_HANDLER_H__ 

#include <vector>
#include <string>
#include "DetectHandlerBase.h"
#include "config_center_parser.h"

#define DISTRIBUTE_LOCK_SOLE_IP "127.0.0.1"
#define DISTRIBUTE_LOCK_SOLE_PORT "8888"

class DtcDetectHandler : public DetectHandlerBase 
{
	enum RetCode {
		RT_INIT_ERR = 10001,
		RT_PARSE_CONF_ERR,
		RT_PARSE_JSON_ERR,
		RT_PRE_RUN_ERR,
		RT_DB_ERR,
		RT_HTTP_ERR,
	};

private:
  static int64_t adminInfoIdx;

private:
  DtcClusterContextType mDtcList;
  SearchCoreClusterContextType mDtcMap;
  int mPrevDetectedIndex;
  DtcClusterContextType mCheckDtcList;
  ParserBase* m_pCurrentParser;
  bool m_bReportEnable;

public:
  DtcDetectHandler(CPollThread* poll);
  ~DtcDetectHandler();
  
  virtual void addTimerEvent();
  virtual void TimerNotify(void);

private:
  void initHandler();

  bool batchDetect(
      const int startIndex,
      const int endIndex);

  bool procSingleDetect(
      const std::string& addr,
      bool& isAlive,
      int& errCode);

  bool broadCastConfirm(
      const std::string& addr,
      bool& needSwitch);

  bool reportDtcAlarm(
      const DetectHandlerBase::ReportType type,
      const std::string& addr,
      const int errCode);

  bool doDtcSwitch(
      const std::string& sIP,
      const std::string& addr,
      const int errCode);

  void verifyAllExpired(const int line);

  int verifySingleExpired(const std::string& sIP, const std::string& addr);

  bool loadDtcClusterInfo();
  bool loadDtcClusterInfoNoOpr();
  bool loadSearchCoreClusterInfo();

  int Connect(const std::string& ip, const uint32_t port);

  char* MakeAdminPackage(const std::string& shardingName, 
    const std::string& ip, 
    const std::string& port, 
    int& msgsize);

  int NotifyRouteAdmin(const std::string& ip, 
    const uint32_t port, 
    const std::string& sharding, 
    const std::string& localip,
    const std::string& localport);

  int NotifyConfigCenter();

  bool CheckIndexGenIsMaster(const std::string& sIP);

  int getDistributedLockForConsistency(
      const std::string& ip,
      const std::string& port);

  void releaseDistributedLock(
      const std::string& ip,
      const std::string& port);
};
#endif // __DTC_DETECT_HANDLER_H__ 
