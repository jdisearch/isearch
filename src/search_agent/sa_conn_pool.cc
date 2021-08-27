#include "sa_conn_pool.h"
#include "sa_conn.h"
#include "sa_listener.h"
#include "sa_server_conn.h"
#include "sa_client_conn.h"
#include "log.h"


#define MAX_CONNECTION_ID  2147483647U 

uint32_t CSearchAgentConnectionPool::GetCurrentConnectionId()
{
    m_iConnectionId++;;
    if(m_iConnectionId >= MAX_CONNECTION_ID)
    {
        m_iConnectionId = 0;
    }
    return m_iConnectionId;
}

CSearchAgentConnectionPool::CSearchAgentConnectionPool()
{
    m_iConnectionId = 0;
    m_mapConnectionPool.clear();
}

ConnectionInterface* CSearchAgentConnectionPool::GetListener(void *argv)
{
    log_debug("CSearchAgentConnectionPool::%s", __FUNCTION__);
    uint32_t iTempConnectionId = GetCurrentConnectionId();
    ConnectionInterface *pConnection = new CSearchListenerConnection(iTempConnectionId);
    if (pConnection == NULL)
    {
        return NULL;
    }
    
    pConnection->Reference(NULL);
    m_mapConnectionPool.insert(make_pair(iTempConnectionId, pConnection));
    return pConnection;
}



ConnectionInterface *CSearchAgentConnectionPool::GetInstanceConnection(void *argv)
{
    log_debug("CSearchAgentConnectionPool::%s", __FUNCTION__);
     uint32_t iTempConnectionId = GetCurrentConnectionId();
    ConnectionInterface *pConnection = new CInstanceConn(iTempConnectionId);
    if (pConnection == NULL)
    {
        return NULL;
    }
    pConnection->Reference(argv);

    m_mapConnectionPool.insert(make_pair(iTempConnectionId, pConnection));
    return pConnection;
}


ConnectionInterface *CSearchAgentConnectionPool::GetClientConnection(void *argv)
{
    log_debug("CSearchAgentConnectionPool::%s", __FUNCTION__);
    uint32_t iTempConnectionId = GetCurrentConnectionId();
    log_debug("ConnectionInterface::%s", __FUNCTION__);
    ConnectionInterface *pConnection = new CClientConn(iTempConnectionId);
    if (pConnection == NULL)
    {
        return NULL;
    }
    m_mapConnectionPool.insert(make_pair(iTempConnectionId, pConnection));

    return pConnection;
}


void CSearchAgentConnectionPool::ReclaimConnection(uint32_t iConnectionId)
{
    m_mapConnectionPool.erase(iConnectionId);
}


ConnectionInterface* CSearchAgentConnectionPool::GetConnectionById(uint32_t iConnectionId)
{
    ConnectionInterface* pConnection = NULL;
    if(m_mapConnectionPool.find(iConnectionId) == m_mapConnectionPool.end()){
        return pConnection;
    }
    return m_mapConnectionPool[iConnectionId];
}
