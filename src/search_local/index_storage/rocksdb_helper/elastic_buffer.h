/////////////////////////////////////////////////////////////
//
//  Elastic buffer for replication between slave and master
//      created by qiuyu
//        1/11/2021
//
/////////////////////////////////////////////////////////////

#ifndef __ELASTIC_BUFFER_H__
#define __ELASTIC_BUFFER_H__

#include <string>

// 1M for each buffer
#define BUFFER_SIZE  (1 << 20)

typedef struct Buffer
{
  unsigned int sDataLen;
  struct Buffer* sNext;
  char* sData;

  ~Buffer()
  {
    if (sData) delete sData;
    sNext= NULL;
    sData = NULL;
  }
}Buffer_t;

class ElasticBuffer
{
  private:
    Buffer_t* mHeadBuffer;
    Buffer_t* mWritingBuffer;
    Buffer_t* mReadingBuffer;
    int mReadLen;
    int mCacheBufferNum;

  public:
    ElasticBuffer()
    :
    mHeadBuffer(NULL),
    mWritingBuffer(NULL),
    mReadingBuffer(NULL),
    mReadLen(0),
    mCacheBufferNum(0)
    {
    }

    virtual ~ElasticBuffer();

    int appendStrValue(const std::string& value);
    int getStrValue(std::string& value);

    Buffer_t* getHeadBuffer() { return mHeadBuffer; }
    Buffer_t* nextBuffer(const Buffer_t* curr) { return curr->sNext; }
    void resetElasticBuffer()
    {
      mWritingBuffer = mHeadBuffer;
      while (mWritingBuffer)
      {
        mWritingBuffer->sDataLen = 0;
        mWritingBuffer = mWritingBuffer->sNext;
      }
      
      mReadingBuffer = mWritingBuffer = mHeadBuffer;
    }
    
    // raw data batch copy
    char* getWritingPos(int dataLen = 0, bool atomicLen = false);
    char* drawingWritingPos(int dataLen);
    
    char* getReadingPos() { return mReadingBuffer->sData + mReadLen; }
    char* drawingReadingPos(int dataLen);
    
    // use fixed buffer size with previours buffer unit to prevent TCP sticky 
    int getBufferSize() { 
      return mCacheBufferNum > 0 ? 
        (BUFFER_SIZE * (mCacheBufferNum - 1) + mWritingBuffer->sDataLen) : 0; }

  private:
    int assignStrValue(
        std::string& value,
        int dataLen);

    int expandElasticBuffer(int dataLen);

    char* encodeLength(
        char *p, 
        uint32_t len);
    
    int decodeLength(
        char* &cur, 
        int pLen,
        uint32_t &len); 
};

#endif // __ELASTIC_BUFFER_H__
