#ifndef __SA_UTIL_H__
#define __SA_UTIL_H__
#include <string>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/un.h>
#include <stddef.h>


using namespace std;

struct sockinfo {
    int       family;              /* socket address family */
    socklen_t addrlen;             /* socket address length */
    union {
        struct sockaddr_in  in;    /* ipv4 socket address */
        struct sockaddr_in6 in6;   /* ipv6 socket address */
        struct sockaddr_un  un;    /* unix domain address */
    } addr;
};


int set_nonblocking(int fd);
int resolve_inet(const string &name, int port, struct sockinfo  *si);
int set_tcpnodelay(int fd);
uint64_t get_now_us();
#endif
