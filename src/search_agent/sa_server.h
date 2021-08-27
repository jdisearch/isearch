#ifndef __SA_SERVER_H__
#define __SA_SERVER_H__
#include "consistent_hash_selector.h"
#include "sa_conn.h"
#include <vector>
#include <map>
#include "sa_conf.h"
using namespace std;


class CSearchServerInstance
{
public:
    CSearchServerInstance(const string& strAddr, 
                          uint32_t iPort,
                          uint64_t iTimeout);
    int InstanceConnect(ConnectionInterface* pConnection);
    const string& GetIpAddress() {return m_strAddr;}
    uint32_t GetPort() {return m_iPort;}
    ConnectionInterface* GetOwnerConnection();
    void SetOwnerConnection(ConnectionInterface *pConnection);
    uint64_t GetTimeout() {return m_iTimeout;};
private:
    string m_strAddr;
    uint32_t m_iPort;
    uint64_t m_iTimeout;
    ConnectionInterface *m_pAgentConnection;
    
};

class CSearchSeverSharding
{
public:
    CSearchSeverSharding(){m_iReadIndex = 0;}    
    void SetShardingName(const string &strShardingName);
    void ConfInstanceEachTransform(const vector<ConfInstance *> &arrConfInstance);
    int ServerEachPreConnect();
    CSearchServerInstance* GetOneWriteInstance();
    CSearchServerInstance* GetOneReadInstance();
    ~CSearchSeverSharding();
private:
    string m_strShardingName;
    vector<CSearchServerInstance *>  m_arrIndexWriteInstanceList;
    vector<CSearchServerInstance *>  m_arrIndexReadInstanceList;
    size_t m_iReadIndex ;

};

class CServerPool
{
public:
    int ServerPoolInit(CSearchAgentConf *pSearchAgentConf);
    ConnectionInterface* GetWriteConnection(const string& strDocId);
    vector<ConnectionInterface  *> GetAllShardingConnections();
    ~CServerPool();
private:
    map<string, CSearchSeverSharding*> m_mapSearchServerShardingMapper;
    vector<CSearchSeverSharding *> m_arrSearchServerShardingList;
    CConsistentHashSelector m_oConsistentHashSelector;
private:
    int ServerEachPreconnect();
    int ConfShardingEachTransform(CSearchAgentConf *pSearchAgentConf);
    int ServerPoolPreConnect();
};

#endif
