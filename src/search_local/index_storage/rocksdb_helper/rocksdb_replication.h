//////////////////////////////////////////////////////////////////////
//
//  
// replication unit under Master-slave Architechture 
//
//////////////////////////////////////////////////////////////////////

#ifndef __ROCKSDB_REPLICATION_H_
#define __ROCKSDB_REPLICATION_H_

#include "common/poller.h"
#include "elastic_buffer.h"
#include "rocksdb_conn.h"

#define REPLICATON_HEADER_MAGIC 0x7654321
#define REPLICATION_PACKET_SIZE (10 * (1 << 20)) // 10M

#define CLEAR_BITS(packetFlag) (packetFlag &= 0x0)

// 2 bit for setting whether the key and value exist in the packet 
#define SET_START_KEY(packetFlag) (packetFlag |= 0x01)
#define CLEAR_START_KEY(packetFlag) (packetFlag &= -2)
#define HAS_START_KEY(packetFlag) (packetFlag & 0x01)

#define SET_END_KEY(packetFlag) (packetFlag |= 0x02)
#define CLEAR_END_KEY(packetFlag) (packetFlag &= -3)
#define HAS_END_KEY(packetFlag) (packetFlag & 0x02)

// 6 bit for replication type
enum class ReplicationType;
#define SET_REQUEST_TYPE(packetFlag, type) (packetFlag |= (type << 2))
#define GET_REQUEST_TYPE(packetFlag) ((RocksdbReplication::ReplicationType)((packetFlag & 0xff) >> 2))

typedef struct ReplicationPacket
{
  int sMagic = REPLICATON_HEADER_MAGIC;
  // char sReplicationType;
  unsigned int sPacketFlag;
  int sRawPacketLen;
  char kvRows[0];

  ReplicationPacket()
  {
    sPacketFlag = 0;
    sRawPacketLen = 0;
  }
    
} ReplicationPacket_t;

class PollThread;
class RocksdbReplication
{
  public:
    enum ReplicationType : unsigned char
    {
      eReplSync,   // slave register
      eReplReqAck,    // slave ask for replication
      eReplRepAck, // response from master
      eReplFin,     // replication finished
      
      // can never bigger than the 'eReplMax'
      eReplMax = 63 //  ((2 << 5) -1)
    };
    
    enum ReplicationErr
    {
      eConnectRefused,
      eStartMasterFailed,
      eAcceptSlaveFailed
    };
  
  private:
    RocksDBConn* mRocksdb;
    PollThread* mGlobalReplicationThread;

  public:
    RocksdbReplication(RocksDBConn* rocksdb);
    virtual ~RocksdbReplication() {}

    int initializeReplication();
    //////////////////////////////////////////
    //            master api
    //////////////////////////////////////////

    //////////////////////////////////////////
    //            slave api
    //////////////////////////////////////////
    int startSlaveReplication(
        const std::string& masterIp,
        int masterPort);
    
    int getReplicationState();

  private:
    int startMasterListener();
    // int updateReplicationState(int state);
};

class RocksReplicationChannel : public PollerObject
{
  public:
    enum WorkerType
    {
      eReplListener, // master wait for slave connect
      eReplChannel   // real replication channel
    };

  private:
    WorkerType   mWorkerType;
    RocksDBConn* mRocksdb;
    PollThread* mLocalReplicationThread; // reference to mGlobalReplicationThread
    ReplicationPacket_t mPacketHeader;
    ElasticBuffer *mPacketBuffer;
    
    // slave temporary variable
    std::string mReplStartKey;
    std::string mReplEndKey;

  public:
    RocksReplicationChannel(
        WorkerType type,
        RocksDBConn* rocksdb,
        PollThread* poll,
        int fd);
    virtual ~RocksReplicationChannel();

    int attachThread();
    virtual void input_notify(void);
    virtual void output_notify(void);
    virtual void hangup_notify(void);
    int triggerReplication();
    
  private:
  public:
    void handleAccept();
    void handleReplication();
    int handleReplicationRegister();
    int handleReplicationRequest();
    int handleReplicationResponse();
    int handleReplicationFinished();
    int recieveReplicationData();
    int sendReplicationData();
    int masterFillRangeKV(std::string& startKey, std::string& endKey);
    int slaveFillRangeKV();
    int slaveConstructRequest(RocksdbReplication::ReplicationType rType);
};


#endif // __ROCKSDB_REPLICATION_H_ 
