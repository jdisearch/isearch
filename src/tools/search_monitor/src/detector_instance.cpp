#include <stdlib.h>
#include <strings.h>
// #include <time.h>

#include <sstream>
// #include <set>

#include "curl_http.h"
// #include "detector_construct.h"
// #include "detector_constant.h"
#include "detector_instance.h"
#include "dtcapi.h"
#include "log.h"
#include "DtcMonitorConfigMgr.h"

static const int kTableMismatch = -2019;
static const int kBadRequest = -56;
static const int kServerError = -2033;
static const std::string kUnkownAdminCode = "unkown admin code";

bool CDetectorInstance::DetectAgent(
    const std::string& accessKey,
    const std::string& ipWithPort,
    const int timeout,
    bool& isAlive,
    int& errCode)
{
  DTC::Server dtc_server;
  dtc_server.StringKey();
  dtc_server.SetTableName("*");
  dtc_server.SetMTimeout(timeout);
  dtc_server.SetAccessKey(accessKey.c_str());
  dtc_server.SetAddress(ipWithPort.c_str());
#if 0
  int ping_ret = dtc_server.Ping();
  if(0 != ping_ret && kTableMismatch != ping_ret)
  {
    isAlive = false; 
    errCode = ping_ret;
    monitor_log_debug("ping error, ret [%d]", ping_ret);
  }
  else
  {
    isAlive = true;
  }
#else
  int ret = dtc_server.Connect();
  if(ret == -CDetectorInstance::eConnectTimeout || ret == -CDetectorInstance::eConnectRefused
      || ret == -CDetectorInstance::eHostUnreach)
  {
    isAlive = false; 
    errCode = ret;
    monitor_log_error("connect to agent error, ret [%d], addr:%s", ret, ipWithPort.c_str());
  }
  else if (ret != 0)
  {
    monitor_log_error("connect to agent error,not handle it now, must pay attention\
        to it, errno:%d, addr:%s", ret, ipWithPort.c_str());
  }
  else
  {
    isAlive = true;
  }
#endif

	return true;
}

bool CDetectorInstance::DetectDTC(
    const std::string& ipWithPort,
    const int timeout,
    bool& isAlive,
    int& errCode)
{
  isAlive = true;
  DTC::Server dtc_server;
  dtc_server.StringKey();
  dtc_server.SetTableName("*");
  dtc_server.SetMTimeout(timeout);
  dtc_server.SetAddress(ipWithPort.c_str());
  DTC::SvrAdminRequest request(&dtc_server);
  request.SetAdminCode(DTC::LogoutHB);
  DTC::Result result;
  int ret = request.Execute(result);
  if(0 != ret && kBadRequest != ret)
  {
    if(kServerError == ret && 
        0 == strcasecmp(kUnkownAdminCode.c_str(), result.ErrorMessage()))
    {
      isAlive = true;
    }
    else
    {
      errCode = ret;
      isAlive = false;
      monitor_log_error("request error, ret [%d], errcode [%d], errmsg [%s], errfrom [%s], addr:%s",\
          ret, result.ResultCode(), result.ErrorMessage(), result.ErrorFrom(), ipWithPort.c_str());
    }
  }

  return true;
}

CDetectorInstance::CDetectorInstance()
{
	//单位：毫秒(ms)，默认5000ms
	// m_get_route_timeout = 5000;
	// m_http_url_agent = "";
	// m_http_url_dtc = "";
}

CDetectorInstance::~CDetectorInstance()
{
	
}
