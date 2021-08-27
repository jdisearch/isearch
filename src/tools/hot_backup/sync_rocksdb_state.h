/*
 * =====================================================================================
 *
 *       Filename:  sync_rocksdb_state.h
 *
 *    Description:  sync_rocksdb_state class definition.
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

#ifndef SYNC_ROCKSDB_STATE_H_
#define SYNC_ROCKSDB_STATE_H_

#include "sync_state_base.h"

class RocksdbFullSync;

SyncState(RocksDb)

private:
    RocksdbFullSync* m_pRocksdbFullSync;
ENDFLAG

#endif