/*
 * =====================================================================================
 *
 *       Filename:  sync_binlog_state.h
 *
 *    Description:  sync_binlog_state class definition.
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

#ifndef SYNC_BINLOG_STATE_H_
#define SYNC_BINLOG_STATE_H_

#include "sync_state_base.h"

class CFullSyncUnit;

SyncState(Binlog)

private:
    CFullSyncUnit* m_pSyncUnit;

ENDFLAG

#endif