/*
* sc_conf.h
*
*  Created on: 2021.1.13
*  Author:  chenyujie
*/

#ifndef SYSTEM_STATE_H_
#define SYSTEM_STATE_H_

#include "noncopyable.h"
#include "singleton.h"

class SystemState : private noncopyable
{
public:
    SystemState()
    : m_bReportFlag(false)
    { }
    virtual ~SystemState()
    { }

public:
    static SystemState* Instance()
    {
        return CSingleton<SystemState>::Instance();
    }

    static void Destroy()
    {
        CSingleton<SystemState>::Destroy();
    }

public:
    void SetReportIndGenFlag(bool bFlag) { m_bReportFlag = bFlag;}
    bool GetReportIndGenFlag() { return m_bReportFlag;}

private:
    bool m_bReportFlag;
};
#endif
