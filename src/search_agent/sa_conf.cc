#include "sa_conf.h"
#include "log.h"
#include "json/json.h"
#include <fstream>
#include <stdlib.h>

using namespace std;

#define OUTTER_TAG "Modules"
#define SERVER_SHARDINGS_TAG "SERVERSHARDING"
#define SHARDING_NAME_TAG "ShardingName"
#define INSTANCE_TAG "INSTANCE"
#define IP_TAG "ip"
#define PORT_TAG "port"
#define ROLE_TAG "role"
#define TIMEOUT_TAG "timeout"

int CSearchAgentConf::ConfOpen(char *filename)
{
    log_debug("CSearchAgentConf::%s", __FUNCTION__);
    Json::Reader configReader;
    ifstream file(filename);
    if (0 == configReader.parse(file, m_jsonConfigServers))
    {
        log_error("open conf %s error", filename);
        return -1;
    }
    return 0;
}

int CSearchAgentConf::ConfCreate(char *filename)
{
    log_debug("CSearchAgentConf::%s", __FUNCTION__);
    int ret = ConfOpen(filename);
    if (ret != 0)
    {
        log_error("open conf error");
        return ret;
    }
    ret = ConfParse();
    if (ret != 0)
    {
        return ret;
    }
    ConfDump();
    return 0;
}

int CSearchAgentConf::ConfParse()
{
    log_debug("CSearchAgentConf::%s", __FUNCTION__);
    if (m_jsonConfigServers.isMember(SERVER_SHARDINGS_TAG) && m_jsonConfigServers[SERVER_SHARDINGS_TAG].isArray())
    {
        Json::Value jsonShardingConfigs = m_jsonConfigServers[SERVER_SHARDINGS_TAG];
        for (unsigned  i = 0; i < jsonShardingConfigs.size(); i++)
        {
            ConfSharding *pConfSharding = ShardingConfParse(jsonShardingConfigs[i]);
            if (pConfSharding == NULL)
            {
                return -1;
            }
            m_arrConfShardingList.push_back(pConfSharding);
        }
    }
    return 0;
}

ConfSharding *CSearchAgentConf::ShardingConfParse(const Json::Value &jsonShardingConfig)
{
    log_debug("CSearchAgentConf::%s", __FUNCTION__);
    ConfSharding *pConfSharding = new ConfSharding();
    if (pConfSharding == NULL)
    {
        log_error("There is no memory for ConfSharding");
        return NULL;
    }
    if (jsonShardingConfig.isMember(SHARDING_NAME_TAG) && jsonShardingConfig[SHARDING_NAME_TAG].isString())
    {
        pConfSharding->strShardingName = jsonShardingConfig[SHARDING_NAME_TAG].asString();
        if (jsonShardingConfig.isMember(INSTANCE_TAG) && jsonShardingConfig[INSTANCE_TAG].isArray())
        {
            for (unsigned  i = 0; i < jsonShardingConfig[INSTANCE_TAG].size(); i++)
            {
                ConfInstance *pConfInstance = InstanceConfParse(jsonShardingConfig[INSTANCE_TAG][i]);
                if (pConfInstance == NULL)
                {
                    log_error("Instance configuration error");
                    delete pConfSharding;
                    return NULL;
                }
                else
                {
                    pConfSharding->arrShardingInstanceList.push_back(pConfInstance);
                }
            }
        }
    }
    else
    {
        delete pConfSharding;
        return NULL;
    }

    return pConfSharding;
}

ConfInstance *CSearchAgentConf::InstanceConfParse(const Json::Value &jsonInstanceConfig)
{
    log_debug("CSearchAgentConf::%s", __FUNCTION__);
    ConfInstance *pConfInstance = new ConfInstance();
    if (pConfInstance == NULL)
    {
        log_error("There is no memory for ConfInstance");
        return NULL;
    }

    if (jsonInstanceConfig.isMember(IP_TAG) && jsonInstanceConfig[IP_TAG].isString() &&
        jsonInstanceConfig.isMember(PORT_TAG) && jsonInstanceConfig[PORT_TAG].isString() )
    {
        pConfInstance->strAddr = jsonInstanceConfig[IP_TAG].asString();
        pConfInstance->iPort = atoi(jsonInstanceConfig[PORT_TAG].asCString());
    }
    else
    {
        delete pConfInstance;
        return NULL;
    }

    if (jsonInstanceConfig.isMember(ROLE_TAG) && jsonInstanceConfig[ROLE_TAG].isString() && jsonInstanceConfig[ROLE_TAG].asString() == "index_gen")
    {
        pConfInstance->eRole = INDEX_WRITE;
    }
    else
    {
        pConfInstance->eRole = INDEX_READ;
    }

    pConfInstance->iTimeout = 500;
    if (jsonInstanceConfig.isMember(TIMEOUT_TAG) && jsonInstanceConfig[TIMEOUT_TAG].isInt())
    {
        pConfInstance->iTimeout = jsonInstanceConfig[TIMEOUT_TAG].asInt();
    }
    return pConfInstance;
}

void CSearchAgentConf::ConfDump()
{
    log_debug("CSearchAgentConf::%s", __FUNCTION__);
    for (size_t i = 0; i < m_arrConfShardingList.size(); i++)
    {
        log_error("sharding %s %d", m_arrConfShardingList[i]->strShardingName.c_str(),
                  (int)m_arrConfShardingList[i]->arrShardingInstanceList.size());

        for (size_t j = 0; j < m_arrConfShardingList[i]->arrShardingInstanceList.size(); j++)
        {
            ConfInstance *instance = m_arrConfShardingList[i]->arrShardingInstanceList[j];
            log_error("\taddr %s:%d \t role %d", instance->strAddr.c_str(), instance->iPort, instance->eRole);
        }
    }
}
