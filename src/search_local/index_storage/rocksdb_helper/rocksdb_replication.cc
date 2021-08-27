//////////////////////////////////////////////////////////////////////
//
//  
// replication unit under Master-slave Architechture 
//
//////////////////////////////////////////////////////////////////////

#include "rocksdb_replication.h"
#include <dbconfig.h>
#include "log.h"

#include "common/poll_thread.h"
#include "common/dtcutils.h"
#include "rocksdb_replication_util.h"
#include "rocksdb_direct_context.h"
#include "db_process_rocks.h"

#define REPLICATION_STATE_KEY "replication_state_key__"
#define REPLICATION_END_KEY "replication_end_key__"
#define REPLICATION_HOLDER_VALUE "9876543210"

extern DTCConfig *gConfig;

// operation level, [prev state][current state]
static bool gReplicationState[(int)ReplicationState::eReplicationMax + 1][(int)ReplicationState::eReplicationMax + 1] = {
  {true, true, true, false, false},
  {false, true, true, false, false},
  {false, true, true, true, true},
  {false, true, false, true, true},
  {false, false, false, false, true}
};

int getReplicationState(RocksDBConn* rocksdb)
{
  std::string state;
  RocksDBConn::RocksItr_t rocksItr;

  int ret = rocksdb->get_entry(REPLICATION_STATE_KEY, state, RocksDBConn::COLUMN_META_DATA);
  if (ret == -RocksDBConn::ERROR_KEY_NOT_FOUND)
  {
    return (int)ReplicationState::eReplicationNoOpr;
  }
  else if (ret < 0)
  {
    log_error("get the replication state from rocksdb failed! ret:%d", ret);
    return -1;
  }

  return std::stoi(state);
}

int updateReplicationState(RocksDBConn* rocksdb, int state)
{
  int prevState = getReplicationState(rocksdb);
  if (!gReplicationState[(int)prevState][(int)state])
  {
    log_error("incompatible state, prev:%d, curr:%d", prevState, state);
    return -1;
  }

  int ret = rocksdb->replace_entry(REPLICATION_STATE_KEY, std::to_string(state), true, RocksDBConn::COLUMN_META_DATA);
  if (ret < 0)
  {
    log_error("init replication status failed! errcode:%d", ret);
  }
  return ret;
}

RocksReplicationChannel::RocksReplicationChannel(
    WorkerType type,
    RocksDBConn* rocksdb,
    PollThread* poll,
    int fd)
:
PollerObject(poll, fd),
mWorkerType(type),
mRocksdb(rocksdb),
mLocalReplicationThread(poll)
{
  mPacketBuffer = new ElasticBuffer();
}

RocksReplicationChannel::~RocksReplicationChannel()
{
  // call base to free the connection
  
  // free elastic buffer
  if (mPacketBuffer) free(mPacketBuffer);
}

int RocksReplicationChannel::attachThread()
{
  enable_input();
  int ret = PollerObject::attach_poller();
  if (ret < 0)
  {
    log_error("attach thread failed!, fd:%d", netfd);
    return -1;
  }

  return 0;
}

void RocksReplicationChannel::input_notify()
{
  log_error("come into rocksdb replication inputNotify");
  switch (mWorkerType)
  {
    case WorkerType::eReplListener:
      return handleAccept();
    case WorkerType::eReplChannel:
      return handleReplication();
    default:
      log_error("unkonwn replication worker type:%d", (int)mWorkerType);
  }

  return;
}

void RocksReplicationChannel::output_notify()
{
  log_error("OutputNotify:: can never come here!!!");
  return;
}

void RocksReplicationChannel::hangup_notify()
{
  log_error("HangupNotify:: close connection!");
  // close socket
  PollerObject::detach_poller();
  delete this;
}

int RocksReplicationChannel::triggerReplication()
{
  // do fully replication from the end of rocksdb
  std::string tempValue;
  RocksDBConn::RocksItr_t rocksItr;
  int ret = mRocksdb->get_last_entry(mReplStartKey, tempValue, rocksItr);
  if (ret < 0)
  {
    log_error("get the start replication key from rocksdb failed!");
    return -1;
  }

  // get replication end key from rocksdb in 'COLUMN_META_DATA' column family
  mReplEndKey = "";
  ret = mRocksdb->get_entry(REPLICATION_END_KEY, mReplEndKey, RocksDBConn::COLUMN_META_DATA);
  if (ret < 0 && ret != -RocksDBConn::ERROR_KEY_NOT_FOUND)
  {
    log_error("get the start replication key from rocksdb failed! ret:%d", ret);
    return -1;
  }

  ret = slaveConstructRequest(RocksdbReplication::eReplSync);
  if (ret < 0)
  {
    log_error("construct slave sync request failed!");
    return -1;
  }

  ret = sendReplicationData();
  if (ret < 0)
  {
    log_error("send sync replication message failed!");
    return -1;
  }

  int state = (int)ReplicationState::eReplicationRegistry; 
  ret = updateReplicationState(mRocksdb, state);
  if (ret < 0)
  {
    log_error("init replication state failed!");
    return -1;
  }
  
  return 0;
}


// accept slave connection and create replication channel
void RocksReplicationChannel::handleAccept()
{
  log_error("handle accept!!!!!");
  int slaveFd;
  struct sockaddr_in peer;
  socklen_t peerSize = sizeof(peer);
  
  // extracts all the connected connections in the pending queue until return
  // EAGAIN 
  while (true)
  {
    slaveFd = accept(netfd, (struct sockaddr*)&peer, &peerSize);
    if (-1 == slaveFd)
    {
      if (errno == EINTR)
      {
        // system call "accept" was interrupted by signal before a valid connection 
        // arrived, go on acceptting
        continue;
      }

      if(errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // no remaining connection on the pending queue, break out
        // log_notice("accept new client error: %m, %d", errno);
        return;
      }

      // accept error
      log_error("accept slave connectting failed, netfd:%d, errno:%d", netfd, errno);
      return;
    }

    log_error("accept slave connectting, peerAddr:%s, slaveFd:%d", inet_ntoa(peer.sin_addr), slaveFd);

    // construct replication worker 
    RocksReplicationChannel* channel = new RocksReplicationChannel(RocksReplicationChannel::eReplChannel, mRocksdb, mLocalReplicationThread, slaveFd);
    if (!channel)
    {
      close(slaveFd);
      log_error("create replication channel failed! peerAddr:%s",  inet_ntoa(peer.sin_addr));
      // return -RocksdbReplication::eAcceptSlaveFailed;
      return;
    }

    int ret = channel->attachThread();
    if (ret < 0)
    {
      delete channel;
      log_error("add replication channel to epoll failed! slaveAddr:%s, fd:%d", inet_ntoa(peer.sin_addr), slaveFd);
      // return -RocksdbReplication::eAcceptSlaveFailed;
      return;
    }
  }

  return;
}

// do replication in the channel
void RocksReplicationChannel::handleReplication()
{
  int ret = recieveReplicationData();
  if (ret < 0) goto HANDLER_END; 
  
  // check header
  assert (mPacketHeader.sMagic == REPLICATON_HEADER_MAGIC); 
  
  {
    RocksdbReplication::ReplicationType type = GET_REQUEST_TYPE(mPacketHeader.sPacketFlag);
    switch (type)
    {
    case RocksdbReplication::eReplSync:
      ret = handleReplicationRegister();
      break;
    case RocksdbReplication::eReplReqAck:
      ret = handleReplicationRequest();
      break;
    case RocksdbReplication::eReplRepAck:
      ret = handleReplicationResponse();
      break;
    case RocksdbReplication::eReplFin:
      ret = handleReplicationFinished();
      break;
    default:
      ret = -1;
      log_error("unkonwn request type:%d", (int)type);
    }
  }

HANDLER_END:
  if (ret < 0)
  {
    int state = (int)ReplicationState::eReplicationFailed; 
    int ret = updateReplicationState(mRocksdb, state);
    if (ret < 0)
    {
      log_error("update replication state failed!");
    }
    hangup_notify();
  }

  return;
}

// master handle slave replication registry
int RocksReplicationChannel::handleReplicationRegister()
{
  // get replication start key and end key
  int ret;
  std::string startKey = "", endKey = "";
  if (HAS_START_KEY(mPacketHeader.sPacketFlag))
  {
    ret = mPacketBuffer->getStrValue(startKey); 
    if (ret != 0)
    {
      log_error("get start key failed!");
      return -1;
    }
  }
  CLEAR_START_KEY(mPacketHeader.sPacketFlag);
  
  if (HAS_END_KEY(mPacketHeader.sPacketFlag))
  {
    ret = mPacketBuffer->getStrValue(endKey);
    if (ret != 0)
    {
      log_error("get start key failed!");
      return -1;
    }
    CLEAR_END_KEY(mPacketHeader.sPacketFlag);
  }
  else 
  {
    // bring back end key
    SET_END_KEY(mPacketHeader.sPacketFlag);
  }
  
  ret = masterFillRangeKV(startKey, endKey);
  if (ret < 0)
  {
    log_error("get range key-value failed! key:%s, value:%s", startKey.c_str(), endKey.c_str());
    return -1;
  }
  
  ret = sendReplicationData();
  if (ret < 0)
  {
    log_error("send replication data to slave failed! key:%s, value:%s", startKey.c_str(), endKey.c_str());
    return -1;
  }
  
  // master no need to hold on the replication state
  // int state = (int)ReplicationState::eReplicationFullSync; 
  // ret = updateReplicationState(mRocksdb, state);
  // if (ret < 0)
  // {
  //   log_error("update replication state failed!");
  //   return -1;
  // }

  return 0;
}

// master handle replication request
int RocksReplicationChannel::handleReplicationRequest()
{
  // get replication start key and end key
  std::string startKey = "", endKey = "";
  
  if (HAS_START_KEY(mPacketHeader.sPacketFlag) == 0)
  {
    log_error("no replication start key in request!");
    return -1;
  }
  int ret = mPacketBuffer->getStrValue(startKey); 
  if (ret != 0)
  {
    log_error("get start key failed!");
    return -1;
  }
  
  if (HAS_END_KEY(mPacketHeader.sPacketFlag) == 0)
  {
    log_error("no replication end key in request!");
    return -1;
  }
  ret = mPacketBuffer->getStrValue(endKey);
  if (ret != 0 || endKey.empty())
  {
    log_error("get start key failed! ret:%d", ret);
    return -1;
  }
  
  CLEAR_START_KEY(mPacketHeader.sPacketFlag);
  CLEAR_END_KEY(mPacketHeader.sPacketFlag);

  ret = masterFillRangeKV(startKey, endKey);
  if (ret < 0)
  {
    log_error("get range key-value failed! key:%s, value:%s", startKey.c_str(), endKey.c_str());
    return -1;
  }
 
  ret = sendReplicationData();
  if (ret < 0)
  {
    log_error("send replication data to slave failed! key:%s, value:%s", startKey.c_str(), endKey.c_str());
    return -1;
  }

  return 0;
}

// slave side
int RocksReplicationChannel::handleReplicationResponse()
{
  int ret;
  // response from handshake with 'replication sync' cmd
  if (HAS_END_KEY(mPacketHeader.sPacketFlag))
  {
    ret = mPacketBuffer->getStrValue(mReplEndKey);
    if (ret != 0)
    {
      log_error("get start key failed!");
      return -1;
    }
   
    assert(!mReplEndKey.empty());

    ret = mRocksdb->insert_entry(REPLICATION_END_KEY, mReplEndKey, true, RocksDBConn::COLUMN_META_DATA);
    if (ret >= 0)
    {
    }
    else if (ret == RocksDBConn::ERROR_DUPLICATE_KEY)
    {
      log_info("insert duplicate replication end key! key:%s", mReplEndKey.c_str());
    }
    else
    {
      log_error("insert the replication end key into rocksdb failed! ret:%d, key:%s", ret, mReplEndKey.c_str());
      return -1;
    }
  }
  
  ret = slaveFillRangeKV();
  if (ret < 0)
  {
    log_error("save range key-value failed!");
    return -1;
  }
  
  ret = slaveConstructRequest(RocksdbReplication::eReplReqAck);
  if (ret < 0)
  {
    log_error("slave side construct replication request failed!");
    return -1;
  }

  ret = sendReplicationData();
  if (ret < 0)
  {
    log_error("send replication data to slave failed! startKey:%s, endKey:%s", mReplStartKey.c_str(), mReplEndKey.c_str());
    return -1;
  }

  return 0;
}

// slave do fully replication finished
int RocksReplicationChannel::handleReplicationFinished()
{
  int ret;
  // response from handshake with 'replication sync' cmd
  if (HAS_END_KEY(mPacketHeader.sPacketFlag))
  {
    ret = mPacketBuffer->getStrValue(mReplEndKey);
    if (ret != 0)
    {
      log_error("get start key failed!");
      return -1;
    }
   
    assert(!mReplEndKey.empty());
    
    // save replication end key to prevent crash
    ret = mRocksdb->insert_entry(REPLICATION_END_KEY, mReplEndKey, true, RocksDBConn::COLUMN_META_DATA);
    if (ret >= 0)
    {
    }
    else if (ret == RocksDBConn::ERROR_DUPLICATE_KEY)
    {
      log_info("insert duplicate replication end key! key:%s", mReplEndKey.c_str());
    }
    else
    {
      log_error("insert the replication end key into rocksdb failed! ret:%d, key:%s", ret, mReplEndKey.c_str());
      return -1;
    }
  }
  
  ret = slaveFillRangeKV();
  if (ret < 0)
  {
    log_error("save range key-value failed!");
    return -1;
  }
  
  // replication has safely done, remove meta data from storage and close the connection
  assert(!mReplEndKey.empty());
  
  ret = mRocksdb->delete_entry(mReplEndKey, true, RocksDBConn::COLUMN_META_DATA);
  if (ret < 0)
  {
    log_error("remove replication end key failed! key:%s, ret:%d", mReplEndKey.c_str(), ret);
    return -1;
  }
  
  int state = (int)ReplicationState::eReplicationFinished; 
  ret = updateReplicationState(mRocksdb, state);
  if (ret < 0)
  {
    log_error("update replication state failed!");
    return -1;
  }

  // close the connection
  hangup_notify();

  log_error("do rocksdb fully replication finished!");

  return 0;
}

// query data in range of (startKey, endKey]
int RocksReplicationChannel::masterFillRangeKV(
    std::string& startKey,
    std::string& endKey)
{
  mPacketBuffer->resetElasticBuffer();
  
  // save header
  char* pos = mPacketBuffer->getWritingPos(sizeof(ReplicationPacket_t), true);
  memmove((void*)pos, (void*)&mPacketHeader, sizeof(ReplicationPacket_t));
  mPacketBuffer->drawingWritingPos(sizeof(ReplicationPacket_t));
  
  // save replication end key
  int ret;
  bool finished = false;
  std::string value;
  RocksDBConn::RocksItr_t rocksItr;
  if (endKey.empty())
  {
    // get end key from rocksdb
    ret = mRocksdb->get_last_entry(endKey, value, rocksItr);
    if (ret < 0)
    {
      log_error("get the start replication key from rocksdb failed!");
      return -1;
    }
    
    mPacketBuffer->appendStrValue(endKey);
  }
  
  if (endKey.empty())
  {
    // no data in storage
    finished = true;
    goto RANGE_QUERY_END;
  }
  
  ret = mRocksdb->retrieve_start(startKey, value, rocksItr, true);
  do {
    if (ret < 0)
    {  
      log_error("query rocksdb failed! key:%s, ret:%d", startKey.c_str(), ret);
      mRocksdb->retrieve_end(rocksItr);
      return -1;
    }
    else if (ret == 1)
    {
      finished = true;
      log_info("no matched key:%s", startKey.c_str());
      break;
    }
    
    // save kv
    ret = mPacketBuffer->appendStrValue(startKey);
    if (ret < 0)
    {
      log_error("append key failed! key:%s", startKey.c_str());
      return -1;
    }
    ret = mPacketBuffer->appendStrValue(value);
    if (ret < 0)
    {
      log_error("append value failed! value:%s", value.c_str());
      return -1;
    }
    
    // skip the range of fully replication
    if([&startKey, &endKey]() -> bool{
        return startKey.compare(endKey) >= 0;
        }())
    {
      finished = true;
      break;
    }

    ret = mRocksdb->next_entry(rocksItr, startKey, value);
  } while (mPacketBuffer->getBufferSize() < REPLICATION_PACKET_SIZE);
      
  mRocksdb->retrieve_end(rocksItr);
  
RANGE_QUERY_END:
  ReplicationPacket_t* respHeader = (ReplicationPacket_t*)(mPacketBuffer->getHeadBuffer()->sData);
  
  if (finished) SET_REQUEST_TYPE(respHeader->sPacketFlag, RocksdbReplication::eReplFin); 
  else SET_REQUEST_TYPE(respHeader->sPacketFlag, RocksdbReplication::eReplRepAck);

  respHeader->sRawPacketLen = mPacketBuffer->getBufferSize();
  
  return 0;
}

int RocksReplicationChannel::slaveConstructRequest(RocksdbReplication::ReplicationType rType)
{
  // build replication sync request
  CLEAR_BITS(mPacketHeader.sPacketFlag);
  
  switch (rType)
  {
  case  RocksdbReplication::eReplSync:
    SET_REQUEST_TYPE(mPacketHeader.sPacketFlag, RocksdbReplication::eReplSync);
    break;
  case RocksdbReplication::eReplReqAck:
    SET_REQUEST_TYPE(mPacketHeader.sPacketFlag, RocksdbReplication::eReplReqAck);
    break;
  default:
    log_error("unsupport request type:%d", rType);
    return -1;
  }
  
  if (!mReplStartKey.empty()) SET_START_KEY(mPacketHeader.sPacketFlag); 
  if (!mReplEndKey.empty()) SET_END_KEY(mPacketHeader.sPacketFlag); 

  mPacketBuffer->resetElasticBuffer();

  char* pos = mPacketBuffer->getWritingPos(sizeof(ReplicationPacket_t), true);
  if (!pos)
  {
    log_error("allocate space for raw data failed!");
    return -1;
  }
  memcpy((void*)pos, (void*)&mPacketHeader, sizeof(ReplicationPacket_t));
  mPacketBuffer->drawingWritingPos(sizeof(ReplicationPacket_t));
  
  int ret;
  // padding replication key
  if (!mReplStartKey.empty())
  {
    ret = mPacketBuffer->appendStrValue(mReplStartKey);
    if (ret < 0)
    {
      log_error("append start key failed! key:%s", mReplStartKey.data());
      return -1;
    }
  }

  if (!mReplEndKey.empty())
  {
    ret = mPacketBuffer->appendStrValue(mReplEndKey);
    if (ret < 0)
    {
      log_error("append end key failed! key:%s", mReplEndKey.data());
      return -1;
    }
  }
  
  // set packet len
  ReplicationPacket_t* header = (ReplicationPacket_t*)mPacketBuffer->getHeadBuffer()->sData;
  header->sRawPacketLen = mPacketBuffer->getBufferSize();

  return 0;
}

// save kv those replicating from master
int RocksReplicationChannel::slaveFillRangeKV()
{
  int ret;
  std::string key, value;
  std::map<std::string, std::string> newEntries;
  while (true)
  {
    mReplStartKey = std::move(key);

    ret = mPacketBuffer->getStrValue(key);
    if (ret < 0)
    {
      log_error("get key from elastic buffer failed!"); 
      return -1;
    }
    else if (1 == ret) break;

    ret = mPacketBuffer->getStrValue(value);
    if (ret != 0)
    {
      log_error("get value from elsatic buffer failed! key:%s", key.c_str());
      return -1;
    }

    newEntries[key] = value;
    
    std::string dKey, dVal;
    extern HelperProcessBase* helperProc;
    RocksdbProcess* p_rocksdb_process = dynamic_cast<RocksdbProcess*>(helperProc);
    if (p_rocksdb_process != NULL){
      ret = p_rocksdb_process->decodeInternalKV(
        key, value, dKey, dVal);
    }
    
    log_error("replication kv, :%s,%s", dKey.c_str(), dVal.c_str());
  }

  ret = mRocksdb->batch_update(std::set<std::string>(), newEntries, true);
  if (ret !=0)
  {
    log_error("batch update replication keys failed!");
    return -1;
  }

  return 0;
}

int RocksReplicationChannel::recieveReplicationData()
{
  // recieve header
  memset((void*)&mPacketHeader, 0, sizeof(ReplicationPacket_t));
  int dataLen = sizeof(ReplicationPacket_t);
  int ret = ReplicationUtil::recieveMessage(netfd, (char*)&mPacketHeader, dataLen);
  if (ret != dataLen)
  {
    log_error("blocking read error! readLen:%d", ret);
    return -1;
  }
  
  // recieve kv
  mPacketBuffer->resetElasticBuffer();

  dataLen = mPacketHeader.sRawPacketLen - sizeof(ReplicationPacket_t);
  while (dataLen > 0)
  {
    int singleReadLen = dataLen > BUFFER_SIZE ? BUFFER_SIZE : dataLen;
    char* pos = mPacketBuffer->getWritingPos(singleReadLen);
    if (!pos)
    {
      log_error("allocate space for raw data failed!");
      return -1;
    }

    int ret = ReplicationUtil::recieveMessage(netfd, pos, singleReadLen);
    if (ret != singleReadLen)
    {
      log_error("blocking read error! readLen:%d", ret);
      return -1;
    }

    mPacketBuffer->drawingWritingPos(singleReadLen);
    dataLen -= singleReadLen;
  }

  return 0;
}

int RocksReplicationChannel::sendReplicationData()
{
  int ret;
  Buffer_t* cur = mPacketBuffer->getHeadBuffer();
  while (cur)
  {
    ret = ReplicationUtil::sendMessage(netfd, cur->sData, cur->sDataLen);
    if (ret != cur->sDataLen)
    {
      log_error("send replication data failed!");
      return -1;
    }
    
    cur = mPacketBuffer->nextBuffer(cur);
  }

  return 0;
}


/////////////////////////////////////////////////////////////////////
//
//            RocksdbReplication implementation
//
/////////////////////////////////////////////////////////////////////

RocksdbReplication::RocksdbReplication(RocksDBConn* rocksdb)
:
mRocksdb(rocksdb),
mGlobalReplicationThread(new PollThread("RocksdbReplicator"))
{
}

int RocksdbReplication::initializeReplication()
{
  // initialize thread
  assert(mGlobalReplicationThread);

  int ret = mGlobalReplicationThread->initialize_thread();
  if (ret < 0)
  {
    log_error("initialize thread poll failed.");
    return -1;
  }

  // running thread
  mGlobalReplicationThread->running_thread();

  ret = startMasterListener();
  if (ret < 0)
  {
    log_error("start listener thread failed.");
    return -1;
  }
  
  return 0;
}

// every rocksdb instance will listen on the replication port
int RocksdbReplication::startMasterListener()
{
  // initialize replication state
  int state = (int)ReplicationState::eReplicationNoOpr; 
  int ret = updateReplicationState(mRocksdb, state);
  if (ret < 0)
  {
    log_error("init replication state failed!");
    return -1;
  }

  // bind port
  int socketFd, masterPort;
  std::string masterIp = dtc::utils::get_local_ip();
  // std::string masterIp = "0.0.0.0"; 
  if (masterIp.empty())
  {
    log_error("get local ip failed!");
    return -1;
  }

  masterPort = gConfig->get_int_val("cache", "HelperReplPort", -1);
  // masterPort = 40411; 
  if (masterPort <= 0)
  {
    log_error("get table def failed!");
    return -1;
  }
  
  socketFd = ReplicationUtil::sockBind(masterIp, masterPort, 256);
  if (socketFd < 0)
  {
    log_error("bind addr to socket failed!");
    return -1;
  }

  log_error("rocksdb replication start to listen on addr:%s:%d, fd:%d", masterIp.c_str(), masterPort, socketFd);

  RocksReplicationChannel* listener = new RocksReplicationChannel(RocksReplicationChannel::eReplListener, mRocksdb, mGlobalReplicationThread, socketFd);
  if (!listener)
  {
    close(socketFd);
    log_error("start master replication failed!");
    return -RocksdbReplication::eStartMasterFailed;
  }
  
  ret = listener->attachThread();
  if (ret < 0)
  {
    delete listener;
    log_error("attach thread failed! fd:%d", socketFd);
    return -RocksdbReplication::eStartMasterFailed;
  }
  //while (true)
  {
  //  listener->handleAccept(); 
  //  sleep(7);
  }
  return 0;
}

int RocksdbReplication::startSlaveReplication(
    const std::string& masterIp,
    int masterPort)
{
  // create connection to the master and add it to the poll with blocking mode
  int fd = ReplicationUtil::connectServer(masterIp, masterPort);
  if (fd < 0) return -ReplicationErr::eConnectRefused;
  
  // create replication channel
  RocksReplicationChannel* channel = new RocksReplicationChannel(RocksReplicationChannel::eReplChannel, mRocksdb, mGlobalReplicationThread, fd);
  if (!channel)
  {
    close(fd);
    log_error("create replication channel failed! addr:%s:%d",  masterIp.c_str(), masterPort);
    return -RocksdbReplication::eStartMasterFailed;
  }
  
  int ret = channel->attachThread();
  if (ret < 0)
  {
    delete channel;
    log_error("attach thread failed! fd:%d", fd);
    return -RocksdbReplication::eStartMasterFailed;
  }
  
  log_error("create replication channel success, fd:%d", fd);

  return channel->triggerReplication();
}

int RocksdbReplication::getReplicationState()
{
  return ::getReplicationState(mRocksdb);
}
