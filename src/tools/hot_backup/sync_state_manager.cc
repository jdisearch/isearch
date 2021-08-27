#include "sync_state_manager.h"
#include "sync_init_state.h"
#include "sync_rocksdb_state.h"
#include "sync_binlog_state.h"
#include "sync_switch_state.h"
#include "sync_fault_state.h"

SyncStateManager::SyncStateManager()
    : m_iPreState(E_SYNC_STATE_INIT)
    , m_iCurrentState(E_SYNC_STATE_INIT)
    , m_iNextState(E_SYNC_STATE_INIT)
    , m_pCurrentState(NULL)
    , m_pLocalClusterParser(NULL)
    , m_oSyncStateMap()
{
    m_oSyncStateMap.insert(std::make_pair(E_SYNC_STATE_INIT , new InitState(this)));
    m_oSyncStateMap.insert(std::make_pair(E_SYNC_STATE_ROCKSDB , new RocksDbState(this)));
    m_oSyncStateMap.insert(std::make_pair(E_SYNC_STATE_BINLOG , new BinlogState(this)));
    m_oSyncStateMap.insert(std::make_pair(E_SYNC_STATE_MS_SWITCH , new SwitchState(this)));
    m_oSyncStateMap.insert(std::make_pair(E_SYNC_STATE_FAULT , new FaultState(this)));

    m_pCurrentState = m_oSyncStateMap[E_SYNC_STATE_INIT];
}

SyncStateManager::~SyncStateManager()
{
    SyncStateMapIter iter = m_oSyncStateMap.begin();
    for (; iter != m_oSyncStateMap.end(); ++iter)
    {
        DELETE(iter->second);
    }
    
    m_oSyncStateMap.clear();

    DELETE(m_pLocalClusterParser);
}

void SyncStateManager::Start()
{
    assert(m_pCurrentState != NULL);
    m_pCurrentState->Enter();
    m_pCurrentState->HandleEvent();
}

void SyncStateManager::ChangeState(int iNewState)
{
    assert(m_pCurrentState != NULL);
    m_iNextState = iNewState;
    m_pCurrentState->Exit();
    log_info(LOG_KEY_WORD "changeState from %d to %d" , m_iCurrentState , iNewState);

    assert(m_oSyncStateMap.find(iNewState) != m_oSyncStateMap.end());
    m_pCurrentState = m_oSyncStateMap[iNewState];

    m_iPreState = m_iCurrentState;
    m_iCurrentState = iNewState;
    m_pCurrentState->Enter();
    m_pCurrentState->HandleEvent();
}