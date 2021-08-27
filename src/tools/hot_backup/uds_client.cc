#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>  
#include <iostream>
#include <vector>
#include <errno.h>
#include "log.h"
#include "uds_client.h"



UDSClient::UDSClient(){
    client_socket = -1;
}

bool UDSClient::Connect(const char *path){
    client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        log_error("创建socket失败");
        return false;
    }
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path));

    if (connect(client_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        log_error("连接服务端失败");
        return false;
    }
    return true;
}

void UDSClient::Close(){
  if(client_socket>0){
    close(client_socket);
  }
}

int UDSClient::sendMessage(const char *data, int dataLen){
    int sendNum = 0;
    int nWrite = 0;
    int ret = 0;
    do {
        ret = send(client_socket, data + nWrite, dataLen - nWrite, 0);
        if ( ret > 0 )
        {
            nWrite += ret;
        } 
        else if ( ret < 0 )
        {
            if ( sendNum < 1000 && (errno == EINTR || errno == EAGAIN) )
            {
                sendNum++;
                continue;
            }
            else
            {
            // connection has issue, need to close the socket
            log_error("write socket failed, fd:%d, errno:%d", client_socket, errno);
            return -1;
            }
        }
        else
        {
            if ( dataLen == 0 ) return 0;

            log_error("unexpected error!!!!!!!, fd:%d, errno:%d", client_socket, errno);
            return -1;
        }
    } while ( nWrite < dataLen );
    
    return 0;
}


int UDSClient::receiveMessage(char *data, int dataLen){
    int readNum = 0;
    int nRead = 0;
    int ret = 0;
    do {
        ret = recv(client_socket, data + nRead, dataLen - nRead, 0);
        if ( ret > 0 )
        {
            nRead += ret;
        }
        else if ( ret == 0 )
        {
            // close the connection
            log_error("peer close the socket, fd:%d", client_socket);
            return -1;
        }
        else
        {
            if ( readNum < 1000 && (errno == EAGAIN || errno == EINTR) )
            {
                readNum++;
                continue;
            }
            else
            {
                // close the connection
                log_error("read socket failed, fd:%d, errno:%d", client_socket, errno);
                return -1;
            }
        }
    } while ( nRead < dataLen );
    return 0;
}

char* UDSClient::sendAndRecv(char* pSendBuffer , int iSendLength , int& iRecvLen)
{
    if(sendMessage(pSendBuffer, iSendLength) == -1){
        log_error("sendMessage error!!!!!!!, fd:%d, errno:%d", client_socket, errno);
        return NULL;
    }
    log_debug("send byte len is over");
    
    //先接收4个字节
    iRecvLen = 0;
    if(receiveMessage((char*)&iRecvLen, DATA_SIZE) == -1){
        log_error("receiveMessage error!!!!!!!, fd:%d, errno:%d", client_socket, errno);
        return NULL;
    }

    //获取需要接收的字符长度
    log_debug("data_len is %d",iRecvLen);
    //根据字符长度获取字符
    char *recv_buffer = (char*)malloc(iRecvLen);
    if(receiveMessage(recv_buffer, iRecvLen) == -1){
        log_error("receiveMessage error!!!!!!!, fd:%d, errno:%d", client_socket, errno);
        return NULL;
    }

    log_debug("send and recv success");
    return recv_buffer;
}
