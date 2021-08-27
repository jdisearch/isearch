#include "sync_fault_state.h"

FaultState::FaultState(SyncStateManager* pSyncStateManager)
    : SyncStateBase()
{
    m_pSyncStateManager = pSyncStateManager;
}

FaultState::~FaultState()
{

}

void FaultState::Enter()
{
    log_error("Here is ErrorState");
}

void FaultState::Exit()
{
    // do nothing
}

void FaultState::HandleEvent()
{
    // 简单处理，程序结束
    log_info("GoodBye hot_backup.");
    exit(0);
}