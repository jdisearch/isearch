//////////////////////////////////////////////////////////////
// Copyright (c) 2018, JD Inc. All Rights Reserved.
// Author:jinweiwei
// Date: 2018-12-28
//
// copy from jinweiwei newdetector directory
//      created by qiuyu on Nov 28, 2018
////////////////////////////////////////////////////////////

#ifndef __DETECTOR_INSTANCE_H__
#define __DETECTOR_INSTANCE_H__


#include <string>
#include <vector>

// #include "config.h"
// #include "singletonbase.h"


class CDetectorInstance //: public CSingletonBase<CDetectorInstance>
{
private:
  enum ErrCode
  {
    eConnectTimeout = 110,
    eConnectRefused = 111,
    eHostUnreach = 113,
  };

public:
	static bool DetectAgent(
      const std::string& accessKey,
      const std::string& ipWithPort,
      const int timeout,
      bool& isAlive,
      int& errCode);

  static bool DetectDTC(
      const std::string& ipWithPort,
      const int timeout,
      bool& isAlive,
      int& errCode);

private:
	CDetectorInstance();
	~CDetectorInstance();

private:
	// static std::string m_report_url; // website url
	// static int m_report_timeout;
	// static int m_report_length;
	// static int m_get_route_timeout;
	// static std::string m_http_url_agent;
	// static std::string m_http_url_dtc;
};


#endif
