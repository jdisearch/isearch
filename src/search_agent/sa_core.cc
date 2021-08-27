#include "sa_core.h"
#include "log.h"
#include "sa_epoll.h"
#include "sa_error.h"

#define MAX_EPOLL_TIMEOUT  3000

CSearchAgentMessagePool *CSearchAgentCore::GetMessagePool()
{
    return &m_oSearhAgentMessgePool;
}

int CSearchAgentCore::CoreStart(struct instance *dai)
{
    log_debug("CSearhcAgentCore::%s", __FUNCTION__);
    m_oSearhAgentMessgePool.InitMessage();
    
    int status = m_oSearchAgentConf.ConfCreate(dai->conf_filename);
    if (status != 0)
    {
        log_error("create conf error");
        return -1;
    }

    status = CEventBase::Instance()->EpollCreate(1024);
    if (status != 0)
    {
        log_error("create epoll error");
        return -1;
    }

    status = m_oServerPool.ServerPoolInit(&m_oSearchAgentConf);
    if (status != 0)
    {
        log_error(" server pool init error");
        return -1;
    }

    status = m_oSearchAgentListener.ListenerInit();
    if (status != 0)
    {
        log_error("  listener init error");
        return -1;
    }
    m_iTimeout = MAX_EPOLL_TIMEOUT;
    InitErrorMessageMapper();
    return 0;
}

void CSearchAgentCore::CoreLoop()
{
    CEventBase::Instance()->EventWait(m_iTimeout, this);
    ProcessCachedWriteEvent();
    m_iTimeout = MAX_EPOLL_TIMEOUT;
    m_oSearhAgentMessgePool.ReclaimTimeoutMessage(m_iTimeout, this);
   // m_iTimeout /= 1000;
}

int CSearchAgentCore::CoreCore(void *arg, uint32_t iEvents)
{
    ConnectionInterface *pConnection = (ConnectionInterface *)arg;
    int status = 0;
    if (iEvents & EPOLLERR)
    {
        CoreError(pConnection);
    }
    if (iEvents & EPOLLIN)
    {
        status = CoreRecv(pConnection);
        if (status != 0 || pConnection->IsEof())
        {
            CoreClose(pConnection);
            log_error("core recv error");
            return -1;
        }
    }
    if (iEvents & EPOLLOUT)
    {
        status = CoreSend(pConnection);
         if (status != 0 ||  pConnection->IsEof())
        { 
            CoreClose(pConnection);
            log_error("core send error");
            return -1;
        }
    }
    return 0;
}

int CSearchAgentCore::CoreRecv(ConnectionInterface *pConnection)
{
    int status = pConnection->Recieve(this);
    if (status != 0)
    {
        log_error("recv on %d failed, %s", pConnection->GetFd(), strerror(errno));
    }
    return status;
}

int CSearchAgentCore::CoreSend(ConnectionInterface *pConnection)
{
    int status = pConnection->Send(this);
    if (status != 0)
    {
        log_error("send on %d failed, %s", pConnection->GetFd(), strerror(errno));
    }
    return status;
}

int CSearchAgentCore::CoreError(ConnectionInterface *pConnection)
{
    int status = 0;
    //int status =  pConnection->m_funcRecv(pConnection, this);
    if (status != 0)
    {
        log_error("recv on %d failed, %s", pConnection->GetFd(), strerror(errno));
    }
    return status;
}

void CSearchAgentCore::CacheSendEvent(ConnectionInterface *pConnection)
{
    log_debug("CSearhcAgentCore::%s", __FUNCTION__);
    if (!pConnection->IsCached())
    {
       // m_oCachedSendConnections.push_back(pConnection);
        m_oCachedSendConnections.insert(make_pair(pConnection->GetConnectionId(), pConnection));
        pConnection->SetCached(true);
    }
}
void CSearchAgentCore::RemoveCacheSendEvent(ConnectionInterface *pConnection)
{
    log_debug("CSearhcAgentCore::%s", __FUNCTION__);
        m_oCachedSendConnections.erase(pConnection->GetConnectionId());
        pConnection->SetCached(false);
}

void CSearchAgentCore::ProcessCachedWriteEvent()
{
    log_debug("CSearhcAgentCore::%s, cached connections %zu", __FUNCTION__,  m_oCachedSendConnections.size());
    for (std::map<uint32_t, ConnectionInterface *>::iterator it = m_oCachedSendConnections.begin() ; it != m_oCachedSendConnections.end(); it++)
    {
        ConnectionInterface *pConnection = it->second;
        pConnection->SetCached(false);
        CoreSend(pConnection);
    }
    m_oCachedSendConnections.clear();
}



int CSearchAgentCore::CoreClose(ConnectionInterface *pConnection)
{
    pConnection->Close(this);
    //CEventBase::Instance()->EventDelConnection(pConnection);
  //  delete pConnection;
    return 0;
}

