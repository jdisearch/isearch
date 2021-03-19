/*
 * =====================================================================================
 *
 *       Filename:  service_instance.h
 *
 *    Description:  service_instance class definition.
 *
 *        Version:  1.0
 *        Created:  06/03/2017
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef _ACTIVESERVICE_INSTANCE_H_
#define _ACTIVESERVICE_INSTANCE_H_

#include "log.h"
#include "config.h"
#include "json/json.h"
#include <stdint.h>
using namespace std;

typedef struct  
{
	uint64_t pad1[7];
	uint64_t req_cnt;
	uint64_t active_cnt;
	uint64_t android_active_cnt;
	uint64_t ios_active_cnt;
	uint64_t repeat_active_cnt;
	uint64_t valid_active_cnt;
	uint64_t ad_click_cnt;
	uint64_t ad_distinct_cnt;
	uint64_t channel_callback_cnt;
	uint64_t security_callback_cnt;
	uint64_t rsp_cnt;
	uint64_t success_rsp_cnt;
	uint64_t sys_err_cnt;
	uint64_t net_err_cnt;
	uint64_t para_err_cnt;
	uint64_t sign_err_cnt;
	uint64_t data_err_cnt;
	uint64_t tcp_recv_cnt;
	uint64_t tcp_err_cnt;
	uint64_t tcp_success_cnt;
	uint64_t aver_req_time[18];
	uint64_t aver_find_his_time[18];
	uint64_t aver_store_his_time[18];
}CpaStat;

static const string filename = "/cpa.stat.10s";

class ServiceInstance 
{
public:
	ServiceInstance() :m_FileName(filename){}
	ServiceInstance(int port,string &appname,string &ip) : m_Port(port), m_FileName(filename),m_AppName(appname),m_Ip(ip){}
	ServiceInstance(const ServiceInstance &a) :m_Port(a.m_Port),m_Dir(a.m_Dir), m_FileName(filename),m_AppName(a.m_AppName),m_Ip(a.m_Ip) {}
	ServiceInstance &operator=(const ServiceInstance &a) {
		m_Port = a.m_Port;
		m_Dir = a.m_Dir;
		m_AppName = a.m_AppName;
		m_Ip = a.m_Ip;
		return *this;
	}
	bool Init();
	int GetPort() const { 
		return m_Port;
	}
	void StatsMap(Json::Value &packet);
private:
	void  EncodePacket(CpaStat *stat, Json::Value &packet);
private:
	int m_Port;
	string m_Dir;
	const string m_FileName;
	string m_AppName;
	string m_Ip;
};


	




#endif
