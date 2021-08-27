
/*
 * =====================================================================================
 *
 *       Filename:  rocksdb_direct_worker.cc 
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhuyao, zhuyao28@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */

#include "rocksdb_direct_worker.h"
#include "db_process_base.h"
#include "poll_thread.h"
#include "log.h"

#include <algorithm>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

RocksdbDirectWorker::RocksdbDirectWorker(
    HelperProcessBase* processor,
    PollThread* poll,
    int fd)
:
PollerObject(poll, fd)
,mDBProcessRocks(processor)
,range_query_row_()
{
}

RocksdbDirectWorker::~RocksdbDirectWorker()
{
}

int RocksdbDirectWorker::add_event_to_poll()
{
  enable_input();
  int ret = PollerObject::attach_poller();
  if (ret < 0)
  {
    log_error("add event to poll failed.");
    return -1;
  }
  
  log_info("add rocksdb worker to event poll successful, fd:%d", netfd);

  return 0;
}

int RocksdbDirectWorker::remove_from_event_poll()
{
  PollerObject::detach_poller();
  
  log_error("delete rocksdb direct worker from poll successful, fd:%d", netfd);
  
  delete this;

  return 0;
}

// because not thinking about the split or delay of data package, so if
// recieving unexpected message, just close the socket. the client will
// reconnect during the next invoking
void RocksdbDirectWorker::input_notify()
{
  int ret = recieve_request();
  if ( ret != 0 )
  {
    remove_from_event_poll();
    return;
  }
   
  proc_direct_request();
  
  send_response();

  return;
}

void RocksdbDirectWorker::proc_direct_request()
{
  switch ((DirectRequestType)mRequestContext.sDirectQueryType)
  {
  case DirectRequestType::eRangeQuery:
    procDirectRangeQuery();
    break;
  case DirectRequestType::eReplicationRegistry:
    procReplicationRegistry();
    break;
  case DirectRequestType::eReplicationStateQuery:
    procReplicationStateQuery();
    break;
  default:
    log_error("unrecognize request type:%d", mRequestContext.sDirectQueryType);
  }
  
  return;
}

void RocksdbDirectWorker::procDirectRangeQuery()
{
  // priority of query conditions
  RangeQuery_t* rQueryConds = (RangeQuery_t*)mRequestContext.sPacketValue.uRangeQuery;
  std::sort(rQueryConds->sFieldConds.begin(), rQueryConds->sFieldConds.end(),
      [this](const QueryCond& cond1, const QueryCond& cond2) ->bool { 
        return cond1.sFieldIndex < cond2.sFieldIndex || 
          (cond1.sFieldIndex == cond2.sFieldIndex && 
           condtion_priority((CondOpr)cond1.sCondOpr, (CondOpr)cond2.sCondOpr)); });
  assert(rQueryConds->sFieldConds[0].sFieldIndex == 0 );

  mResponseContext.sDirectRespValue.uRangeQueryRows = (uint64_t)(&range_query_row_);
  mDBProcessRocks->process_direct_query(&mRequestContext, &mResponseContext);
  return;
}

void RocksdbDirectWorker::procReplicationRegistry()
{
  ReplicationQuery_t* registryCond = (ReplicationQuery_t*)mRequestContext.sPacketValue.uReplicationQuery;
  int ret = mDBProcessRocks->TriggerReplication(registryCond->sMasterIp, registryCond->sMasterPort);
  mResponseContext.sRowNums = 0;
  mResponseContext.sDirectRespValue.uReplicationState = ret;
  return;
}

void RocksdbDirectWorker::procReplicationStateQuery()
{
  int ret = mDBProcessRocks->QueryReplicationState();
  mResponseContext.sRowNums = 0;
  mResponseContext.sDirectRespValue.uReplicationState = ret;
  return;
}
int RocksdbDirectWorker::recieve_request()
{
  static const int maxContextLen = 16 << 10; // 16k
  static char dataBuffer[maxContextLen];
  
  mRequestContext.reset();
  
  int dataLen;
  int ret = recieve_message((char*)&dataLen, sizeof(dataLen));
  if ( ret != 0 ) return -1;
  assert( dataLen <= maxContextLen );
  
  ret = recieve_message(dataBuffer, dataLen);
  if ( ret != 0 ) return -1;
  
  mRequestContext.serialize_from(dataBuffer, dataLen);
  
  // // priority of query conditions
  // RangeQuery_t* rQueryConds = (RangeQuery_t*)mRequestContext.sPacketValue.uRangeQuery;
  // std::sort(rQueryConds->sFieldConds.begin(), rQueryConds->sFieldConds.end(),
  //    [this](const QueryCond& cond1, const QueryCond& cond2) ->bool { 
  //       return cond1.sFieldIndex < cond2.sFieldIndex || 
  //         (cond1.sFieldIndex == cond2.sFieldIndex && 
  //          condtion_priority((CondOpr)cond1.sCondOpr, (CondOpr)cond2.sCondOpr)); });
  // assert(rQueryConds->sFieldConds[0].sFieldIndex == 0 );

  return 0;
}

void RocksdbDirectWorker::send_response()
{
  mResponseContext.sMagicNum = mRequestContext.sMagicNum;
  mResponseContext.sSequenceId = mRequestContext.sSequenceId;

  std::string rowValue;
  mResponseContext.serialize_to((DirectRequestType)mRequestContext.sDirectQueryType,rowValue);
  
  int valueLen = rowValue.length();
  int ret = send_message((char*)&valueLen, sizeof(int));
  if ( ret < 0 )
  {
    log_error("send response failed, close the connection. netfd:%d, sequenceId:%" PRIu64, netfd, mResponseContext.sSequenceId);
    remove_from_event_poll();
    return;
  }
  
  // send row value
  ret = send_message(rowValue.c_str(), valueLen);
  if ( ret < 0 )
  {
    log_error("send response failed, close the connection. netfd:%d, sequenceId:%" PRIu64, netfd, mResponseContext.sSequenceId);
    remove_from_event_poll();
    return;
  }

  log_info("send response successful. netfd:%d, sequenceId:%" PRIu64, netfd, mResponseContext.sSequenceId);
  
  mResponseContext.free((DirectRequestType)mRequestContext.sDirectQueryType);

  return;
}

int RocksdbDirectWorker::recieve_message(
    char* data,
    int dataLen)
{
  int readNum = 0;
  int nRead = 0;
  int ret = 0;
  do {
    ret = read(netfd, data + nRead, dataLen - nRead);
    if ( ret > 0 )
    {
      nRead += ret;
    }
    else if ( ret == 0 )
    {
      // close the connection
      log_error("peer close the socket, fd:%d", netfd);
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
        log_error("read socket failed, fd:%d, errno:%d", netfd, errno);
        return -1;
      }
    }
  } while ( nRead < dataLen );

  return 0;
}

int RocksdbDirectWorker::send_message(
    const char* data,
    int dataLen)
{
  int sendNum = 0;
  int nWrite = 0;
  int ret = 0;
  do {
    ret = write(netfd, data + nWrite, dataLen - nWrite);
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
        log_error("write socket failed, fd:%d, errno:%d", netfd, errno);
        return -1;
      }
    }
    else
    {
      if ( dataLen == 0 ) return 0;

      log_error("unexpected error!!!!!!!, fd:%d, errno:%d", netfd, errno);
      return -1;
    }
  } while ( nWrite < dataLen );

  return 0;
}

// judge the query condition priority those with the same field index
// (==) > (>, >=) > (<, <=) > (!=)
bool RocksdbDirectWorker::condtion_priority(
    const CondOpr lc,
    const CondOpr rc)
{
  switch ( lc )
  {
    case CondOpr::eEQ:
      return true;
    case CondOpr::eNE:
      return false;
    case CondOpr::eLT:
    case CondOpr::eLE:
      if ( rc == CondOpr::eNE ) return true;
      return false;
    case CondOpr::eGT:
    case CondOpr::eGE:
      if ( rc == CondOpr::eEQ ) return false;
      return true;
    default:
      log_error("unkonwn condtion opr:%d", (int)lc);
  }

  return false;
}
