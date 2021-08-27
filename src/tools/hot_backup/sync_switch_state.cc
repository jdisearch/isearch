#include "sync_switch_state.h"
#include "system_state.h"

SwitchState::SwitchState(SyncStateManager* pSyncStateManager)
    : SyncStateBase()
{
    m_pSyncStateManager = pSyncStateManager;
}

SwitchState::~SwitchState()
{

}

void SwitchState::Enter()
{
    log_info(LOG_KEY_WORD "enter into Switch state...");

    // 主备确认，需加载集群配置文件
    ParserBase* pParser = m_pSyncStateManager->GetLocalClusterParser();
    if (!ConfigCenterParser::Instance()->UpdateParser(pParser))
    {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
        return;
    }
}

void SwitchState::Exit()
{
    SystemState::Instance()->SwitchClose();
    log_error(LOG_KEY_WORD "exit Switch state");
}

void SwitchState::HandleEvent()
{
    // 更新 新主，并检查主备是否活跃
    ParserBase* pParser = m_pSyncStateManager->GetLocalClusterParser();
    if (CComm::ReInitDtcAgency(pParser))
    {
        log_error("reinit dtc master and slave error.");
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
        return;
    }

    // 提前获取主Binlog的位置
    int iRet = CComm::registor.Regist();
    if (iRet != -DTC::EC_INC_SYNC_STAGE
    && iRet != -DTC::EC_FULL_SYNC_STAGE)
    {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
        return;
    }

    // 跳转至下一个状态
    int iPreState = m_pSyncStateManager->GetPreState();
    if (E_SYNC_STATE_INIT == iPreState && -DTC::EC_FULL_SYNC_STAGE == iRet)
    {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_ROCKSDB);
    }else if (E_SYNC_STATE_BINLOG == iPreState || -DTC::EC_INC_SYNC_STAGE == iRet)
    {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_BINLOG);
    }else{
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
    }
}