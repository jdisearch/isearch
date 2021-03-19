/*
 * =====================================================================================
 *
 *       Filename:  monitor.h
 *
 *    Description:  monitor class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef  __COMMON_MONITOR_H__
#define  __COMMON_MONITOR_H__

#include "roll_file_appender.h"
#include <sstream>
#include <pthread.h>
#include <queue>
#include <sys/time.h>
#include <json/json.h>
#include <stdint.h>
namespace common {

	class CallerInfo{
	public:
		friend class ProfilerMonitor;
	private:
		uint64_t	_BeginTimeStamp;
		std::string _Key;
	};

	class InfoItem {
	public:
		friend class ProfilerMonitor;
		InfoItem() :_Next(NULL), _Count(1) {}
		bool operator==(const InfoItem& a) {
			return this->_ElapsedTime == a._ElapsedTime &&
				this->_Key == a._Key &&
				this->_ProcessState == a._ProcessState &&
				this->_NowTimeStamp == a._NowTimeStamp;
		}
		std::string  ToProfilerItem();

	private:
		uint64_t _ElapsedTime;
		std::string _Key;
		std::string _ProcessState;
		time_t _NowTimeStamp;
		InfoItem *_Next;
		uint32_t _Count;
	
	};

	


	class ProfilerMonitor {
	public:

		static ProfilerMonitor& GetInstance() {
			static ProfilerMonitor instance;
			return instance;
		}

		bool Initialize();
		inline const  std::string& GetIp() const { return _LocalIpAddr; };
		CallerInfo RegisterInfo(const std::string& key);
	    void RegisterInfoEnd(CallerInfo& call_info);
	    void FunctionError(CallerInfo& call_info);

	private:
		pthread_t _ReportThread;
		pthread_cond_t _NotEmpty;
		pthread_mutex_t _Mutex;

		InfoItem *_InfoHead;
		InfoItem *_InfoTail;

		bool _StopFlag;
		bool _InitialzeSuccess;
		std::string _LocalIpAddr;
		RollingFileAppender _FileAppender;
	private:
		ProfilerMonitor() {
			pthread_mutex_init(&_Mutex, NULL);
			pthread_cond_init(&_NotEmpty, NULL);
			pthread_create(&_ReportThread, NULL, ProcessCycle, NULL);
			_StopFlag = false;
		}
		~ProfilerMonitor() {
			_StopFlag = true;
			pthread_cond_signal(&_NotEmpty);
			pthread_join(_ReportThread, NULL);
		}
		static void *ProcessCycle(void *arg);

		void Coalesce(InfoItem *head, std::vector<InfoItem>& temp_result);
		
		bool GetLocalIpAddr();
		
		void PushReportItem(InfoItem* item) {
			pthread_mutex_lock(&_Mutex);
			if (_InfoHead == NULL) {
				_InfoHead = _InfoTail = item;
			}
			else {
				_InfoTail->_Next = item;
				_InfoTail = item;
			}

			pthread_cond_signal(&_NotEmpty);
			pthread_mutex_unlock(&_Mutex);
		}
	};
};


#endif // ! __COMMON_MONITOR_H__
