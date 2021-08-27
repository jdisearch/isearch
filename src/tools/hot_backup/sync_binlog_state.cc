#include "sync_binlog_state.h"
#include "comm.h"
#include "indexgen_reporter.h"
#include "sync_unit.h"

BinlogState::BinlogState(SyncStateManager* pSyncStateManager)
    : SyncStateBase()
    , m_pSyncUnit(new CFullSyncUnit())
{
    m_pSyncStateManager = pSyncStateManager;
}

BinlogState::~BinlogState()
{
    DELETE(m_pSyncUnit);
}

void BinlogState::Enter()
{
    // 设置同步状态
    CComm::registor.SetSyncStatus(E_SYNC_BINLOG_SYNC_ING);

    // 刷新本地Indexgen连接
    ParserBase*  pParser = m_pSyncStateManager->GetLocalClusterParser();
    IndexGenNetContext oTempNetContext(pParser->GetLocalIndexGenIp()
                , pParser->GetLocalIndexGenPort());

    if (!IndexgenReporter::Instance()->Connect2LocalIndexGen(oTempNetContext))
    {
        m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
        return;
    }
}

void BinlogState::Exit()
{
    int iNextState = m_pSyncStateManager->GetNextState();
    if (E_SYNC_STATE_MS_SWITCH == iNextState)
    {
        log_info("State: Binlog -> Switch");
        // 主备切换，需重新获取新主的Binlog位置
        CComm::registor.ClearJournalID();
    }
}

void BinlogState::HandleEvent()
{
        // 开始Binlog的同步
        switch (CComm::registor.Regist())
        {
        case -DTC::EC_FULL_SYNC_STAGE:
        case -DTC::EC_INC_SYNC_STAGE:
            {
                if (m_pSyncUnit->Run(&CComm::master,1))
                {
                    m_pSyncStateManager->ChangeState(E_SYNC_STATE_MS_SWITCH);
                }else{
                    m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
                }
            }
            break;
        case -DTC::EC_ERR_SYNC_STAGE:
        default:
		    {
			    log_error("hb status is __NOT__ correct!"
				    "try use --fixed parament to start");
                m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
		    }
            break;
        }
}