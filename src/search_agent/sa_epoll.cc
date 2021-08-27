#include "sa_epoll.h"
#include "sa_core.h"
#include "log.h"
#include "errno.h"
#include <malloc.h>

int CEventBase::EpollCreate(int nevent)
{
    log_debug("CEventBase::%s", __FUNCTION__);
    m_iEventCnt = nevent;
    m_iEp = epoll_create(nevent);
    if (m_iEp < 0)
    {
        log_error("create epoll error");
        return -1;
    }
    int status = fcntl(m_iEp, F_SETFD, FD_CLOEXEC);
    if (status < 0)
    {
        log_error("fcntl for FD_CLOEXEC on e %d failed :%s", m_iEp, strerror(errno));
        close(m_iEp);
        return -1;
    }

    m_pEvents = (struct epoll_event *)malloc(nevent * sizeof(*m_pEvents));
    if (m_pEvents == NULL)
    {
        return -1;
    }
    return 0;
}

int CEventBase::EventAddConnection(ConnectionInterface *pConnection)
{
    struct epoll_event event;
    event.events = (uint32_t)(EPOLLIN | EPOLLOUT | EPOLLET);
    event.data.ptr = pConnection;
    int status = epoll_ctl(m_iEp, EPOLL_CTL_ADD, pConnection->GetFd(), &event);
    if (status < 0)
    {
        log_error("epoll ctrl on e %d, failed %s", m_iEp, strerror(errno));
    }
    return status;
}

int CEventBase::EventDelOut(ConnectionInterface *pConnection)
{
    log_debug("CEventBase::%s", __FUNCTION__);
    struct epoll_event event;
    int ep = m_iEp;
    event.events = (uint32_t)(EPOLLIN | EPOLLET);
    event.data.ptr = pConnection;
    int status = epoll_ctl(ep, EPOLL_CTL_MOD, pConnection->GetFd(), &event);
    if (status < 0)
    {
        log_error("epoll  ctl on e %d sd %d failed %s", ep, pConnection->GetFd(), strerror(errno));
    }
    return status;
}

int CEventBase::EventWait(int iTimeout, CSearchAgentCore *pCore)
{
    struct epoll_event *pEvents = m_pEvents;
    int nevent = m_iEventCnt;
    for (;;)
    {
        int i;
        nevent = epoll_wait(m_iEp, pEvents, m_iEventCnt, iTimeout);
        if(nevent > 0)
        {
            for(i = 0 ; i < nevent;  i++ )
            {
                struct epoll_event *pTempEvent =  &pEvents[i];
                ConnectionInterface *pConnection = (ConnectionInterface *)(pTempEvent->data.ptr);
                log_debug("epoll triggered on conn %d", pConnection->GetFd());
                pCore->CoreCore(pTempEvent->data.ptr, pTempEvent->events);
            }
            return nevent;
        }
        if(nevent == 0)
        {
            if(iTimeout == -1)
            {
                log_error("epoll wait on e %d with %d events and %d timeout returned no events", m_iEp, nevent, iTimeout);
                return -1;
            }
            return 0;
        }
        if(errno == EINTR)
        {
            continue;
        }
        log_error("epoll wait on e %d with %d events failed :%s", m_iEp, nevent, strerror(errno));
        return -1;
    }
    
}


int CEventBase::EventDelConnection(ConnectionInterface *pConnection)
{
    
    int status = epoll_ctl(m_iEp, EPOLL_CTL_DEL, pConnection->GetFd(), NULL);
    if (status < 0)
    {
        log_error("epoll ctrl on e %d, failed %s", m_iEp, strerror(errno));
    }
    return status;
}