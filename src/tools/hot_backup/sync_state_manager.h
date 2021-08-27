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

#ifndef SYNC_STATE_MANAGER_H_
#define SYNC_STATE_MANAGER_H_

#include <utility>
#include <assert.h>
#include "sync_common.h"
#include "dtcapi.h"
#include "config_center_parser.h"

class SyncStateBase;
class ParserBase;

class SyncStateManager
{
public:
    SyncStateManager();
    ~SyncStateManager();

public:
    void Start();
    void ChangeState(int iNewState);

public:
    void BindLocalClusterParser(const ParserBase* const pParser)
    { m_pLocalClusterParser = const_cast<ParserBase*>(pParser);}

    ParserBase* const GetLocalClusterParser()
    { return m_pLocalClusterParser; }

    int GetCurrentState() { return m_iCurrentState;}
    int GetNextState() { return m_iNextState;}
    int GetPreState() { return m_iPreState;}

private:
    int m_iPreState;
    int m_iCurrentState;
    int m_iNextState;
    SyncStateBase* m_pCurrentState;
    ParserBase* m_pLocalClusterParser;
    SyncStateMap m_oSyncStateMap;
};

#endif