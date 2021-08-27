#include "rocksdb_full_sync.h"
#include <stdlib.h>
#include "log.h"
#include "memcheck.h"
#include "system_state.h"

RocksdbFullSync::RocksdbFullSync()
    : m_pUdsClient(new UDSClient())
{}

RocksdbFullSync::~RocksdbFullSync()
{
    DELETE(m_pUdsClient);
}

bool RocksdbFullSync::Run(std::string sLocalRocksDbPort , std::string sPeerShardMasterIp , int iPeerShardMasterPort)
{
    if (!InitRocksDB(sLocalRocksDbPort)){
		log_error("init rocksDb failed");
		return false;
	}

	RegisterMasterInfo(sPeerShardMasterIp , iPeerShardMasterPort);

	if (!RangeQueryReq()){
		log_error("rocksDb fullSync failed");
		return false;
	}
    return true;
}

bool RocksdbFullSync::InitRocksDB(std::string sPort)
{
    std::string sUnixSocketDir = "/tmp/domain_socket/rocks_direct_";
	sUnixSocketDir.append(sPort);
	sUnixSocketDir.append(".sock");
    log_info("rocksdb dir:%s,", sUnixSocketDir.c_str());
    return m_pUdsClient->Connect(sUnixSocketDir.c_str());
}

void RocksdbFullSync::RegisterMasterInfo(std::string sPeerShardMasterIp , int iPeerShardMasterPort)
{ 
    DirectRequestContext direct_request_context;
    memset(&direct_request_context , 0 , sizeof(DirectRequestContext));
	direct_request_context.sDirectQueryType = (uint8_t)DirectRequestType::eReplicationRegistry;

    ReplicationQuery_t oReplicationQuery;
    oReplicationQuery.sMasterIp = sPeerShardMasterIp;
    oReplicationQuery.sMasterPort = iPeerShardMasterPort;
    log_error("rocksDb fullSync ip:%s, port:%d", sPeerShardMasterIp.c_str() , iPeerShardMasterPort);
    direct_request_context.sPacketValue.uReplicationQuery = (uint64_t)(&oReplicationQuery);

    int iRegistryLength = direct_request_context.binary_size(DirectRequestType::eReplicationRegistry);
    char* buffer = (char*)malloc(iRegistryLength + DATA_SIZE);
    *(int*)buffer = iRegistryLength;

    direct_request_context.serialize_to(buffer + DATA_SIZE, iRegistryLength);
    SendToRocksDb(buffer , iRegistryLength + DATA_SIZE, (int)DirectRequestType::eReplicationRegistry);
    free(buffer);
}

bool RocksdbFullSync::RangeQueryReq()
{
    DirectRequestContext direct_request_context;
    memset(&direct_request_context , 0 , sizeof(DirectRequestContext));
    direct_request_context.sDirectQueryType = (uint8_t)DirectRequestType::eReplicationStateQuery;

    int iRangeQueryLength = direct_request_context.binary_size(DirectRequestType::eReplicationStateQuery);
    char* buffer = (char*)malloc(iRangeQueryLength + DATA_SIZE);
    *(int*)buffer = iRangeQueryLength;

    direct_request_context.serialize_to(buffer + DATA_SIZE, iRangeQueryLength);

    int iRet = -1;
    while (iRet != (int)ReplicationState::eReplicationFinished
    && iRet != (int)ReplicationState::eReplicationFailed)
    {
        iRet = SendToRocksDb(buffer , iRangeQueryLength + DATA_SIZE, (int)DirectRequestType::eReplicationStateQuery);
        if (SystemState::Instance()->GetSwitchState()){
            return false;
        }
        
        usleep(1000000); // 1s
    }
    free(buffer);
    return (int)ReplicationState::eReplicationFinished == iRet;
}

int RocksdbFullSync::SendToRocksDb(char* pBuffer, int iBufferLen, int iCmdType)
{
    int iRecvLen = 0;
    char* pRecvBuffer = m_pUdsClient->sendAndRecv(pBuffer , iBufferLen , iRecvLen);

    DirectResponseContext oResponse;
    if (pRecvBuffer != NULL && iRecvLen != 0)
    {
        oResponse = ParseResponseFromBytes(iCmdType , pRecvBuffer , iRecvLen);
        free(pRecvBuffer);
    }
    log_info("rocksDb response ret:%d" , (int)oResponse.sDirectRespValue.uReplicationState);
    return (uint64_t)oResponse.sDirectRespValue.uReplicationState;
}

DirectResponseContext RocksdbFullSync::ParseResponseFromBytes(int iDirectRequestType, char* pPointer, int iLength)
{
    DirectResponseContext oTempDirRespContext;
    oTempDirRespContext.sMagicNum = *(uint16_t*)pPointer;
    pPointer += sizeof(uint16_t);
    iLength -= sizeof(uint16_t);

    oTempDirRespContext.sSequenceId = *(uint64_t*)pPointer;
    pPointer += sizeof(uint64_t);
    iLength -= sizeof(uint64_t);

    oTempDirRespContext.sRowNums = *(int16_t*)pPointer;
    pPointer += sizeof(int16_t);
    iLength -= sizeof(int16_t);

    switch ((DirectRequestType)iDirectRequestType)
    {
        // case E_DR_REGISTRY:
        case DirectRequestType::eReplicationStateQuery:
        {
            char ss = pPointer[0];
            oTempDirRespContext.sDirectRespValue.uReplicationState = atoi(&ss);
            log_error("new replication state: %d", atoi(&ss));
            pPointer += sizeof(uint8_t);
            iLength -= sizeof(uint8_t);
        }
        break;
    default:
        break;
    }

    if (0 != iLength)
    {
        log_error("cmd may be async mode");
    }
    
    return oTempDirRespContext;
}
