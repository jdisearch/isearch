/*
* service_instance.cc
*
*  Created on : 2017.3.6
*  Author : qiulu
*/
#include "service_instance.h"
#include "sstream"
#include "dirent.h"
#include "json/value.h"
#include "json/writer.h"
#include <sys/mman.h>

using namespace std;


const char *active_root = "/export/servers/active_service/";

bool ServiceInstance::Init()
{
	ostringstream integer_oss;
	integer_oss << m_Port;
	m_Dir = active_root + integer_oss.str() + "/stats";
	DIR* dir;
	dir = opendir(m_Dir.c_str());
	if (dir == NULL) {
		log_error("can not open directory [%s]", m_Dir.c_str());
		return false;
	}
	closedir(dir);
	return true;
}

void ServiceInstance::StatsMap(Json::Value &packet)
{
	string file = m_Dir + m_FileName;
	string body_str = "";
	int fd = open(file.c_str(), O_RDONLY, 0);
	void *statmap = NULL;
	struct stat st;
	if (fd > 0){
		fstat(fd, &st);
		statmap = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	}
	CpaStat *stat;
	if (NULL != statmap) {
		stat = (CpaStat *)statmap;
		EncodePacket(stat, packet);
		munmap(statmap, st.st_size);
	}
	if(fd > 0)
		close(fd);

}


void ServiceInstance::EncodePacket(CpaStat *stat,Json::Value &packet)
{
	//Json::Value packet;
	//Json::FastWriter wr;
	string body_str;
	packet["port"] = static_cast<Json::Value::Int64>(m_Port);
	packet["appname"] = m_AppName;
	packet["ip"] = m_Ip;
	packet["req_cnt"] = static_cast<Json::Value::Int64>(stat->req_cnt);
	packet["active_cnt"] = static_cast<Json::Value::Int64>(stat->active_cnt);
	packet["android_active_cnt"] = static_cast<Json::Value::Int64>(stat->android_active_cnt);
	packet["ios_active_cnt"] = static_cast<Json::Value::Int64>(stat->ios_active_cnt);
	double aver_req_time = (0 != stat->aver_req_time[1]) ? 
		((double)stat->aver_req_time[0] / (double)stat->aver_req_time[1]) : 0;
	packet["aver_req_time"] = aver_req_time;
	packet["sys_err_cnt"] = static_cast<Json::Value::Int64>(stat->sys_err_cnt);
	packet["net_err_cnt"] = static_cast<Json::Value::Int64>(stat->net_err_cnt);
	packet["para_err_cnt"] = static_cast<Json::Value::Int64>(stat->para_err_cnt);
	packet["sign_err_cnt"] = static_cast<Json::Value::Int64>(stat->sign_err_cnt);
	packet["valid_active_cnt"] = static_cast<Json::Value::Int64>(stat->valid_active_cnt);
	//body_str = wr.write(packet);
	//return body_str;
}