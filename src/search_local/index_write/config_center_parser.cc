#include "config_center_parser.h"
#include "json/json.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <errno.h>

std::string ParserBase::GetlocalClusterPath()
{
    std::stringstream sTemp;
    sTemp << m_oConfigContext.iCaPid;
    std::string sVaildDir = m_oConfigContext.sCaDir + sTemp.str();

    if(access(sVaildDir.c_str() , F_OK) != 0)
    {
        log_info("Path:%s is not existing.",sVaildDir.c_str());
        sVaildDir = (E_HOT_BACKUP_JSON_PARSER == m_iParserId) ? "../conf/localCluster.json" : "../conf/localCluster.xml";
    }
    
    return sVaildDir;
}

////***************JsonParser***********************
JsonParser::JsonParser()
    : ParserBase(E_HOT_BACKUP_JSON_PARSER)
    , m_oLocalIps()
{ 
    GetLocalIps();
}

bool JsonParser::ParseConfig(std::string path)
{
    Json::Reader reader;
    Json::Value oLocalClusterContext;

    m_oSearchCoreClusterContext.clear();
    m_oOriginalLocalClusterContext.clear();

    std::ifstream iStream(path.c_str());
    if (!iStream.is_open())
    {
        log_error("load %s failed.", path.c_str());
        return false;
    }

    if (reader.parse(iStream, oLocalClusterContext) 
        && oLocalClusterContext.isMember("MODULE"))
    {   
        Json::Value oServerSharding = oLocalClusterContext["MODULE"]["SERVERSHARDING"];
        std::string sTempShardingName = "";
        for (int i = 0; i < (int)oServerSharding.size(); i++)
        {
            if (oServerSharding[i].isMember("ShardingName")
                 && oServerSharding[i]["ShardingName"].isString())
            {
                sTempShardingName = oServerSharding[i]["ShardingName"].asString();
            }
            else
            {
                log_error("ShardingName in incorrect");
                return false;
            }

            if (oServerSharding[i].isMember("INSTANCE")
                 && oServerSharding[i]["INSTANCE"].isArray())
            {
                Json::Value oInstance = oServerSharding[i]["INSTANCE"];
                for (int i = 0; i < (int)oInstance.size(); i++)
                {
                    ServerNode node;
                    node.sShardingName = sTempShardingName;
                    if (oInstance[i].isMember("ip")
                        && oInstance[i]["ip"].isString())
                    {
                        node.ip = oInstance[i]["ip"].asString();
                    }
                    else
                    {
                        log_error("ip is incorrect");
                        return false;
                    }

                    if (oInstance[i].isMember("role")
                        && oInstance[i]["role"].isString())
                    {
                        node.role = oInstance[i]["role"].asString();
                    }

                    if (node.role != INDEX_GEN_ROLE)
                    {
                        log_info("shardingName:%s,ip:%s, %s's serverNode Context  is useless here." 
                        , sTempShardingName.c_str()
                        , node.ip.c_str()
                        , node.role.c_str());
                        continue;
                    }

                    // DTC default port
                    node.port = "30311";

                    if ("" == node.ip || "" == node.port)
                    {
                        log_error("instance is incorrect");
                        return false;
                    }

                    if(oInstance[i].isMember("disasterRole")
                        && oInstance[i]["disasterRole"].isString())
                    {
                        node.sDisasterRole = oInstance[i]["disasterRole"].asString();
                    }
                    else
                    {
                        log_error("disasterRole is incorrect");
                        return false;
                    }

                    std::vector<ServerNode>::iterator iter = std::find(m_oSearchCoreClusterContext[node.ip].begin(),
                            m_oSearchCoreClusterContext[node.ip].end(), node);
                    if (iter != m_oSearchCoreClusterContext[node.ip].end())
                    {
                        log_error("instance %s:%s is repeated", node.ip.c_str(), node.port.c_str());
                        return false;
                    }
                    m_oSearchCoreClusterContext[node.ip].push_back(node);

                    iter = std::find(m_oOriginalLocalClusterContext[sTempShardingName].begin(),
                            m_oOriginalLocalClusterContext[sTempShardingName].end(), node);
                    if (iter != m_oOriginalLocalClusterContext[sTempShardingName].end())
                    {
                        log_error("instance %s:%s is repeated", node.ip.c_str(), node.port.c_str());
                        return false;
                    }

                    m_oOriginalLocalClusterContext[sTempShardingName].push_back(node);

                    std::vector<std::string >::iterator oIpIter = std::find(m_oLocalIps.begin() , m_oLocalIps.end() , node.ip);
                    if (oIpIter != m_oLocalIps.end())
                    {
                        m_bIsMaster = (DISASTER_ROLE_MASTER == node.sDisasterRole);
                        log_info("find local ip:%s success, master flag:%d.", node.ip.c_str() , (int)m_bIsMaster);
                    }
                }
            }
            else
            {
                log_error("INSTANCE in incorrect");
                return false;
            }
        }
        if (0 == m_oSearchCoreClusterContext.size()
        || 0 == m_oOriginalLocalClusterContext.size())
        {
            log_error("server is empty");
            return false;
        }
    }
    else
    {
        log_error("localCluster.json file format has some error, please check.");
        iStream.close();
        return false;
    }

#if DEBUG_LOG_ENABLE
    SearchCoreClusterCtxIter iter = m_oSearchCoreClusterContext.begin();
    for ( ; iter != m_oSearchCoreClusterContext.end(); ++iter)
    {
        std::vector<ServerNode>::iterator iTempIter = iter->second.begin();
        for (; iTempIter != iter->second.end(); ++iTempIter)
        {
            log_debug("Json-Parser Key(IP:%s) , Value:ShardingName:%s, ip:%s, port:%s, weight:%d, role:%s, disasterRole:%s" 
            , iter->first.c_str()
            , iTempIter->sShardingName.c_str()
            , iTempIter->ip.c_str()
            , iTempIter->port.c_str()
            , iTempIter->weight
            , iTempIter->role.c_str()
            , iTempIter->sDisasterRole.c_str());
        }
    }

    iter = m_oOriginalLocalClusterContext.begin();
    for ( ; iter != m_oOriginalLocalClusterContext.end(); ++iter)
    {
        std::vector<ServerNode>::iterator iTempIter = iter->second.begin();
        for (; iTempIter != iter->second.end(); ++iTempIter)
        {
            log_debug("Json-Parser Key(ShardingName:%s) , Value:IP:%s, port:%s, weight:%d, role:%s, disasterRole:%s" 
            , iter->first.c_str()
            , iTempIter->ip.c_str()
            , iTempIter->port.c_str()
            , iTempIter->weight
            , iTempIter->role.c_str()
            , iTempIter->sDisasterRole.c_str());
        }
    }

#endif

    return true;
}

bool JsonParser::ParseLocalConfig()
{
    Json::Reader reader;
    Json::Value oHotBackUpContext;

    std::string sPath = "../conf/index_gen.json";
    std::ifstream iStream(sPath.c_str());
    if (!iStream.is_open())
    {
        log_error("load %s failed.", sPath.c_str());
        return false;
    }

    if (reader.parse(iStream, oHotBackUpContext)
        && oHotBackUpContext.isMember("CONFIG"))
    {
        Json::Value oConfigContext = oHotBackUpContext["CONFIG"];
        if (oConfigContext.isMember("UpdateInterval")
            && oConfigContext["UpdateInterval"].isInt())
        {
            m_oConfigContext.iUpdateInterval = oConfigContext["UpdateInterval"].asInt();
        }
        else
        {
            log_error("updateInterval is incorrect");
            return false;
        }
        
        if (oConfigContext.isMember("CaDir")
            && oConfigContext["CaDir"].isString())
        {
            m_oConfigContext.sCaDir = oConfigContext["CaDir"].asString();
        }
        else
        {
            log_error("CaDir is incorrect");
            return false;
        }

        if (oConfigContext.isMember("CaPid")
            && oConfigContext["CaPid"].isInt())
        {
            m_oConfigContext.iCaPid = oConfigContext["CaPid"].asInt();
        }
        else
        {
            log_error("CaPid in incorrect");
            return false;
        }
        
        log_debug("Json-Parser UpdateInterval:%d \
         , CaDir:%s ,CaPid:%d", m_oConfigContext.iUpdateInterval 
         , m_oConfigContext.sCaDir.c_str() , m_oConfigContext.iCaPid);
    }
    else
    {
        log_error("hot_backup.json file format has some error, please check.");
        iStream.close();
        return false;
    }

    return true;
}

bool JsonParser::GetLocalIps()
{
    m_oLocalIps.clear();
    const char* shell = "/sbin/ifconfig -a|grep inet|grep -v 127.0.0.1 \
                    |grep -v inet6|awk '{print $2}'|tr -d \"addr:\"";
  
  FILE *fp;
  if ((fp = popen(shell, "r")) == NULL)
  {
    log_info("open the shell command failed.");
    return false;
  }
  
  // maybe has multiple network card
  char buf[1024];
  memset(buf , 0 , 1024 * sizeof(char));
  while (fgets(buf, 1023, fp) != NULL)
  {
    log_info("local ip:%s", buf);

    


    // remove the character '\n' from the tail of the buf because the system call
    // fgets will get it 
    std::string ip(buf);
    if (ip[ip.length() - 1] != '\n')
    {
      log_error("syntax error for fgets, ip:%s", ip.c_str());
      return false;
    }
    ip.erase(ip.begin() + (ip.length() - 1));
    log_info("local ip:%s", ip.c_str());
    m_oLocalIps.push_back(ip);
  }

  // the main function has ignored the signal SIGCHLD, so the recycle comand
  // SIGCHLD sent by child process of popen will be ignored, then pclose will
  // return -1 and set errno to ECHILD

  if (pclose(fp) == -1 && errno != ECHILD) 
  {
    log_info("close the file descriptor failed. errno:%d", errno);
    return false;
  }

  if (m_oLocalIps.empty())
  {
    log_error("get local ips failed.");
    return false;
  }

  return true;
}

////***************JsonParser***********************
ParserBase* const ConfigCenterParser::CreateInstance(int iParserId)
{
        ParserBase* pCurParser = new JsonParser();

        if(!pCurParser->ParseLocalConfig())
        {
            DELETE(pCurParser);
            pCurParser = NULL;
        }
        return pCurParser;
}

bool ConfigCenterParser::UpdateParser(ParserBase* const pParser)
{
    if(NULL == pParser) { return false;}
    
    std::string sVaildDir = pParser->GetlocalClusterPath();

    log_info("sCaDirPath:%s" ,sVaildDir.c_str());

    if(sVaildDir.empty() || !pParser->ParseConfig(sVaildDir))
    {
        log_error("UpdateParser error, please check");
        return false;
    }
    return true;
}

