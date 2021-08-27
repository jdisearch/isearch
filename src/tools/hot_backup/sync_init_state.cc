#include "sync_init_state.h"
#include "sync_state_manager.h"
#include "daemon.h"
#include "config_center_parser.h"
#include "config_update_operator.h"

InitState::InitState(SyncStateManager* pSyncStateManager)
    : SyncStateBase()
    , m_pClusterMonitor(NULL)
{ 
    m_pSyncStateManager = pSyncStateManager;
}

InitState::~InitState()
{
    DELETE(m_pClusterMonitor);
}

void InitState::Enter()
{
    stat_init_log_("hbp");
    log_info(LOG_KEY_WORD "enter into init state...");
    DaemonBase::DaemonStart(CComm::backend);

    // 以 fix 模式启动，清理hb、slave环境
	if (CComm::fixed) {
		if (CComm::fixed_hb_env()) {
			m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
            return;
		}
		if (CComm::fixed_slave_env()) {
			m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
            return;
		}
	}

    if (CComm::check_hb_status()) {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
        log_error("check hb status error.");
		return;
	}
    if (CComm::registor.Init()) {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
        log_error("init registor error.");
		return;
	}
}

void InitState::Exit()
{
    log_error(LOG_KEY_WORD "exit init state");
}

void InitState::HandleEvent()
{
    assert(m_pSyncStateManager);

    // 锁住hbp的日志目录
	if (CComm::uniq_lock()) {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
		log_error("another process already running, exit");
		return;
	}

    // 初始化集群配置文件解析器，加载基础配置项
    ParserBase*  pParser = ConfigCenterParser::Instance()->CreateInstance(E_HOT_BACKUP_JSON_PARSER);
    if (NULL == pParser)
	{
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
		log_error("no memory for pLocalClusterParser");
		return;
	}

        // 设置log打印权限
    log_info("logLevel:%d.", pParser->GetConfigContext().iLogLevel);
	stat_set_log_level_(pParser->GetConfigContext().iLogLevel);

    // 绑定文件解析器至StateManager
    m_pSyncStateManager->BindLocalClusterParser(pParser);

    // 初始化集群配置文件监控
    m_pClusterMonitor = new UpdateOperator();
    if (NULL == m_pClusterMonitor)
    {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
        log_error("no memory for pLocalClusterConfigUpdater");
        return;
    }
    if (!m_pClusterMonitor->InitUpdateThread())
    {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
        log_error("update operator init failed.");
        return;
    }

    // 跳转至下一个状态
    NextState();
}
