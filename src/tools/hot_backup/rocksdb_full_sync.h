/*
 * =====================================================================================
 *
 *       Filename:  rocksdb_full_sync.h
 *
 *    Description:  rocksdb_full_sync class definition.
 *
 *        Version:  1.0
 *        Created:  13/01/2021
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  chenyujie, chenyujie28@jd.com@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#ifndef ROCKSDB_FULL_SYNC_H_
#define ROCKSDB_FULL_SYNC_H_

#include "uds_client.h"

class RocksdbFullSync
{
public:
    RocksdbFullSync();
    ~RocksdbFullSync();

public:
    bool Run(std::string sLocalRocksDbPort , std::string sPeerShardMasterIp , int iPeerShardMasterPort);

private:
    bool InitRocksDB(std::string sPort);
    void RegisterMasterInfo(std::string sPeerShardMasterIp , int iPeerShardMasterPort);
    bool RangeQueryReq(void);
    DirectResponseContext ParseResponseFromBytes(int , char* , int );
    int SendToRocksDb(char* pBuffer, int iBufferLen, int iCmdType);

private:
    UDSClient* m_pUdsClient;
};
#endif