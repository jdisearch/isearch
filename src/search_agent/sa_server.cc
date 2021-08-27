#include "sa_server.h"
#include "log.h"
#include "sa_epoll.h"
#include "sa_util.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h> 
#include <unistd.h>
#include "sa_conn.h"
#include "sa_server_conn.h"
#include "sa_conn_pool.h"



CSearchServerInstance::CSearchServerInstance(const string &strAddr,
                                             uint32_t iPort,
                                             uint64_t iTimeout) : m_strAddr(strAddr),
                                                                  m_iPort(iPort),
                                                                  m_iTimeout(iTimeout)
{
}

void CSearchSeverSharding::ConfInstanceEachTransform(const vector<ConfInstance *> &arrConfInstance)
{
    log_debug("CSearchServerInstance::%s , arrConfInstance size %zu", __FUNCTION__, arrConfInstance.size());
    for (size_t i = 0; i < arrConfInstance.size(); i++)
    {
        CSearchServerInstance *pSearchServerInstance = new CSearchServerInstance(arrConfInstance[i]->strAddr,
                                                                                 arrConfInstance[i]->iPort,
                                                                                 arrConfInstance[i]->iTimeout);
	log_debug("initialize CSearchServerInstance %s:%d", arrConfInstance[i]->strAddr.c_str(), arrConfInstance[i]->iPort);
        if (arrConfInstance[i]->eRole == INDEX_WRITE)
        {
            m_arrIndexWriteInstanceList.push_back(pSearchServerInstance);
        }
        else
        {
            m_arrIndexReadInstanceList.push_back(pSearchServerInstance);
        }
    }
}

void CSearchSeverSharding::SetShardingName(const string &strShardingName)
{
    m_strShardingName = strShardingName;
}

int CServerPool::ConfShardingEachTransform(CSearchAgentConf *pSearchAgentConf)
{
    const vector<ConfSharding *> &arrConfShardingList = pSearchAgentConf->GetArrConfShardingList();
    for (size_t i = 0; i < arrConfShardingList.size(); i++)
    {
        CSearchSeverSharding *pSearchSharding = new CSearchSeverSharding();
        m_oConsistentHashSelector.AddNode(arrConfShardingList[i]->strShardingName.c_str());
        pSearchSharding->SetShardingName(arrConfShardingList[i]->strShardingName);
        pSearchSharding->ConfInstanceEachTransform(arrConfShardingList[i]->arrShardingInstanceList);
        m_arrSearchServerShardingList.push_back(pSearchSharding);
        m_mapSearchServerShardingMapper.insert(make_pair(arrConfShardingList[i]->strShardingName, pSearchSharding));
    }
    return 0;
}

int CServerPool::ServerPoolInit(CSearchAgentConf *pSearchAgentConf)
{
    log_debug("CSearchServerInstance::%s", __FUNCTION__);
    int status = ConfShardingEachTransform(pSearchAgentConf);
    if(status != 0)
    {
	return status;
    }
    return  ServerPoolPreConnect();
}

CServerPool::~CServerPool()
{
    log_debug("CSearchServerInstance::%s", __FUNCTION__);
    for (size_t i = 0; i < m_arrSearchServerShardingList.size(); i++)
    {
        delete m_arrSearchServerShardingList[i];
    }
    m_arrSearchServerShardingList.clear();
}

CSearchSeverSharding::~CSearchSeverSharding()
{
    log_debug("CSearchServerInstance::%s", __FUNCTION__);
    for (size_t i = 0; i < m_arrIndexWriteInstanceList.size(); i++)
    {
        delete m_arrIndexWriteInstanceList[i];
    }
    for (size_t i = 0; i < m_arrIndexReadInstanceList.size(); i++)
    {
        delete m_arrIndexReadInstanceList[i];
    }
    m_arrIndexReadInstanceList.clear();
    m_arrIndexWriteInstanceList.clear();
}

int CServerPool::ServerPoolPreConnect()
{
    log_debug("CSearchServerInstance::%s", __FUNCTION__);
    for (size_t i = 0; i < m_arrSearchServerShardingList.size(); i++)
    {
        int status = m_arrSearchServerShardingList[i]->ServerEachPreConnect();
        if (status < 0)
        {
            return status;
        }
    }
    return 0;
}

int CSearchSeverSharding::ServerEachPreConnect()
{
    log_debug("CSearchServerInstance::%s", __FUNCTION__);

    for (size_t i = 0; i < m_arrIndexReadInstanceList.size(); i++)
    {
        ConnectionInterface *pConnection = CSearchAgentConnectionPool::Instance()->GetInstanceConnection(m_arrIndexReadInstanceList[i]);
        if (pConnection == NULL)
        {
            log_error("get connection error");
            return -1;
        }
        int status = m_arrIndexReadInstanceList[i]->InstanceConnect(pConnection);
        if (status < 0)
        {
            return status;
        }
    }
    for (size_t i = 0; i < m_arrIndexWriteInstanceList.size(); i++)
    {
        ConnectionInterface *pConnection = CSearchAgentConnectionPool::Instance()->GetInstanceConnection(m_arrIndexWriteInstanceList[i]);
        if (pConnection == NULL)
        {
            log_error("get connection error");
            return -1;
        }
        int status = m_arrIndexWriteInstanceList[i]->InstanceConnect(pConnection);
        if (status < 0)
        {
            return status;
        }
    }
    return 0;
}

int CSearchServerInstance::InstanceConnect(ConnectionInterface *pConnection)
{
    
    log_debug("CSearchServerInstance::%s", __FUNCTION__);
    CInstanceConn* intanceConnection = static_cast<CInstanceConn *>(pConnection);
    int status = intanceConnection->InstanceConnect();
    if(status != 0 ){
        log_error("instance connection error %s:%d", m_strAddr.c_str(), m_iPort);
        return status;
    }
    CEventBase::Instance()->EventAddConnection(pConnection);
   // pConnection->m_bIsConnected = true;
    m_pAgentConnection = pConnection;
    return 0;
}

ConnectionInterface* CServerPool::GetWriteConnection(const string& strDocId)
{
    log_debug("CSearchServerInstance::%s", __FUNCTION__);
    log_debug("Get one index write connection to forward, doc_id %s", strDocId.c_str());
    const string strShardingName = m_oConsistentHashSelector.Select(strDocId.c_str(), strDocId.length());
    map<string, CSearchSeverSharding* >::const_iterator iter = m_mapSearchServerShardingMapper.find(strShardingName);
    return iter->second->GetOneWriteInstance()->GetOwnerConnection();
}


CSearchServerInstance* CSearchSeverSharding::GetOneWriteInstance()
{
    log_debug("CSearchServerInstance::%s", __FUNCTION__);
    return m_arrIndexWriteInstanceList[0];
}

CSearchServerInstance* CSearchSeverSharding::GetOneReadInstance()
{
    log_debug("CSearchServerInstance::%s", __FUNCTION__);
    m_iReadIndex++;
    if(m_iReadIndex == m_arrIndexReadInstanceList.size()){
            m_iReadIndex = 0 ;
    }
    return m_arrIndexReadInstanceList[m_iReadIndex];
}

 vector<ConnectionInterface *> CServerPool::GetAllShardingConnections()
 {
    log_debug("CSearchServerInstance::%s", __FUNCTION__);
     vector<ConnectionInterface *> oAllShardingConnections;
      map<string, CSearchSeverSharding* >::const_iterator iter = m_mapSearchServerShardingMapper.begin();
     for(; iter != m_mapSearchServerShardingMapper.end() ; iter++ )
     {
         oAllShardingConnections.push_back(iter->second->GetOneReadInstance()->GetOwnerConnection());
     }
     return oAllShardingConnections;
 }


ConnectionInterface* CSearchServerInstance::GetOwnerConnection() 
{
    log_debug("CSearchServerInstance::%s", __FUNCTION__);
    ConnectionInterface *pConnection = NULL;
    if( NULL == m_pAgentConnection)
    {
        pConnection = CSearchAgentConnectionPool::Instance()->GetInstanceConnection(this);
         if(pConnection == NULL){
             return NULL;
         }
    } 
    else{
    	return m_pAgentConnection;
    }
	
    int status = InstanceConnect(pConnection);
    if(status < 0 )
    {
        log_error("instance connection error");
        delete pConnection;
        return NULL;
    }
    m_pAgentConnection = pConnection;
    return pConnection;
}

void CSearchServerInstance::SetOwnerConnection(ConnectionInterface *pConnection)
{
    m_pAgentConnection = pConnection;
}
