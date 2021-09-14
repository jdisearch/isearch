/////////////////////////////////////////////////////
//
// for detect Agent and Dtc instance
//     create by qiuyu on Nov 27, 2018
//
/////////////////////////////////////////////////////

#include "DetectUtil.h"
#include "sockaddr.h"
#include "log.h"
#include "detector_instance.h"
#include "DtcMonitorConfigMgr.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sstream>
#include <errno.h>

bool DetectUtil::connectServer(
    int& fd, 
    const std::string& ip, 
    const int port)
{
#if 0
  CSocketAddress addr;
  std::stringstream bindAddr;

  bindAddr << ip << ":" << port << "/tcp";
  addr.SetAddress(bindAddr.str().c_str(), (const char*)NULL);
  if (addr.SocketFamily() != 0)
  {
    monitor_log_error("invalid addr.");
    return false;
  }

  fd = addr.CreateSocket();
  if (fd < 0)
  {
    monitor_log_error("create socket failed. errno:%d", errno);
    return false;
  }

  fcntl(fd, F_SETFL, O_RDWR|O_NONBLOCK);
  int ret = addr.ConnectSocket(fd);
  if (ret != 0)
  {
    monitor_log_error("connect to server failed. ip:%s, portL%d, errno:%d", ip.c_str(), port, errno);
    return false;
  }
#else
  monitor_log_info("connect to server. ip:%s, port:%d", ip.c_str(), port);
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
  {
    monitor_log_error("create socket failed. ip:%s, port:%d", ip.c_str(), port);
    return false;
  }
  
  struct sockaddr_in net_addr;
  net_addr.sin_addr.s_addr = inet_addr(ip.c_str());
  net_addr.sin_family = AF_INET;
  net_addr.sin_port = htons(port);
  // block to connect
  int ret = connect(fd, (struct sockaddr *)&net_addr, sizeof(struct sockaddr));
  if (ret < 0)
  {
    monitor_log_error("connect to server failed, fd:%d, errno:%d, ip:%s, port:%d", fd, errno, ip.c_str(), port);
    close(fd);
    return false;
  }
  
  // set socket to non-block
  fcntl(fd, F_SETFL, O_RDWR|O_NONBLOCK);
#endif

  return true;
}

int DetectUtil::recieveMessage(
    const int fd, 
    char* data, 
    const int dataLen)
{
  int readNum = 0;
  int nRead = 0;
  int nRet = 0;
  do {
    nRet = read(fd, data + nRead, dataLen - nRead);
    if (nRet > 0)
    {
      nRead += nRet;
      if (nRead == dataLen) return nRead;
    }
    else if (nRet == 0)
    {
      // close the connection
      monitor_log_error("client close the socket, fd:%d", fd);
      // return nRead;
      return -1;
    }
    else
    {
      if (readNum < 1000 && (errno == EAGAIN || errno == EINTR))
      {
        readNum++;
        continue;
      }
      else
      {
        // close the connection
        monitor_log_error("client close the socket, fd:%d", fd);
        // return nRead;
        return -1;
      }
    }
  }while (nRead < dataLen);

  return dataLen;
}

int DetectUtil::sendMessage(
    const int netfd,
    char* data, 
    const int dataLen)
{
  int sendNum = 0;
  int nWrite = 0;
  int nRet = 0;
  do {
    nRet = write(netfd, data + nWrite, dataLen - nWrite);
    if (nRet > 0)
    {
      nWrite += nRet;
      if (dataLen == nWrite) return nWrite;
    }
    else if (nRet < 0)
    {
      if (sendNum < 1000 && (errno == EINTR || errno == EAGAIN))
      {
        sendNum++;
        continue;
      }
      else
      {
        // connection has issue, need to close the socket
        monitor_log_error("write socket failed, fd:%d, errno:%d", netfd, errno);
        return -1;
        // return nWrite;
      }
    }
    else
    {
      monitor_log_error("write socket failed, fd:%d, errno:%d", netfd, errno);
      return -1;
    }
  }
  while(nWrite < dataLen);

  return dataLen;
}

bool DetectUtil::detectAgentInstance(
    const std::string& accessKey,
    const std::string& ipWithPort,
    const int timeout,
    bool& isAlive,
    int& errCode)
{
  return CDetectorInstance::DetectAgent(accessKey, ipWithPort, timeout, isAlive, errCode);
}

bool DetectUtil::detectDtcInstance(
    const std::string& ipWithPort,
    const int timeout,
    bool& isAlive,
    int& errCode)
{
  return CDetectorInstance::DetectDTC(ipWithPort, timeout, isAlive, errCode);
}

// network endian is big endian
void DetectUtil::translateByteOrder(uint64_t &value)
{
#if __BYTE_ORDER == __BIG_ENDIAN
  // do noting, network endian is big endian
#elif __BYTE_ORDER == __LITTLE_ENDIAN
  // translate to little endian
  unsigned char *val, temp;
  val = (unsigned char*)&value;
  temp = val[0];
  val[0] = val[7];
  val[7] = temp;
  temp = val[1];
  val[1] = val[6];
  val[6] = temp;
  temp = val[2];
  val[2] = val[5];
  val[5] = temp;
  temp = val[3];
  val[3] = val[4];
  val[4] = temp;
#else
#error unkown endian
#endif

  return;
}
