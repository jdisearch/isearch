#ifndef __CHC_CLI_SERVERS_H
#define __CHC_CLI_SERVERS_H

#include "dtcint.h"

extern "C"
{
#include "app_client_set.h"
}

#define DEFAULT_ROUTE_EXPIRE_TIME 120
#define DEFAULT_MAX_REMOVE_THRESHOLD_TIME 1800
#define DEFAULT_REMOVE_ERROR_COUNT 3
#define DEFAULT_ROUTE_INTERVAL_TIME 5
#define DEFAULT_SERVER_POS 0

namespace DTC {

	class DTCQosServer
	{
		public:
			DTCQosServer();
			~DTCQosServer(void);
		private:
			DTCQosServer(const DTCQosServer& qosServer);
			DTCQosServer& operator=(const DTCQosServer& qosServer);

		public:
			int GetStatus()
			{
				return this->m_Status;
			}
			void SetStatus(int iFlag)
			{
				this->m_Status = iFlag;
			}

			int GetWeight()
			{
				return this->m_Weight;
			}
			void setWeight(int iFlag)
			{
				this->m_Weight = iFlag;
			}

			uint64_t GetRemoveTimeStamp()
			{
				return this->m_RemoveTimeStamp;
			}	
			void SetRemoveTimeStamp(uint64_t iFlag)
			{
				this->m_RemoveTimeStamp = iFlag;
			}

			uint64_t GetLastRemoveTime()
			{
				return this->m_LastRemoveTime;
			}	
			void SetLastRemoveTime(uint64_t iFlag)
			{
				this->m_LastRemoveTime = iFlag;
			}

			uint64_t GetMaxTimeStamp()
			{
				return this->m_MaxTimeStamp;
			}	
			void SetMaxTimeStamp(uint64_t iFlag)
			{
				this->m_MaxTimeStamp = iFlag;
			}

			Server* GetDTCServer()
			{
				return this->m_Server;
			}

			void SetDTCServer(Server* ser)
			{
				this->m_Server = ser;
			}

			void ResetDTCServer()
			{
				this->m_Server = NULL;
			}

			void CreateDTCServer()
			{
				this->m_Server = new Server();
			}

		public:
			friend class DTCServers;

		private:
			int m_Status;
			int m_Weight;
			uint64_t m_RemoveTimeStamp;//需要被摘除的时间
			uint64_t m_LastRemoveTime; //上次被移除的时间点
			uint64_t m_MaxTimeStamp;//最大指数阀值
			Server *m_Server;
	};

};

#endif 
