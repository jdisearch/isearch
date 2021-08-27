
/*
 * =====================================================================================
 *
 *       Filename:  rocksdb_direct_context.h 
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
#ifndef __ROCKSDB_DIRECT_CONTEXT_H__
#define __ROCKSDB_DIRECT_CONTEXT_H__

#if 1

#include <stdint.h>
#include <string>
#include <vector>
#include <deque>
#include <assert.h>
#include <string.h>

static const uint16_t sgMagicNum = 12345; // global magic number

enum class ReplicationState : uint8_t
{
  eReplicationNoOpr = 0,      // do nothing 
  eReplicationFailed,     // error occur during replication
  eReplicationRegistry,
  eReplicationFullSync,
  eReplicationFinished,
  
  eReplicationMax
};

enum class DirectRequestType : uint8_t
{
  eInvalid = 0,
  eRangeQuery,
  eReplicationRegistry,
  eReplicationStateQuery
};
// operator must be matched with DTC with the same order
enum class CondOpr : uint8_t
{
  eEQ, // ==   0
  eNE, // !=   1
  eLT, // <    2
  eLE, // <=   3
  eGT, // >    4
  eGE  // >=    5
};

//bool operator==(const CondOpr lc, const CondOpr rc)
//{
//  return (int)lc == (int)rc;
//}

#pragma pack(push, 1)
struct QueryCond
{
  uint8_t sFieldIndex;
  uint8_t sCondOpr; // CondOpr
  // uint8_t sCondValueLen;
  // char sCondValue[];
  std::string sCondValue;

private:
  int binary_size()
  {
    static int fixHeaderLen = sizeof(sFieldIndex) + sizeof(sCondOpr) + sizeof(int) /* value len */;
    return fixHeaderLen + sCondValue.length();
  }

  void serialize_to(char *&data)
  {
    *(uint8_t *)data = sFieldIndex;
    data += sizeof(uint8_t);

    *(uint8_t *)data = sCondOpr;
    data += sizeof(uint8_t);

    *(int *)data = sCondValue.length();
    data += sizeof(int);

    memmove((void *)data, (void *)sCondValue.c_str(), sCondValue.length());
    data += sCondValue.length();
  }

  void serialize_from(const char *data, int &condLen)
  {
    const char *begPos = data;

    sFieldIndex = *(uint8_t *)data;
    data += sizeof(uint8_t);

    sCondOpr = *(uint8_t *)data;
    data += sizeof(uint8_t);

    int len = *(int *)data;
    data += sizeof(int);

    sCondValue.assign(data, len);
    condLen = data - begPos + len;
  }

  friend class DirectRequestContext;
};

struct LimitCond
{
  int sLimitStart = -1;
  int sLimitStep = -1;

  void reset() { sLimitStart = -1, sLimitStep = -1; }
};

typedef struct {
  std::string sMasterIp;
  int sMasterPort;

  void reset() {
    sMasterIp.clear();
    sMasterPort = -1;
  }
} ReplicationQuery_t;
typedef struct {
  std::vector<QueryCond> sFieldConds;
  std::vector<std::pair<int, bool/* asc or not*/> > sOrderbyFields;
  LimitCond sLimitCond;

  void reset() {
    sFieldConds.clear();
    sOrderbyFields.clear();
    sLimitCond.reset();
  }
} RangeQuery_t;
struct DirectRequestContext
{
  uint16_t sMagicNum;
  uint64_t sSequenceId;
  uint8_t sDirectQueryType;
  // uint8_t sCondValueNum;
  // char sContextValue[];
  union {
    uint64_t uPlaceHolder; // useless now
    uint64_t uReplicationQuery; // actually pointer of ReplicationQuery_t*
    uint64_t uRangeQuery;       // actually pointer of RangeQuery_t*
  } sPacketValue;
  
  void reset()
  {
    sMagicNum = 0;
    sSequenceId = 0;
    switch ((DirectRequestType)sDirectQueryType)
    {
    case DirectRequestType::eRangeQuery:
        ((RangeQuery_t*)(sPacketValue.uRangeQuery))->reset();
        break;
    case DirectRequestType::eReplicationRegistry:
        ((ReplicationQuery_t*)(sPacketValue.uReplicationQuery))->reset();
        break;
    case DirectRequestType::eReplicationStateQuery:
    default:
        sPacketValue.uPlaceHolder = 0;
    }
    sDirectQueryType = (uint8_t)DirectRequestType::eInvalid; 
  }
  
  // binary format size for transporting in
  int binary_size(DirectRequestType rType)
  {
    int fixedLen = sizeof(sMagicNum) + sizeof(sSequenceId) + sizeof(sDirectQueryType);
    switch (rType)
    {
    case DirectRequestType::eRangeQuery:
      {
        fixedLen += sizeof(uint8_t) * 2;
        std::vector<QueryCond>& sFieldConds = ((RangeQuery_t*)sPacketValue.uRangeQuery)->sFieldConds; 
        for ( size_t idx = 0; idx < sFieldConds.size(); idx++ )
        {
          fixedLen += sFieldConds[idx].binary_size();
        }
    
        std::vector<std::pair<int, bool> >& sOrderbyFields = ((RangeQuery_t*)sPacketValue.uRangeQuery)->sOrderbyFields;
        for (size_t idx = 0; idx < sOrderbyFields.size(); idx++)
        {
          fixedLen += (sizeof(int) + sizeof(bool));
        }
        fixedLen += sizeof(LimitCond);
      }
      break;
    case DirectRequestType::eReplicationRegistry:
      {
        fixedLen += sizeof(int);
        fixedLen += ((ReplicationQuery_t*)sPacketValue.uReplicationQuery)->sMasterIp.length();
        fixedLen += sizeof(int);
      }
      break;
    case DirectRequestType::eReplicationStateQuery:
      fixedLen += sizeof(sPacketValue.uPlaceHolder);
      break;
    default:
      return -1;
    }
    return fixedLen;
  }

  // before call this function, should call 'binary_size' to evaluate the size of the struct
  void serialize_to(char *data, int len)
  {
    *(uint16_t *)data = sMagicNum;
    data += sizeof(uint16_t);

    *(uint64_t *)data = sSequenceId;
    data += sizeof(uint64_t);
    
    *(uint8_t*)data = sDirectQueryType;
    data += sizeof(uint8_t);

    switch ((DirectRequestType)sDirectQueryType)
    {
    case DirectRequestType::eRangeQuery:
      {
        std::vector<QueryCond>& sFieldConds = ((RangeQuery_t*)sPacketValue.uRangeQuery)->sFieldConds;
        *(uint8_t*)data = sFieldConds.size();
        data += sizeof(uint8_t);
        for ( size_t idx = 0; idx < sFieldConds.size(); idx++ )
        {
          sFieldConds[idx].serialize_to(data);
        }
        
        std::vector<std::pair<int, bool> >& sOrderbyFields = ((RangeQuery_t*)sPacketValue.uRangeQuery)->sOrderbyFields;
        *(uint8_t*)data = sOrderbyFields.size();
        data += sizeof(uint8_t);
        for ( size_t idx = 0; idx < sOrderbyFields.size(); idx++ )
        {
          *(int*)data = sOrderbyFields[idx].first;
          data += sizeof(int);

      *(bool *)data = sOrderbyFields[idx].second;
      data += sizeof(bool);
    }

        memmove((void*)data, (void*)&((RangeQuery_t*)sPacketValue.uRangeQuery)->sLimitCond, sizeof(LimitCond));
        data += sizeof(LimitCond);
      }
      break;
    case DirectRequestType::eReplicationRegistry:
      {  
        ReplicationQuery_t* rt = (ReplicationQuery_t*)sPacketValue.uReplicationQuery;
        *(int*)data = rt->sMasterIp.length();
        data += sizeof(int);
        memcpy((void*)data, (void*)rt->sMasterIp.data(), rt->sMasterIp.length());
        data += rt->sMasterIp.length();
        *(int*)data = rt->sMasterPort;
        data += sizeof(int);
      }
      break;
    case DirectRequestType::eReplicationStateQuery:
      *(uint64_t*)data = sPacketValue.uPlaceHolder;
      break;
    default:
      assert(false);
    }
  }

  void serialize_from(const char *data, int dataLen)
  {
    sMagicNum = *(uint16_t *)data;
    data += sizeof(uint16_t);
    dataLen -= sizeof(uint16_t);

    sSequenceId = *(uint64_t *)data;
    data += sizeof(uint64_t);
    dataLen -= sizeof(uint64_t);
    
    sDirectQueryType = *(uint8_t*)data;
    data += sizeof(uint8_t);
    dataLen -= sizeof(uint8_t);

    switch ((DirectRequestType)sDirectQueryType)
    {
    case DirectRequestType::eRangeQuery:
      {
        RangeQuery_t* rpt = new RangeQuery_t();
        assert(rpt);
        sPacketValue.uRangeQuery = (uint64_t)rpt;
        uint8_t condNum = *(uint8_t*)data;
        data += sizeof(uint8_t);
        dataLen -= sizeof(uint8_t);

    QueryCond cond;
    int condLen = 0;
    for (uint8_t idx = 0; idx < condNum; idx++)
    {
      cond.serialize_from(data, condLen);
      data += condLen;
      dataLen -= condLen;

          ((RangeQuery_t*)sPacketValue.uRangeQuery)->sFieldConds.push_back(cond);
        }
    
        std::pair<int, bool> orPair;
        uint8_t orderNum = *(uint8_t*)data;
        data += sizeof(uint8_t);
        dataLen -= sizeof(uint8_t);
        for ( uint8_t idx = 0; idx < orderNum; idx++ )
        {
          orPair.first = *(int*)data;
          data += sizeof(int);
          dataLen -= sizeof(int);

      orPair.second = *(bool *)data;
      data += sizeof(bool);
      dataLen -= sizeof(bool);

          ((RangeQuery_t*)sPacketValue.uRangeQuery)->sOrderbyFields.push_back(orPair);
        }
    
        memmove((void*)&((RangeQuery_t*)sPacketValue.uRangeQuery)->sLimitCond, (void*)data, sizeof(LimitCond));
        dataLen -= sizeof(LimitCond);
      }
      break;
    case DirectRequestType::eReplicationRegistry:
      {
        ReplicationQuery_t* rpt = new ReplicationQuery_t();
        assert(rpt);
        
        int len = *(int*)data;
        rpt->sMasterIp.assign(data + sizeof(int), len);
        data += (sizeof(int) + len);
        dataLen -= (sizeof(int) + len);
        rpt->sMasterPort = *(int*)data;
        dataLen -= sizeof(int);

        sPacketValue.uReplicationQuery = (uint64_t)rpt;
      }
      break;
    case DirectRequestType::eReplicationStateQuery:
      {
        sPacketValue.uPlaceHolder = *(uint64_t*)data;
        dataLen -= sizeof(uint64_t);
      }
      break;
    default:
    assert( dataLen == 0 );
    }
  }
};

typedef struct {
  //int16_t sRowNums; // number of matched rows or errno
  std::deque<std::string> sRowValues;
  // char* sRowValues;
} RangeQueryRows_t;
struct DirectResponseContext
{
  uint16_t sMagicNum;
  uint64_t sSequenceId;
  int16_t sRowNums; 

  union {
    uint8_t uReplicationState;
    uint64_t uRangeQueryRows;
  } sDirectRespValue;
  
  void serialize_to(DirectRequestType rType, std::string& data)
  {
    int headerLen = sizeof(uint16_t) + sizeof(uint64_t) + sizeof(int16_t);
    data.clear();
    data = std::move(std::string((char*)this, headerLen));
    
    switch (rType)
    {
    case DirectRequestType::eRangeQuery:
      {
        for (size_t idx = 0; idx < (size_t)sRowNums; idx++)
        {
          data.append(((RangeQueryRows_t*)sDirectRespValue.uRangeQueryRows)->sRowValues.front());
          ((RangeQueryRows_t*)sDirectRespValue.uRangeQueryRows)->sRowValues.pop_front();
        }
      }
      break;
    case DirectRequestType::eReplicationRegistry:
    case DirectRequestType::eReplicationStateQuery:
      data.append(std::to_string(sDirectRespValue.uReplicationState));
      break;
    default:
      assert(false);
    }
  }
  
  void free(DirectRequestType requestType)
  {
    sMagicNum = 0;
    sSequenceId = 0;
    sRowNums = -1;
    
    switch (requestType)
    {
    case DirectRequestType::eRangeQuery:
      {
        RangeQueryRows_t* uRR = (RangeQueryRows_t*)sDirectRespValue.uRangeQueryRows;
        if (!uRR)
        {
          uRR->sRowValues.clear();
        }
      }
      break;
    case DirectRequestType::eReplicationRegistry:
    case DirectRequestType::eReplicationStateQuery:
      sDirectRespValue.uReplicationState = (uint8_t)ReplicationState::eReplicationFailed;
      break;
    default:
      assert(false);
    }
  }
};
#pragma pack(pop)

#endif

#endif // __ROCKSDB_DIRECT_CONTEXT_H__
