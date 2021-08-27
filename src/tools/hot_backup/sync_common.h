/*
 * =====================================================================================
 *
 *       Filename:  sync_common.h
 *
 *    Description:  sync_common class definition.
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

#ifndef SYNC_COMMON_H_
#define SYNC_COMMON_H_

#include <stdlib.h>
#include <map>

class SyncStateBase;

#define LOG_KEY_WORD  "[SyncState]:"

enum SYNC_STATE_ENUM
{
    E_SYNC_STATE_INIT,
    E_SYNC_STATE_MS_SWITCH,
    E_SYNC_STATE_ROCKSDB,
    E_SYNC_STATE_BINLOG,
    E_SYNC_STATE_FAULT
};

enum SYNC_STATE_ERROR_ENUM
{
    E_SYNC_STATE_NORMAL = 0,
    E_SYNC_STATE_SHELL_RUN_FAIL = -100
};

typedef std::map<int , SyncStateBase*> SyncStateMap;
typedef SyncStateMap::iterator SyncStateMapIter;


#endif