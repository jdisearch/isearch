#include "sa_util.h"
#include "sys/time.h"
#include "log.h"

int set_nonblocking(int fd)
{
    int flags;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
    {
        return flags;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int resolve_inet(const string &name, int port, struct sockinfo *si)
{
    struct addrinfo hints;
    struct addrinfo *ai, *cai;
    string node;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_addrlen = 0;
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    node = name;
    char service[20];
    snprintf(service, 20, "%d", port);
    int status = getaddrinfo(node.c_str(), service, &hints, &ai);
    if (status < 0)
    {
        log_error("address resolution of node %s:%s, failed: %s", node.c_str(), service, strerror(errno));
        return -1;
    }
    bool found = false;
    for (cai = ai; cai != NULL; cai = cai->ai_next)
    {
        si->family = cai->ai_family;
        si->addrlen = cai->ai_addrlen ;
        memcpy(&si->addr, cai->ai_addr, si->addrlen);
        found = true;
        break;
    }
    freeaddrinfo(ai);
    return !found ? -1 : 0 ;
}


int set_tcpnodelay(int fd)
{
    int nodelay ;
    socklen_t len;
    nodelay =1;
    len = sizeof(nodelay);
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, len);
}

uint64_t get_now_us()
{
    timeval now;
    gettimeofday(&now, NULL);
    uint64_t now_timestamp = (uint64_t) now.tv_sec * (uint64_t)1000000;
    return now_timestamp + now.tv_usec;
}