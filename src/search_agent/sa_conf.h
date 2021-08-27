#ifndef __SA_CONF_H__
#define __SA_CONF_H__
#include <fstream>
#include <vector>
#include "json/json.h"

class CSearchSeverSharding;
typedef enum
{
    INDEX_READ,
    INDEX_WRITE
} InstanceRole;

typedef struct stConfInstance
{
    std::string strAddr;
    int iPort;
    InstanceRole eRole;
    int iTimeout;
} ConfInstance;

typedef struct stConfSharding
{
    std::string strShardingName;
    std::vector<ConfInstance *> arrShardingInstanceList;
    ~stConfSharding()
    {
        for(size_t i = 0 ; i < arrShardingInstanceList.size() ;i++){
            delete arrShardingInstanceList[i];
        }
        arrShardingInstanceList.clear();
    }
} ConfSharding;

class CSearchAgentConf
{
public:
    int ConfCreate(char *filename);
    int ConfOpen(char *filename);
    int ConfParse();
    void ConfDump();
    int ServerInit(std::vector<CSearchSeverSharding *>& pServerList);
    const std::vector<ConfSharding *>& GetArrConfShardingList() {return m_arrConfShardingList;};
    ~CSearchAgentConf()
    {
        for(size_t i = 0 ; i < m_arrConfShardingList.size() ;i++)
        {
            delete m_arrConfShardingList[i];
        }
        m_arrConfShardingList.clear();
    }
private:
    Json::Value m_jsonConfigServers;
    std::vector<ConfSharding *> m_arrConfShardingList;
private:
    ConfSharding* ShardingConfParse(const Json::Value& jsonShardingConfig);
    ConfInstance* InstanceConfParse(const Json::Value& jsonInstanceConfig);
};

#endif
