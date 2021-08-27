#ifndef __SA_CONN_POOL_H__
#define __SA_CONN_POOL_H__

#include <map>
#include "singleton.h"
#include <stdint.h>

class ConnectionInterface;
class CSearchAgentConnectionPool
{
    public:
        static CSearchAgentConnectionPool *Instance()
	    {
		    return CSingleton<CSearchAgentConnectionPool>::Instance();
	    }
        static void Destroy()
	    {
		    CSingleton<CSearchAgentConnectionPool>::Destroy();
	    }
        CSearchAgentConnectionPool();
        ConnectionInterface* GetListener(void *argv);
        ConnectionInterface *GetInstanceConnection(void *argv);
        ConnectionInterface *GetClientConnection(void *argv);
        void ReclaimConnection(uint32_t iConnectionId);   
        ConnectionInterface* GetConnectionById(uint32_t iConnectionId);
    private:
        uint32_t m_iConnectionId;
        std::map<uint32_t, ConnectionInterface*> m_mapConnectionPool;
        private:
        uint32_t GetCurrentConnectionId();

};

#endif

