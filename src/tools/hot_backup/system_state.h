/*
 * =====================================================================================
 *
 *       Filename:  system_state.h
 *
 *    Description:  system_state class definition.
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

#ifndef SYSTEM_STATE_H_
#define SYSTEM_STATE_H_

#include "noncopyable.h"
#include "singleton.h"

enum SYNC_SWITCH_ENUM
{
    E_SYNC_SWITCH_ON,
    E_SYNC_SWITCH_OFF
};

class SystemState : private noncopyable
{
public:
    SystemState() 
    : m_iSyncSwitch(E_SYNC_SWITCH_OFF){};
    virtual ~SystemState() {};

public:
    static SystemState* Instance()
    {
        return CSingleton<SystemState>::Instance();
    };

    static void Destroy()
    {
        CSingleton<SystemState>::Destroy();
    };

public:
    void SwithOpen() { m_iSyncSwitch = E_SYNC_SWITCH_ON;}
    void SwitchClose() { m_iSyncSwitch = E_SYNC_SWITCH_OFF;}
    bool GetSwitchState() {
        return (E_SYNC_SWITCH_ON == m_iSyncSwitch);
    }

private:
    int m_iSyncSwitch;
};

#endif
