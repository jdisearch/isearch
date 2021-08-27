/*
*  config_update_operator.h
*
*  Created on: 2021.01.12
*  Author: chenyujie
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
    void Attach() {AttachTimer(m_pUpdateTimerList);};
    virtual void TimerNotify(void);
    bool StartThread(void);

private:
    CPollThread* m_pUpdateThread;
    CTimerList* m_pUpdateTimerList;
    ParserBase* m_pCurParser;
};

#endif
