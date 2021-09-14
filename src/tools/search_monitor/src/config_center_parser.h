/////////////////////////////////////////////////////////////////
//
//   Parse config from config_center
//       created by chenyujie on Dec 14, 2020
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

enum E_SEARCH_MONITOR_PARSER_ID
{
    E_SEARCH_MONITOR_XML_PARSER = 0,
    E_SEARCH_MONITOR_JSON_PARSER
};

typedef std::map<std::string, std::vector<ServerNode> > SearchCoreClusterContextType;
typedef SearchCoreClusterContextType::iterator SearchCoreClusterCtxIter;

typedef std::vector<std::pair<std::string, std::string> > DtcClusterContextType;
typedef DtcClusterContextType::iterator DtcClusterCtxIter;

class ParserBase
{
public:
    ParserBase(int iParserId)
    : m_oDtcClusterContext()
    , m_oSearchCoreClusterContext()
    , m_iParserId(iParserId)
    { };
    virtual ~ParserBase(){};

public:
    const SearchCoreClusterContextType& GetSearchCoreClusterContext() const {return m_oSearchCoreClusterContext;}
    const DtcClusterContextType& GetDtcClusterContext() const { return m_oDtcClusterContext;}
    int GetCurParserId() {return m_iParserId;}

public:
    virtual bool ParseConfig(std::string path) = 0;

protected:
    DtcClusterContextType m_oDtcClusterContext;
    SearchCoreClusterContextType m_oSearchCoreClusterContext;
    int m_iParserId;
};

class XmlParser : public ParserBase
{
public:
    XmlParser();
    virtual ~XmlParser(){};

public:
    virtual bool ParseConfig(std::string path);

private:
    bool ParseConfigCenterConfig(const char *buf, int len);
};

class JsonParser : public ParserBase
{
public:
    JsonParser();
    virtual ~JsonParser(){};

public:
    virtual bool ParseConfig(std::string path);
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
    ParserBase* const CreateInstance(int iParserId = E_SEARCH_MONITOR_JSON_PARSER);
    bool UpdateParser(ParserBase* const pParser);
    bool CheckConfigModifyOrNot(long iStartTime);
    long GetConfigCurrentTime(std::string sPath);
};

#endif
