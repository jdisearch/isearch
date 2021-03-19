#include <stdio.h>
#include <sys/types.h>
#include <sstream>
#include <stdlib.h>  
#include <string.h>
#include <netdb.h>  
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "monitor.h"
#include "json/json.h"
#include "log.h"

namespace common {

	const int statistic_period = 5;

	void * ProfilerMonitor::ProcessCycle(void * arg)
	{
		pthread_mutex_lock(&ProfilerMonitor::GetInstance()._Mutex);
		std::vector<InfoItem> temp_result;
		int last_append_time = time(NULL);
		while (!ProfilerMonitor::GetInstance()._StopFlag) {
			if (ProfilerMonitor::GetInstance()._InfoHead == NULL) {
				pthread_cond_wait(&ProfilerMonitor::GetInstance()._NotEmpty, &ProfilerMonitor::GetInstance()._Mutex);
				continue;
			}
			InfoItem *head = ProfilerMonitor::GetInstance()._InfoHead;
			ProfilerMonitor::GetInstance()._InfoHead = ProfilerMonitor::GetInstance()._InfoTail = NULL;
			pthread_mutex_unlock(&ProfilerMonitor::GetInstance()._Mutex);
			
			ProfilerMonitor::GetInstance().Coalesce(head, temp_result);
			int now_time = time(NULL);
			if (now_time - last_append_time >= statistic_period) {
				last_append_time = now_time;
				std::stringstream report_info;
				for (std::vector<InfoItem>::iterator it = temp_result.begin(); it != temp_result.end(); it++) {
					report_info << it->ToProfilerItem() << "\n";
				}
				temp_result.clear();
				ProfilerMonitor::GetInstance()._FileAppender.DoAppend(report_info.str());
			}
		
			pthread_mutex_lock(&ProfilerMonitor::GetInstance()._Mutex);

		}


		return NULL;
	}

	void  ProfilerMonitor::Coalesce(InfoItem * head, std::vector<InfoItem>& temp_result)
	{
		InfoItem *p = head;
		InfoItem *q;
		while (p != NULL) {

			std::vector<InfoItem>::iterator it = temp_result.begin();	
			for ( ; it != temp_result.end(); it++) {
				if (*it == *p) {
					it->_Count++;
					break;
				}
			}
			if (temp_result.size() == 0  || it == temp_result.end()) {
				temp_result.push_back(*p);
			}

			q = p; 
			p = p->_Next;
			delete q;		
		}

	}

	bool ProfilerMonitor::GetLocalIpAddr(){
		char hname[128];
		gethostname(hname, sizeof(hname));
		struct hostent *host;
		host = gethostbyname(hname);
	    _LocalIpAddr = inet_ntoa(*(struct in_addr*)(host->h_addr_list[0]));
		return true;
	}



	bool ProfilerMonitor::Initialize(){

		_InfoHead = _InfoTail = NULL;
		_InitialzeSuccess =  GetLocalIpAddr() && _FileAppender.Initialize();
		return _InitialzeSuccess;
	}

	CallerInfo ProfilerMonitor::RegisterInfo(const std::string& key) {
		CallerInfo caller_info;
		if(!_InitialzeSuccess) return caller_info;
		struct timeval tv_begin;
		gettimeofday(&tv_begin, NULL);
		caller_info._Key = key;
		caller_info._BeginTimeStamp = tv_begin.tv_sec * 1000 + tv_begin.tv_usec / 1000;
		return caller_info;
	}

	void ProfilerMonitor::RegisterInfoEnd(CallerInfo& call_info) {
		  if(!_InitialzeSuccess) return ;
		 struct timeval tv_begin;
		 gettimeofday(&tv_begin, NULL);
		 InfoItem *item =  new InfoItem();
		 if (item != NULL) {
			 uint64_t end_timestamp = tv_begin.tv_sec * 1000 + tv_begin.tv_usec / 1000;
			 item->_Key = call_info._Key;
			 item->_ElapsedTime = end_timestamp - call_info._BeginTimeStamp;
			 item->_ProcessState = "0";
			 item->_NowTimeStamp = time(NULL) ;
			 PushReportItem(item);
		 }
	 }

	 void ProfilerMonitor::FunctionError(CallerInfo& call_info) {
		 if(!_InitialzeSuccess) return ;
		 InfoItem *item = new InfoItem();
		 if (item != NULL) {
			 item->_Key = call_info._Key;
			 item->_ElapsedTime = -1;
			 item->_ProcessState = "1";
			 item->_NowTimeStamp = time(NULL) ;
			 PushReportItem(item);
		 }
	 }


	std::string InfoItem::ToProfilerItem(){
		 Json::Value info;
		 Json::FastWriter wr;
	 	 char now_time[64];
	 	 struct tm* p = localtime(&_NowTimeStamp);
		 strftime(now_time, 64, "%Y%m%d%H%M%S000", p);
		 info["time"] = now_time;
		 info["key"] = _Key;
		 info["hostname"] = ProfilerMonitor::GetInstance().GetIp();
		 info["processState"] = _ProcessState;
		 std::stringstream elapse_oss;
		 elapse_oss << _ElapsedTime;
		 info["elapsedTime"] = elapse_oss.str();
		 std::stringstream count_oss;
		 count_oss << _Count;
		 info["count"] = count_oss.str();
		 return wr.write(info);
	 }
};



#ifdef  DEBUG_PROFILER
int main() {
	_init_log_("test", "./");
	_set_log_level_(7);
	log_error("begin test");
	common::ProfilerMonitor::GetInstance().Initialize();
	
	log_error("iniialize success");
	while (true) {
		common::CallerInfo caller_info = common::ProfilerMonitor::GetInstance().RegisterInfo(std::string("userInfoExport"));
		sleep(1);
		common::ProfilerMonitor::GetInstance().RegisterInfoEnd(caller_info);
	}
}
#endif //  DDEBUG_PROFILER
