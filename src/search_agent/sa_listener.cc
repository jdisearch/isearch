#include "sa_listener.h"
#include "sa_server.h"
#include "sa_epoll.h"
#include "sa_core.h"
#include "log.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "sa_conn.h"

#define HTTP_LISTEN_PORT 80



int CSearchListenerConnection::ListenerAccept(CSearchAgentCore *pCoreCtx)
{
    int fd;
    int status;
    for (;;)
    { 
        fd = accept(m_iFd, NULL, NULL);
        if (fd < 0)
        {
            if (errno == EINTR)
            {
                log_debug("accept on listener %d not ready, -eintr", m_iFd);
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNABORTED)
            {
                m_eConnectionStatus &= ~RECV_READY ;
                log_debug("accept on %d not ready eagain", m_iFd);
                return 0;
            }
            if (errno == EMFILE || errno == ENFILE)
            {
                m_eConnectionStatus &= ~RECV_READY ;
                log_debug("accept on %d , no enough fd for use", m_iFd);
                return 0;
            }
            log_error("accept on listener %d failed %s ", m_iFd, strerror(errno));
            return -1;
        }
        break;
    }
    ConnectionInterface *pClientConnection = CSearchAgentConnectionPool::Instance()->GetClientConnection(pCoreCtx);
    if (pClientConnection == NULL)
    {
        log_error("get connection for %d from %d failed %s ", m_iFd, fd, strerror(errno));
        status = close(fd);
        return -1;
    }
    pClientConnection->SetFd(fd);
    status = set_nonblocking(fd);
    if (status < 0)
    {
        log_error("set nonblock on client %d, from p  %d failed %s", fd, m_iFd, strerror(errno));
        return status;
    }
    status = set_tcpnodelay(fd);
    if (status < 0)
    {
        //pClientConnection->m_funcClose();
        log_error("set tcpnodelay on client %d, from p %d  failed %s", fd, m_iFd, strerror(errno));
        return status;
    }
    status = fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (status < 0)
    {
        //pClientConnection->Close();
        log_error("fcntl FD_CLOEXEC on c %d from p %d failed %s", fd, m_iFd, strerror(errno));
        return status;
    }
    status = CEventBase::Instance()->EventAddConnection(pClientConnection);
    if (status < 0)
    {
        //pClientConnection->Close();
        log_error("event add conn from client %d failed %s", fd, strerror(errno));
        return status;
    }
    
    return 0;
}


int CSearchListener::ListenerInit()
{
    ConnectionInterface *pSearchAgentConn = CSearchAgentConnectionPool::Instance()->GetListener(this);
    if (pSearchAgentConn == NULL)
    {
        log_error("t get listener connection error");
        return -1;
    }
    CSearchListenerConnection *pListenerConnection = static_cast<CSearchListenerConnection *> (pSearchAgentConn) ;
    return pListenerConnection->Initialize();
}


CSearchListenerConnection::CSearchListenerConnection(uint32_t iConnectionId):ConnectionInterface(iConnectionId)
{
    m_eConnectionType = LISTENER;
}

void CSearchListenerConnection::Reference(void *pOwner)
{
    struct sockinfo oSockInfo;
    resolve_inet("0.0.0.0", HTTP_LISTEN_PORT, &oSockInfo);
    memcpy(&m_oSocketAddress, &(oSockInfo.addr), sizeof(sockaddr_in));
}
    
int CSearchListenerConnection::Recieve(CSearchAgentCore *pCoreCtx)
{
    m_eConnectionStatus |= RECV_READY ;
    do{
         int status = ListenerAccept(pCoreCtx);
         if(status != 0){
             return status;
         }
    }while(m_eConnectionStatus & RECV_READY);
    return 0;
}

int CSearchListenerConnection::Initialize()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        log_error("socket failed: %s", strerror(errno));
        return -1;
    }
    m_iFd =  fd;
    int status;
    int reuse = 1;
    socklen_t len = sizeof(reuse);
    status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, len);
    if (status < 0)
    {
        log_error("reuse off addr failed %s", strerror(errno));
        return -1;
    }

    status = bind(fd, &m_oSocketAddress, sizeof(struct sockaddr));
    if (status < 0)
    {
        log_error(" bind http listener error %s", strerror(errno));
        return -1;
    }

    status = listen(fd, 1024);
    if (status < 0)
    {
        log_error("listen  error for fd %d,%s", fd, strerror(errno));
        return -1;
    }

    status = set_nonblocking(fd);
    if (status < 0)
    {
        log_error("set fd %d nonblocking error %s", fd, strerror(errno));
        return -1;
    }

    status = CEventBase::Instance()->EventAddConnection(this);
    if (status < 0)
    {
        return -1;
    }
    status = CEventBase::Instance()->EventDelOut(this);
    if (status < 0)
    {
        return -1;
    }

    return 0;
}

