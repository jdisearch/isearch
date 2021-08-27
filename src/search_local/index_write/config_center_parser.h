/////////////////////////////////////////////////////////////////
//
//   Parse config from config_center
//       created by chenyujie on Jan 4, 2021
////////////////////////////////////////////////////////////////
#ifndef CONFIG_CENTER_PARSER_H_
#define CONFIG_CENTER_PARSER_H_

#include <vector>
#include <map>
#include <string>
#include "MarkupSTL.h"
#include "log.h"
#include "singleton.h"
#include "memcheck.h"
#include <stdint.h>
#include "noncopyable.h"

#define INDEX_GEN_ROLE "index_gen"
#define SEARCH_ROLE "search"
#define DISASTER_ROLE_MASTER "master"
#define DISASTER_ROLE_REPLICATE "replicate"

#define DEBUG_LOG_ENABLE 1

struct ServerNode
{
    std::string ip;
    std::string port;
    int weight;
    std::string role;
    std::string sDisasterRole;
    std::string sShardingName;

    bool operator==(const ServerNode& node)
    {
      return (ip == node.ip) && (port == node.port) && (weight == node.weight) 
      && (role == node.role) && (sDisasterRole == node.sDisasterRole) && (sShardingName == node.sShardingName);
    }

    void operator()(
        std::string _sIP,
        std::string _sPort,
        int _iWeight,
        std::string _sRole,
        std::string _sDisasterRole,
        std::string _sShardingName
    )
    {
        ip = _sIP;
        port = _sPort;
        weight = _iWeight;
        role = _sRole;
        sDisasterRole = _sDisasterRole;
        sShardingName = _sShardingName;
    }
};

struct ConfigContext
{
    int iUpdateInterval;
    std::string sCaDir;
    int iCaPid;
};

enum E_SEARCH_MONITOR_PARSER_ID
{
    E_HOT_BACKUP_JSON_PARSER
};

typedef std::map<std::string, std::vector<ServerNode> > SearchCoreClusterContextType;
typedef SearchCoreClusterContextType::iterator SearchCoreClusterCtxIter;

class ParserBase
{
public:
    ParserBase(int iParserId)
    : m_oOriginalLocalClusterContext()
    , m_oSearchCoreClusterContext()
    , m_oConfigContext()
    , m_iParserId(iParserId)
    , m_bIsMaster(false)
    { };
    virtual ~ParserBase(){};

public:
    const SearchCoreClusterContextType& GetOriginalLocalClusterContext() const {return m_oOriginalLocalClusterContext;}
    const SearchCoreClusterContextType& GetSearchCoreClusterContext() const {return m_oSearchCoreClusterContext;}
    const ConfigContext& GetConfigContext() const { return m_oConfigContext;}
    int GetCurParserId() {return m_iParserId;}
    bool CheckLocalIpIsMaster() { return m_bIsMaster;}
    std::string GetlocalClusterPath();

public:
    virtual bool ParseConfig(std::string path) = 0;
    virtual bool ParseLocalConfig() = 0;

protected:
    SearchCoreClusterContextType m_oOriginalLocalClusterContext;
    SearchCoreClusterContextType m_oSearchCoreClusterContext;
    ConfigContext m_oConfigContext;
    int m_iParserId;
    bool m_bIsMaster;
};

class JsonParser : public ParserBase
{
public:
    JsonParser();
    virtual ~JsonParser(){};

public:
    virtual bool ParseConfig(std::string path);
    virtual bool ParseLocalConfig();

private:
    bool GetLocalIps(void);

private:
    std::vector<std::string> m_oLocalIps;
};

/// ************************************************************
/// * Different type files parser instance factory class
/// ************************************************************
class ConfigCenterParser : private noncopyable
{
public:
    ConfigCenterParser(){ };
    virtual ~ConfigCenterParser(){ };

public:
    static ConfigCenterParser* Instance()
    {
        return CSingleton<ConfigCenterParser>::Instance();
    };

    static void Destroy()
    {
        CSingleton<ConfigCenterParser>::Destroy();
    };

public:
    ParserBase* const CreateInstance(int iParserId = E_HOT_BACKUP_JSON_PARSER);
    bool UpdateParser(ParserBase* const pParser);
};

#endif
