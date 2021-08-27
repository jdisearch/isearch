#include "config_update_operator.h"
#include "poll_thread.h"
#include "comm.h"
#include "memcheck.h"
#include "config_center_parser.h"
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

bool UpdateOperator::StartThread(void)
{
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
    if (NULL == m_pCurParser)
    {
        log_error("config center parser init failed.");
        return;
    }

    static long lastModifyTime = 0;
    static std::string sAppFieldFile = m_pCurParser->GetlocalClusterPath();
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
    fclose(fp);
    if(modifyTime != lastModifyTime){
        lastModifyTime = modifyTime;
        if (!ConfigCenterParser::Instance()->UpdateParser(m_pCurParser))
        {
            Attach();
            return;
        }
        SystemState::Instance()->SetReportIndGenFlag(m_pCurParser->CheckLocalIpIsMaster());
    } else {
        log_debug("file[%s] not change.", sAppFieldFile.c_str());
    }
    Attach();
    return;
}