#ifndef __SA_EPOLL_H__
#define __SA_EPOLL_H__
#include <sys/epoll.h>
#include "sa_conn.h"
#include "singleton.h"

class CEventBase
{
public:
    static CEventBase *Instance()
	{
		return CSingleton<CEventBase>::Instance();
	}
    static void Destroy()
	{
		CSingleton<CEventBase>::Destroy();
	}
   
    int EpollCreate(int nevent);
    int EventAddConnection(ConnectionInterface *pConnection);
    int EventDelConnection(ConnectionInterface *pConnection);
    int EventDelOut(ConnectionInterface *pConnection);
    int EventWait(int iTimeout, CSearchAgentCore *pCore);
private:
    int m_iEp;
    struct epoll_event *m_pEvents;
    int m_iEventCnt;
};

#endif
