/////////////////////////////////////////////////////
//
// for detect Agent and Dtc instance
//     create by qiuyu on Nov 27, 2018
//
/////////////////////////////////////////////////////

#ifndef __DETECT_UTIL_H__
#define __DETECT_UTIL_H__

#include <string>
#include <stdint.h>

class DetectUtil
{
public:
static bool connectServer(
    int& fd, 
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

static bool detectAgentInstance(
    const std::string& accessKey,
    const std::string& ipWithPort,
    const int timeout,
    bool& isAlive,
    int& errCode);

static bool detectDtcInstance(
    const std::string& ipWithPort,
    const int timeout,
    bool& isAlive,
    int& errCode);

static void translateByteOrder(uint64_t& value); 
};

#endif // __DETECT_UTIL_H__
