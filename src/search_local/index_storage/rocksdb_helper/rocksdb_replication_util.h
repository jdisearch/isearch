/////////////////////////////////////////////////////
//
// for detect Agent and Dtc instance
//     create by qiuyu on Nov 27, 2018
//
/////////////////////////////////////////////////////

#ifndef __DETECT_UTIL_H__
#define __DETECT_UTIL_H__

#include <string>

class ReplicationUtil
{
public:
static int sockBind(
    const std::string& ip, 
    uint16_t port, 
    int backlog = 0);

static int connectServer(
    const std::string& ip, 
    const int port);

static int recieveMessage(
    const int fd, 
    char* data, 
    const int dataLen);

static int sendMessage(
    const int fd, 
    char* data, 
    const int dataLen);

static void translateByteOrder(uint64_t& value); 
};

#endif // __DETECT_UTIL_H__
