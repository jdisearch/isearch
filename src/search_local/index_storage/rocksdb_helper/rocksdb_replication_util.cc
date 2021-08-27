/////////////////////////////////////////////////////
//
// for detect Agent and Dtc instance
//     create by qiuyu on Nov 27, 2018
//
/////////////////////////////////////////////////////

#include "rocksdb_replication_util.h"
#include "common/net_addr.h"
#include "common/log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sstream>

int ReplicationUtil::sockBind (
    const std::string& ip, 
    uint16_t port, 
    int backlog)
{
	int netfd;
	int reuse_addr = 1;
	struct sockaddr_in inaddr;

	bzero (&inaddr, sizeof (struct sockaddr_in));
	inaddr.sin_family = AF_INET;
	inaddr.sin_port = htons (port);

	if(inet_pton(AF_INET, ip.c_str(), &inaddr.sin_addr) <= 0)
	{
		log_error("invalid address %s:%d", ip.c_str(), port);
		return -1;
	}

	if((netfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		log_error("make tcp socket error, %m");
		return -1;
	}

	setsockopt(netfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof (reuse_addr));
	setsockopt(netfd, SOL_TCP, TCP_NODELAY, &reuse_addr, sizeof (reuse_addr));
	reuse_addr = 60;
	/* 避免没有请求的空连接唤醒epoll浪费cpu资源 */
	setsockopt(netfd, SOL_TCP, TCP_DEFER_ACCEPT, &reuse_addr, sizeof (reuse_addr));

	if(bind(netfd, (struct sockaddr *)&inaddr, sizeof(struct sockaddr)) == -1)
	{
		log_error("bind tcp %s:%u failed, %m", ip.c_str(), port);
		close (netfd);
		return -1;
	}

	if(listen(netfd, backlog) == -1)
	{
		log_error("listen tcp %s:%u failed, %m", ip.c_str(), port);
		close (netfd);
		return -1;
	}
  
	return netfd;
}

int ReplicationUtil::connectServer(
    const std::string& ip, 
    const int port)
{
  log_info("connect to server. ip:%s, port:%d", ip.c_str(), port);
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
  {
    log_error("create socket failed. ip:%s, port:%d", ip.c_str(), port);
    return -1;
  }
  
  struct sockaddr_in net_addr;
  net_addr.sin_addr.s_addr = inet_addr(ip.c_str());
  net_addr.sin_family = AF_INET;
  net_addr.sin_port = htons(port);
  // block to connect
  int ret = connect(fd, (struct sockaddr *)&net_addr, sizeof(struct sockaddr));
  if (ret < 0)
  {
    log_error("connect to server failed, fd:%d, errno:%d, ip:%s, port:%d", fd, errno, ip.c_str(), port);
    close(fd);
    return -1;
  }
  
  // set socket to non-block
  // fcntl(fd, F_SETFL, O_RDWR|O_NONBLOCK);

  return fd;
}

int ReplicationUtil::recieveMessage(
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
      log_error("client close the socket, fd:%d", fd);
      return -1;
    }
    else
    {
      if (readNum < 1000 || (errno == EAGAIN || errno == EINTR))
      {
        readNum++;
        log_error("fd:%d read error ,try again.", fd);
      }
    }
  }while (nRead < dataLen);

  return dataLen;
}

int ReplicationUtil::sendMessage(
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
      if (sendNum < 1000 || (errno == EINTR || errno == EAGAIN))
      {
        sendNum++;
        log_error("fd:%d send error ,try again.", netfd);
        continue;
      }
      else
      {
        // connection has issue, need to close the socket
        log_error("write socket failed, fd:%d, errno:%d", netfd, errno);
        return -1;
        // return nWrite;
      }
    }
    else
    {
      log_error("write socket failed, fd:%d, errno:%d", netfd, errno);
      return -1;
    }
  }
  while(nWrite < dataLen);

  return dataLen;
}

// network endian is big endian
void ReplicationUtil::translateByteOrder(uint64_t &value)
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
