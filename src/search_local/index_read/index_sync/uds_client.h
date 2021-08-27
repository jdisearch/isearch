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

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>  
#include <iostream>

class DirectRequestContext;

class UDSClient{
public:
    UDSClient();
    ~UDSClient() {
        Close();
    };
    bool Connect(const char *path);
    bool sendAndRecv(
        DirectRequestContext &direct_request_context, 
        std::vector<std::vector<std::string> >& row_fields, 
        std::vector<struct TableField* > table_field_vec);

    int receiveMessage(char *data, int dataLen);
    int sendMessage(const char *data, int dataLen);
    void Close();
private:
    int client_socket;
};


#endif
