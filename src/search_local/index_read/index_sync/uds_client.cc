/*
 * =====================================================================================
 *
 *       Filename:  uds_client.h
 *
 *    Description:  uds client class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include "uds_client.h"
#include "log.h"
#include "rocksdb_direct_context.h"

#define DATA_SIZE 4
#define BUFFER_SIZE 1024


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

void parseResponse(char *data, int dataLen, std::vector<std::vector<std::string> >& row_fields, int field_num){
    int sMagicNum = *(uint16_t*)data;
    data += sizeof(uint16_t);
    dataLen -= sizeof(uint16_t);

    int sSequenceId = *(uint64_t*)data;
    data += sizeof(uint64_t);
    dataLen -= sizeof(uint64_t);
    
    int sRowNums = *(int16_t*)data;
    data += sizeof(int16_t);
    dataLen -= sizeof(int16_t);
    
    log_debug("sMagicNum is %d, sSequenceId is %d, sRowNums is %d, field_num is %d",sMagicNum, sSequenceId, sRowNums, field_num);

    //获取行数
    row_fields.resize(sRowNums);
    for (int16_t idx = 0; idx < sRowNums; idx++ )
    {
        for (int idx1 = 0; idx1 < field_num; idx1++ )
	    {
        
            //先获取四个字节长度  再获取相应的大小
            int value_len = *(int*)data;
            data += sizeof(int);
            dataLen -= sizeof(int);

            //将接下来一定数量的字符转化为string
            std::string value(data, value_len);
            data += value_len;
            dataLen -= value_len;
            log_debug("field value:%s", value.c_str());
            row_fields[idx].push_back(value);
	    }
    }

    log_debug("parseResponse end");
    assert(dataLen==0);

}


bool UDSClient::sendAndRecv(DirectRequestContext &direct_request_context, std::vector<std::vector<std::string> >& row_fields, std::vector<struct TableField* > table_field_vec)
{

    int len = direct_request_context.binary_size(DirectRequestType::eRangeQuery);
    char *send_buffer = (char*)malloc(len + DATA_SIZE);
    *(int*)send_buffer = len;
    //send_buffer += DATA_SIZE;
    

    direct_request_context.serialize_to(send_buffer + DATA_SIZE, len);

    log_debug("send byte len is %d",len + DATA_SIZE);
    if(sendMessage(send_buffer, len + DATA_SIZE) == -1){
        log_error("sendMessage error!!!!!!!, fd:%d, errno:%d", client_socket, errno);
        free(send_buffer);
        return false;
    }
    
    log_debug("send byte len is over");
    
    //先接收4个字节
    int data_len = 0;
    if(receiveMessage((char*)&data_len, DATA_SIZE) == -1){
        log_error("receiveMessage error!!!!!!!, fd:%d, errno:%d", client_socket, errno);
        free(send_buffer);
        return false;
    }

    //获取需要接收的字符长度
    log_debug("data_len is %d",data_len);
    //根据字符长度获取字符
    char *recv_buffer = (char*)malloc(data_len);
    if(receiveMessage(recv_buffer, data_len) == -1){
        log_error("receiveMessage error!!!!!!!, fd:%d, errno:%d", client_socket, errno);
        free(send_buffer);
        free(recv_buffer);
        return false;
    }

    parseResponse(recv_buffer, data_len, row_fields, (int)table_field_vec.size());

    free(send_buffer);
    free(recv_buffer);
    return true;
}
