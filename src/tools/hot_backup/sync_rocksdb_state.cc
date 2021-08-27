#include "sync_rocksdb_state.h"
#include "rocksdb_full_sync.h"

RocksDbState::RocksDbState(SyncStateManager* pSyncStateManager)
    : SyncStateBase()
    , m_pRocksdbFullSync(new RocksdbFullSync())
{
    m_pSyncStateManager = pSyncStateManager;
}

RocksDbState::~RocksDbState()
{
    DELETE(m_pRocksdbFullSync);
}

void RocksDbState::Enter()
{
    log_info(LOG_KEY_WORD "enter into RocksDb state...");
    if (NULL == m_pRocksdbFullSync)
    {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
    }

    CComm::registor.SetSyncStatus(E_SYNC_ROCKSDB_FULL_SYNC_ING);
}

void RocksDbState::Exit()
{
    log_error(LOG_KEY_WORD "exit RocksDb state");
}

void RocksDbState::HandleEvent()
{
    // 进行RocksDb存储层全量同步
    ParserBase*  pParser = m_pSyncStateManager->GetLocalClusterParser();
    if (!m_pRocksdbFullSync->Run(pParser->GetLocalkeywordDtcPort(),
                pParser->GetPeerShardMasterNet().m_sIp,
                pParser->GetPeerShardMasterNet().m_iPort))
    {
        log_error("RocksdbFullSync run failed");
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
        return;
    }

    log_info("\"RocksDb-FULL-SYNC\" is finish");
    CComm::registor.SetSyncStatus(E_SYNC_ROCKSDB_FULL_SYNC_FINISH);

    // 跳转至下一个状态
    m_pSyncStateManager->ChangeState(E_SYNC_STATE_BINLOG);
}