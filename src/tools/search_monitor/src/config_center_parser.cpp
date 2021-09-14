#include "config_center_parser.h"
#include "DtcMonitorConfigMgr.h"
#include "json/json.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>

XmlParser::XmlParser()
    : ParserBase(E_SEARCH_MONITOR_XML_PARSER)
{ }

bool XmlParser::ParseConfig(std::string path)
{
  bool bResult = false;
  m_oDtcClusterContext.clear();
  m_oSearchCoreClusterContext.clear();
  
  FILE* fp = fopen(path.c_str(), "rb" );
  if (fp == NULL)
  {
    monitor_log_error("load config center info failed.");
    return false;
  }

  // Determine file length
  fseek( fp, 0L, SEEK_END );
  int nFileLen = ftell(fp);
  fseek( fp, 0L, SEEK_SET );

  // Load string
  std::allocator<char> mem;
  std::allocator<char>::pointer pBuffer = mem.allocate(nFileLen+1, NULL);
  if ( fread( pBuffer, nFileLen, 1, fp ) == 1 )
  {
    pBuffer[nFileLen] = '\0';
    bResult = ParseConfigCenterConfig(pBuffer,nFileLen);
  }
  fclose(fp);
  mem.deallocate(pBuffer,1);
  return bResult;
}

bool XmlParser::ParseConfigCenterConfig(const char *buf, int len)
{
  std::string attr;
  std::string xmldoc(buf,len);
  CMarkupSTL xml;
  if(!xml.SetDoc(xmldoc.c_str()))
  {
    monitor_log_error("parse config file error");
    return false;
  }
  xml.ResetMainPos();

  if (!xml.FindElem("MODULE"))
  { 
    monitor_log_error("no local module info");
    return false;
  }

  while (xml.FindChildElem("SERVERSHARDING")) 
  {
    xml.IntoElem();
    std::string shardingname = xml.GetAttrib("ShardingName");
    if ("" == shardingname) {
      monitor_log_error("sharding name is empty");
      return false;
    }
    while (xml.FindChildElem("INSTANCE")) 
    {
      if (xml.IntoElem())
      {
        ServerNode node;
        node.ip = xml.GetAttrib("ip");
        node.port = xml.GetAttrib("port");
        node.weight = atoi((xml.GetAttrib("weight")).c_str());
        node.role = xml.GetAttrib("role");
        if ("" == node.ip || "" == node.port || node.weight <= 0){
          monitor_log_error("instance is not correct");
          return false;
        }
        if (node.role != INDEX_GEN_ROLE)
        {
            node.role = SEARCH_ROLE;
        }

        std::string addr = node.ip + ":30311";
        std::pair<std::string, std::string> tmpAddr(shardingname, addr);
        if (std::find(m_oDtcClusterContext.begin(), m_oDtcClusterContext.end(), tmpAddr) 
        == m_oDtcClusterContext.end())
        {
          m_oDtcClusterContext.push_back(tmpAddr);
        }

        m_oSearchCoreClusterContext[shardingname].push_back(node);
        xml.OutOfElem();
      }
    }
    xml.OutOfElem();
  }

  if (0 == m_oDtcClusterContext.size())
  {
    monitor_log_error("local server list is empty");
    return false;
  }

  return true;
}

////***************JsonParser***********************
JsonParser::JsonParser()
    : ParserBase(E_SEARCH_MONITOR_JSON_PARSER)
{ }

bool JsonParser::ParseConfig(std::string path)
{
    Json::Reader reader;
    Json::Value oLocalClusterContext;

    m_oDtcClusterContext.clear();
    m_oSearchCoreClusterContext.clear();

    std::ifstream iStream(path.c_str());
    if (!iStream.is_open())
    {
        monitor_log_error("load %s failed.", path.c_str());
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
                monitor_log_error("ShardingName in incorrect");
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
                        monitor_log_error("ip is incorrect");
                        return false;
                    }

                    std::string addr = node.ip + ":30311";
                    std::pair<std::string, std::string> tmpAddr(node.ip, addr);
                    if (std::find(m_oDtcClusterContext.begin(), m_oDtcClusterContext.end(), tmpAddr) 
                    == m_oDtcClusterContext.end())
                    {
                        m_oDtcClusterContext.push_back(tmpAddr);
                    }

                    if (oInstance[i].isMember("port")
                        && oInstance[i]["port"].isString())
                    {
                        node.port = oInstance[i]["port"].asString();
                    }
                    else
                    {
                        monitor_log_error("port is incorrect");
                        return false;
                    }

                    if (oInstance[i].isMember("weight")
                        && oInstance[i]["weight"].isInt())
                    {
                        node.weight = oInstance[i]["weight"].asInt();
                    }
                    else
                    {
                        log_error("weight is incorrect");
                        return false;
                    }

                    if (oInstance[i].isMember("role")
                        && oInstance[i]["role"].isString())
                    {
                        node.role = oInstance[i]["role"].asString();
                    }

                    if ("" == node.ip || "" == node.port || node.weight <= 0)
                    {
                        log_error("instance is incorrect");
                        return false;
                    }
                    if (node.role != INDEX_GEN_ROLE)
                    {
                        node.role = SEARCH_ROLE;
                    }

                    if(oInstance[i].isMember("disasterRole")
                        && oInstance[i]["disasterRole"].isString())
                    {
                        node.sDisasterRole = oInstance[i]["disasterRole"].asString();
                    }

                    std::vector<ServerNode>::iterator it = std::find(m_oSearchCoreClusterContext[node.ip].begin(),
                            m_oSearchCoreClusterContext[node.ip].end(), node);
                    if (it != m_oSearchCoreClusterContext[node.ip].end())
                    {
                        monitor_log_error("instance %s:%s is repeated", node.ip.c_str(), node.port.c_str());
                        return false;
                    }
                    m_oSearchCoreClusterContext[node.ip].push_back(node);
                }
            }
            else
            {
                monitor_log_error("INSTANCE in incorrect");
                return false;
            }
        }
        if (0 == m_oSearchCoreClusterContext.size())
        {
            monitor_log_error("server is empty");
            return false;
        }
    }
    else
    {
        monitor_log_error("localCluster.json file format has some error, please check.");
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

    DtcClusterCtxIter oDtcIter = m_oDtcClusterContext.begin();
    for( ; oDtcIter != m_oDtcClusterContext.end(); ++oDtcIter)
    {
        log_debug("Json-Parser DtcCluster Key(IP:%s) , Value: (DTC addr:%s)"
        , oDtcIter->first.c_str() ,oDtcIter->second.c_str());
    }
#endif

    return true;
}

////***************JsonParser***********************
ParserBase* const ConfigCenterParser::CreateInstance(int iParserId)
{
        ParserBase* pCurParser = NULL;
        switch (iParserId)
        {
        case E_SEARCH_MONITOR_XML_PARSER:
            {
                pCurParser = new XmlParser();
            }
            break;
        case E_SEARCH_MONITOR_JSON_PARSER:
            {
                pCurParser = new JsonParser();
            }
            break;
        default:
            {
                monitor_log_error("Unknow file type, please check");
            }
            break;
        }

        if(!UpdateParser(pCurParser))
        {
            DELETE(pCurParser);
            pCurParser = NULL;
        }
        return pCurParser;
}

bool ConfigCenterParser::UpdateParser(ParserBase* const pParser)
{
    if(NULL == pParser) { return false;}

    std::string sVaildDir = DtcMonitorConfigMgr::getInstance()->getValidDir();

    if(access(sVaildDir.c_str() , F_OK) != 0)
    {
        monitor_log_info("Path:%s is not existing.",sVaildDir.c_str());
        int iParserId = pParser->GetCurParserId();
        sVaildDir = (E_SEARCH_MONITOR_JSON_PARSER == iParserId) ? "../conf/localCluster.json" : "../conf/localCluster.xml";
    }

    monitor_log_info("sCaDirPath:%s" ,sVaildDir.c_str());

    if(sVaildDir.empty() || !pParser->ParseConfig(sVaildDir))
    {
        monitor_log_error("UpdateParser error, please check");
        return false;
    }
    return true;
}

bool ConfigCenterParser::CheckConfigModifyOrNot(long iStartTime)
{
    std::string sVaildDir = DtcMonitorConfigMgr::getInstance()->getValidDir();
    long modifyTime = GetConfigCurrentTime(sVaildDir);
    monitor_log_debug("check config modify, preCheckTime:%ld , modifyTime:%ld", iStartTime , modifyTime);
    return (modifyTime != iStartTime);
}

long ConfigCenterParser::GetConfigCurrentTime(std::string sPath)
{
    FILE * fp = fopen(sPath.c_str(), "r");
    if(NULL == fp){
        monitor_log_error("open file[%s] error.", sPath.c_str());
        return -1;
    }
    int fd = fileno(fp);
    struct stat buf;
    fstat(fd, &buf);
    long modifyTime = buf.st_mtime;
    fclose(fp);
    return modifyTime;
}
