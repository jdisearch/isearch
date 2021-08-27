/////////////////////////////////////////////////////////////
//
//  Elastic buffer for replication between slave and master
//      created by qiuyu
//        1/11/2021
//
/////////////////////////////////////////////////////////////

#include "elastic_buffer.h"
#include "log.h"

#include <string.h>
#include <assert.h>

ElasticBuffer::~ElasticBuffer()
{
  while (mHeadBuffer)
  {
    mWritingBuffer = mHeadBuffer;
    mHeadBuffer = mHeadBuffer->sNext;
    delete mWritingBuffer;
  }
}

int ElasticBuffer::appendStrValue(const std::string& value)
{
  // encode value datalen, now use fixed int
  int dataLen = sizeof(int); 
  char* writePos = getWritingPos(dataLen, true);
  if (!writePos) return -1;
  
  dataLen = value.length();
  *(int*)writePos = dataLen;
  writePos = drawingWritingPos(sizeof(int));

  writePos = getWritingPos(dataLen);
  if (!writePos) return -1;
  
  int curLen;
  while (dataLen > 0)
  {
    curLen = BUFFER_SIZE - mWritingBuffer->sDataLen;
    curLen = curLen > dataLen ? dataLen : curLen;
    memcpy((void*)writePos, (void*)value.data(), curLen);
    drawingWritingPos(curLen);
    dataLen -= curLen;
  }

  return 0;
}

int ElasticBuffer::getStrValue(std::string& value)
{
  value.clear();

  bool bRet = [this]()->bool {
    // check whether has enough reading space
    return (NULL == mReadingBuffer->sNext && mReadLen == mReadingBuffer->sDataLen); 
  }();
  if (bRet) return 1;

  // need to handle atomic value split in the tail of buffer --- TCP sticky packet
  int ret, dataLen;
  char* rPos = getReadingPos();
  if (mReadLen + sizeof(int) > BUFFER_SIZE)
  {
    // skip to the next buffer
    rPos = drawingReadingPos(BUFFER_SIZE - mReadLen);
  }
  dataLen = *(int*)rPos;
  assert(dataLen >= 0);
  rPos = drawingReadingPos(sizeof(int));

  // if (unlikely(0 == dataLen))
  if (0 == dataLen)
  {
    value = "";
  }
  else
  {
    // assign string value
    ret = assignStrValue(value, dataLen);
    // assert(ret > 0);
  }
  
  drawingReadingPos(dataLen);

  return 0;
}

// make sure enough space for store 'dataLen' raw contents after the following
// write
char* ElasticBuffer::getWritingPos(int dataLen, bool atomicLen)
{
  int ret = expandElasticBuffer(dataLen);
  if (ret < 0) return NULL;
  
  // if the remaining space is not enough for storing the atomic value,
  // skip to the next buffer
  if (atomicLen && mWritingBuffer->sDataLen + dataLen > BUFFER_SIZE)
  {
    mWritingBuffer = mWritingBuffer->sNext;
  }

  return mWritingBuffer->sData + mWritingBuffer->sDataLen;
}
    
char* ElasticBuffer::drawingWritingPos(int dataLen) 
{ 
  int skipLen = mWritingBuffer->sDataLen + dataLen;
  while (skipLen >= BUFFER_SIZE)
  {
    assert(mWritingBuffer);
    skipLen -= BUFFER_SIZE;
    mWritingBuffer = mWritingBuffer->sNext;
  }
  
  if (!mWritingBuffer) return NULL;

  mWritingBuffer->sDataLen = skipLen;
  return mWritingBuffer->sData + mWritingBuffer->sDataLen;
}

char* ElasticBuffer::drawingReadingPos(int dataLen)
{
  mReadLen += dataLen;
  if (mReadLen < BUFFER_SIZE) return mReadingBuffer->sData + mReadLen;

  while (mReadLen >= BUFFER_SIZE)
  {
    assert(mReadingBuffer);
    mReadLen -= BUFFER_SIZE;
    mReadingBuffer = mReadingBuffer->sNext;
  }

  return mReadingBuffer ? mReadingBuffer->sData + mReadLen : NULL;
}

// for binary stream copy 
int ElasticBuffer::assignStrValue(
    std::string& value,
    int dataLen)
{
  // assign length of memory from reading pos
  int readLen = mReadLen;
  int skipLen = mReadLen + dataLen;
  char* readPos = getReadingPos(); 
  if (skipLen <= BUFFER_SIZE)
  {
    value.append((const char*)readPos, dataLen);
    return 0;
  }

  Buffer_t* curBuffer = mReadingBuffer; 
  while (skipLen > BUFFER_SIZE)
  {
    assert(readPos);
    value.append((const char*)readPos, BUFFER_SIZE - readLen);
    readLen = 0;
    skipLen -= BUFFER_SIZE;
    curBuffer = nextBuffer(curBuffer);
    readPos = curBuffer ? curBuffer->sData : NULL;
  }
  
  assert(readPos);
  value.append((const char*)readPos, skipLen);
  return 0;
}

int ElasticBuffer::expandElasticBuffer(int dataLen)
{
  // has enough remaining space
  if (mWritingBuffer && BUFFER_SIZE - mWritingBuffer->sDataLen >= dataLen) return 0;
  
  // if the single key is too large, just report error
  if (dataLen > BUFFER_SIZE)
  {
    log_error("too large size for one key, dataLen:%d", dataLen);
  }
  
  Buffer_t* curBuffer = mWritingBuffer;
  int totalLen = (curBuffer ?  curBuffer->sDataLen : 0) + dataLen;
  while (totalLen > BUFFER_SIZE || !mHeadBuffer)
  {
    Buffer_t * nBuffer = []() ->Buffer_t* {
      Buffer_t * newBuffer = new Buffer_t();
      if (!newBuffer) return NULL;
      newBuffer->sDataLen = 0;
      newBuffer->sNext = NULL;
      newBuffer->sData = new char[BUFFER_SIZE];
      return newBuffer;
    }();
    
    if (!nBuffer || !nBuffer->sData)
    {
      log_error("create buffer unit failed, errno:%d", errno);
      return -1;
    }
    
    mCacheBufferNum++;

    // if (unlikely(!mHeadBuffer))
    if (!mHeadBuffer)
    {
      mHeadBuffer = mReadingBuffer = mWritingBuffer = nBuffer;
    }
    else
    {
      curBuffer->sNext = nBuffer;
    }
    curBuffer = nBuffer;
    
    totalLen -= BUFFER_SIZE; 
  }
  
  return 0;
}

char* ElasticBuffer::encodeLength(char *p, uint32_t len) 
{
	if(len < 240) 
  {
	    p[0] = len;
	    return p+1;
	} 
  else if(len < (13<<8)) 
  {
	    p[0] = 0xF0 + (len>>8);
	    p[1] = len & 0xFF;
	    return p+2;
	} 
  else if(len < (1<<16)) 
  {
	    p[0] = 253;
	    p[1] = len >> 8;
	    p[2] = len & 0xFF;
	    return p+3;
	} 
  else if(len < (1<<24)) 
  {
	    p[0] = 254;
	    p[1] = len >> 16;
	    p[2] = len >> 8;
	    p[3] = len & 0xFF;
	    return p+4;
	} 
  else 
  {
	    p[0] = 255;
	    p[1] = len >> 24;
	    p[2] = len >> 16;
	    p[3] = len >> 8;
	    p[4] = len & 0xFF;
	    return p+5;
	}
	return 0;
}

int ElasticBuffer::decodeLength(
    char* &cur, 
    int pLen,
    uint32_t &len) 
{
  int ret = -1;
	
  len = *cur++;
  pLen--;
	if (len < 240) 
  {
	} 
  else if(len <= 252) 
  {
    if(!cur) goto ReportError;
    len = ((len & 0xF)<<8) + *cur++;
    pLen--;
	} 
  else if(len == 253) 
  {
	    if (pLen < 2) goto ReportError;
	    len = (cur[0]<<8) + cur[1];
	    cur += 2;
      pLen -= 2;
	} 
  else if(len == 254) 
  {
	    if (pLen < 3) goto ReportError;
	    len = (cur[0]<<16) + (cur[1]<<8) + cur[2];
	    cur += 3;
      pLen -= 3;
	} 
  else 
  {
	    if (pLen < 4) goto ReportError;
	    len = (cur[0]<<24) + (cur[1]<<16) + (cur[2]<<8) + cur[3];
	    cur += 4;
      pLen -= 4;
	    if (len > (64<<20))
      {
        log_error("too large packet, len:%d", len);
	    	return -1;
      }
	}
	ret = 0;

  if (ret != 0)
  {
ReportError:
    log_error("invalid data len when decode!");
  }
  return ret;
}
