#ifndef __SA_BUFFER_H__
#define __SA_BUFFER_H__
#include <malloc.h>
#include "stdint.h"

#define MBUF_SIZE 16384


class MessageBuffer
{
public:
    MessageBuffer();
    bool Init(int iBufferSize = MBUF_SIZE);
    uint32_t BufferSize();
    char *WritePos();
    char *ReadPos();
    uint32_t UnReadSize();
    bool Enlarge();
    bool isEmpty();
    bool isFull();
    void IncrReadPosition(uint32_t iReadPosition);
    void IncrWritePosition(uint32_t iWritePosition);
    ~MessageBuffer();
    void SetWritePosition(uint32_t iWritePostion);
    MessageBuffer *Split();

public:
    char *m_pBuffer;
    uint32_t m_iBufferSize;
    uint32_t m_iWritePosition;
    uint32_t m_iReadPosition;
};

#endif