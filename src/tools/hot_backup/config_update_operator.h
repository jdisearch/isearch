/*
 * =====================================================================================
 *
 *       Filename:  config_update_operator.h
 *
 *    Description:  config_update_operator class definition.
 *
 *        Version:  1.0
 *        Created:  12/01/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  chenyujie, chenyujie28@jd.com@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef CONFIG_UPDATE_OPERATOR_H_
#define CONFIG_UPDATE_OPERATOR_H_

#include "timerlist.h"
#include "log.h"
#include <string>

class CPollThread;
class ParserBase;

class UpdateOperator: public CTimerObject
{
public:
    UpdateOperator();
    virtual ~UpdateOperator();

public:
    void Attach() {AttachTimer(m_pUpdateTimerList);}
    virtual void TimerNotify(void);
    bool InitUpdateThread(void);

private:
    CPollThread* m_pUpdateThread;
    CTimerList* m_pUpdateTimerList;
    ParserBase* m_pCurParser;
};

#endif
