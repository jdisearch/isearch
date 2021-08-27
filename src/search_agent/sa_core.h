#ifndef __SA_CORE_H__
#define __SA_CORE_H__
#include "sa_msg.h"
#include "sa_conf.h"
#include "sa_server.h"
#include "sa_listener.h"
#include "sa_conn_pool.h"
#include "sa_msg_pool.h"
#define SA_MAXHOSTNAMELEN 256

struct instance
{
    int log_level;
    char *log_dir;
    char *conf_filename;
    char hostname[SA_MAXHOSTNAMELEN];
    int event_max_timeout;
    pid_t pid;
    char *pid_filename;
    unsigned pidfile : 1;
    int cpumask;
    char **argv;
};

class ConnectionInterface;

class CSearchAgentCore
{
public:
    int CoreStart(struct instance *dai);
    void CoreLoop();
    int CoreCore(void *args, uint32_t iEvents);
    CSearchListener * GetAgentListener() {return &m_oSearchAgentListener;};
    CServerPool *GetServerPool() {return &m_oServerPool;}
    void CacheSendEvent(ConnectionInterface *pConnection);
    void RemoveCacheSendEvent(ConnectionInterface *pConnection);
    CSearchAgentMessagePool* GetMessagePool();
private:
    uint64_t m_iMsgId;
    uint64_t m_iFragId;
    CSearchAgentMessagePool m_oSearhAgentMessgePool;
    CSearchAgentConf m_oSearchAgentConf;
    CServerPool m_oServerPool;
    CSearchListener m_oSearchAgentListener;
    uint64_t m_iTimeout;
    std::map<uint32_t, ConnectionInterface *> m_oCachedSendConnections;
private:
    int CoreError(ConnectionInterface *pConnection);
    int CoreRecv(ConnectionInterface *pConnection);
    int CoreSend(ConnectionInterface *pConnection);
    int CoreClose(ConnectionInterface *pConnection);
    //int CoreTimeout();
    void ProcessCachedWriteEvent();

};
#endif
