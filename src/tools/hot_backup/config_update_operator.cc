#include "config_update_operator.h"

#include "assert.h"
#include "config_center_parser.h"
#include "poll_thread.h"
#include "comm.h"
#include "memcheck.h"
#include "system_state.h"

UpdateOperator::UpdateOperator()
    : CTimerObject()
    , m_pUpdateThread(new CPollThread("localClusterConfigMonitor"))
    , m_pUpdateTimerList(NULL)
    , m_pCurParser(ConfigCenterParser::Instance()->CreateInstance(E_HOT_BACKUP_JSON_PARSER))
{
    if (m_pUpdateThread != NULL)
    {
        int iUpdateInterval = (NULL == m_pCurParser) ? 1 : m_pCurParser->GetConfigContext().iUpdateInterval;
        m_pUpdateTimerList = m_pUpdateThread->GetTimerList(iUpdateInterval);
    }
}

UpdateOperator::~UpdateOperator()
{ 
    DELETE(m_pUpdateThread);
    DELETE(m_pCurParser);
}

bool UpdateOperator::InitUpdateThread()
{
    assert(m_pCurParser != NULL);

    if (!ConfigCenterParser::Instance()->UpdateParser(m_pCurParser))
    {
        return false;
    }

    if (-1 == m_pUpdateThread->InitializeThread())
    {
        log_error("init updateThread error.");
        return false;
    }
    m_pUpdateThread->RunningThread();
    Attach();

    return true;
}

void UpdateOperator::TimerNotify()
{
    static std::string sAppFieldFile = m_pCurParser->GetlocalClusterPath();
    static std::string sMasterInfo = m_pCurParser->GetPeerShardMasterIp();
    FILE * fp = fopen(sAppFieldFile.c_str(), "r");
    if(NULL == fp){
        log_error("open file[%s] error.", sAppFieldFile.c_str());
        Attach();
        return ;
    }
    int fd = fileno(fp);
    struct stat buf;
    fstat(fd, &buf);
    long modifyTime = buf.st_mtime;
    static long lastModifyTime = modifyTime;
    fclose(fp);
    if(modifyTime != lastModifyTime){
        lastModifyTime = modifyTime;

        if (!ConfigCenterParser::Instance()->UpdateParser(m_pCurParser)){
            Attach();
            return;
        }

        std::string sNewMaster = m_pCurParser->GetPeerShardMasterIp();
        log_error("Master-Slave happening...,currentMaster:%s , newMaster:%s",
            sMasterInfo.c_str() , sNewMaster.c_str());
        if (sMasterInfo != sNewMaster)
        {
            sMasterInfo = sNewMaster;
            SystemState::Instance()->SwithOpen();
            log_error("Master-Slave-Swithing...");
        }else{
            log_info("Current Sharding no change");
        }
    }
    Attach();
    return;
}
