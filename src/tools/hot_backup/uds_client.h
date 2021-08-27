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

#ifndef _UDS_CLIENT_H_
#define _UDS_CLIENT_H_

#include "rocksdb_direct_context.h"

#define DATA_SIZE 4
#define BUFFER_SIZE 1024

class UDSClient{
public:
    UDSClient();
    ~UDSClient() {
        Close();
    };

public:
    bool Connect(const char *path);
    char* sendAndRecv(char* pSendBuffer , int iSendLength , int& iRecvLen);

private:
    int receiveMessage(char *data, int dataLen);
    int sendMessage(const char *data, int dataLen);
    void Close();

private:
    int client_socket;
};
#endif
