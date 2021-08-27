/*
 * =====================================================================================
 *
 *       Filename:  sync_init_state.h
 *
 *    Description:  sync_init_state class definition.
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

#ifndef SYNC_INIT_STATE_H_
#define SYNC_INIT_STATE_H_

#include "sync_state_base.h"

class UpdateOperator;

SyncState(Init)

private:
    UpdateOperator* m_pClusterMonitor;
ENDFLAG

#endif