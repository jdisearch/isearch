/*
 * =====================================================================================
 *
 *       Filename:  sync_state_manager.h
 *
 *    Description:  SyncStateManager class definition.
 *
 *        Version:  1.0
 *        Created:  13/01/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  chenyujie, chenyujie28@jd.com@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef SYNC_STATE_BASE_H_
#define SYNC_STATE_BASE_H_

#include <assert.h>
#include "log.h"
#include "memcheck.h"
#include "comm.h"
#include "registor.h"
#include "sync_state_manager.h"

class SyncStateBase
{
public:
    SyncStateBase() : m_pSyncStateManager(NULL) {};
    virtual ~SyncStateBase() {};

public:
    /// **************************
    /// 进入当前状态时，一些处理，比如: 初始化
    /// **************************
    virtual void Enter(void) = 0;

    /// **************************
    /// 退出当前状态时，一些处理
    /// **************************
    virtual void Exit(void) = 0;

    /// **************************
    /// 当前状态时，所要处理的业务逻辑，包括：状态跳转判断逻辑
    /// **************************
    virtual void HandleEvent() = 0;

    /// **************************
    /// 各状态下，统一跳转判断逻辑
    /// **************************
    void NextState()
    {
        int iSyncStatus = CComm::registor.GetSyncStaus();
        log_debug("sync status:%d",iSyncStatus);

        switch (iSyncStatus)
        {
        case E_SYNC_PURE:
        case E_SYNC_ROCKSDB_FULL_SYNC_FINISH:
        case E_SYNC_BINLOG_SYNC_ING:
            {
                m_pSyncStateManager->ChangeState(E_SYNC_STATE_MS_SWITCH);
            }
            break;
        case E_SYNC_ROCKSDB_FULL_SYNC_ING:
            {
                log_error("hb status is __NOT__ correct!try use --fixed parament to start");
                m_pSyncStateManager->ChangeState(E_SYNC_STATE_FAULT);
            }
            break;
        default:
            break;
        }
    };

protected:
    SyncStateManager* m_pSyncStateManager;
};

// 减少冗余代码编写
#define SYNCSTATE_NAME(stateName)  stateName##State

#define SyncState(stateName)                                    \
class SYNCSTATE_NAME(stateName) : public SyncStateBase          \
{                                                               \
public:                                                         \
    SYNCSTATE_NAME(stateName)(SyncStateManager*);               \
    virtual ~SYNCSTATE_NAME(stateName)();                       \
                                                                \
public:                                                         \
    virtual void Enter(void);                                   \
    virtual void Exit(void);                                    \
    virtual void HandleEvent();                                 

#define ENDFLAG };

#endif