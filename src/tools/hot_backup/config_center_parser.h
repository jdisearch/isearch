/*
 * =====================================================================================
 *
 *       Filename:  config_center_parser.h
 *
 *    Description:  config_center_parser class definition.
 *
 *        Version:  1.0
 *        Created:  04/01/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  chenyujie, chenyujie28@jd.com@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef CONFIG_CENTER_PARSER_H_
#define CONFIG_CENTER_PARSER_H_

#include <stdlib.h>
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

struct NetContext
{
    std::string m_sIp;
    int m_iPort;

    NetContext()
        : m_sIp("127.0.0.1")
        , m_iPort(11017)
    { }

    NetContext(std::string _sIp , std::string _sPort)
        : m_sIp(_sIp)
        , m_iPort(11017)
    {
        try
        {
            m_iPort = atoi(_sPort.c_str());
        }
        catch(...)
        {
            log_error("string to int has exception.");
        }
    }
    
    void operator()(std::string _sIp , std::string _sPort)
    {
        m_sIp = _sIp;
        try
        {
            m_iPort = atoi(_sPort.c_str());
        }
        catch(...)
        {
            m_iPort = 11017;
            log_error("string to int has exception.");
        }
    }
};

struct ServerNode
{
    std::string ip;
    std::string port;
    std::string keywordtablePort;
    std::string rocksDbReplPort;
    std::string originaltablePort;
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
        std::string _skeywordtablePort,
        std::string _srocksDbReplPort,
        std::string _soriginaltablePort,
        int _iWeight,
        std::string _sRole,
        std::string _sDisasterRole,
        std::string _sShardingName
    )
    {
        ip = _sIP;
        port = _sPort;
        keywordtablePort = _skeywordtablePort;
        rocksDbReplPort = _srocksDbReplPort;
        originaltablePort = _soriginaltablePort;
        weight = _iWeight;
        role = _sRole;
        sDisasterRole = _sDisasterRole;
        sShardingName = _sShardingName;
    }
};

struct ConfigContext
{
    int iLogLevel;
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
    , m_oPeerShardMasterNet()
    , m_iParserId(iParserId)
    , m_sLocalSingleIp("")
    , m_sLocalkeywordDtcPort("")
    , m_bIsMaster(false)
    { };
    virtual ~ParserBase(){};

public:
    const SearchCoreClusterContextType& GetOriginalLocalClusterContext() const {return m_oOriginalLocalClusterContext;}
    const SearchCoreClusterContextType& GetSearchCoreClusterContext() const {return m_oSearchCoreClusterContext;}
    const ConfigContext& GetConfigContext() const { return m_oConfigContext;}
    int GetCurParserId() {return m_iParserId;}
    bool CheckLocalIpIsMaster() { return m_bIsMaster;}
    std::string GetLocalDtcNet();
    const std::string& GetLocalkeywordDtcPort() { return m_sLocalkeywordDtcPort;}
    const std::string& GetLocalIndexGenIp() { return m_sLocalSingleIp;}
    std::string GetLocalIndexGenPort();
    std::string GetPeerShardMasterIp();
    std::string GetlocalClusterPath();
    const NetContext& GetPeerShardMasterNet() { return m_oPeerShardMasterNet;}

public:
    virtual bool ParseConfig(std::string path) = 0;
    virtual bool ParseLocalConfig() = 0;

protected:
    SearchCoreClusterContextType m_oOriginalLocalClusterContext;
    SearchCoreClusterContextType m_oSearchCoreClusterContext;
    ConfigContext m_oConfigContext;
    NetContext m_oPeerShardMasterNet;
    int m_iParserId;
    std::string m_sLocalSingleIp;
    std::string m_sLocalkeywordDtcPort;
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
