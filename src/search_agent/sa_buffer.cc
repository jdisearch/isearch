#include "sa_buffer.h"
#include <string.h>
#include "log.h"

MessageBuffer::MessageBuffer()
{
    m_pBuffer = NULL;
}

bool MessageBuffer::Init(int iBufferSize )
{
    m_pBuffer = new char[iBufferSize];
    if (m_pBuffer == NULL)
    {
        return false;
    }
    m_iWritePosition = 0;
    m_iReadPosition = 0;
    m_iBufferSize = iBufferSize;
    return true;
}

uint32_t MessageBuffer::BufferSize()
{
    return m_iBufferSize - m_iWritePosition;
}

char *MessageBuffer::WritePos()
{
    return &m_pBuffer[m_iWritePosition];
}

char *MessageBuffer::ReadPos()
{
    return &m_pBuffer[m_iReadPosition];
}

uint32_t MessageBuffer::UnReadSize()
{
    return m_iWritePosition - m_iReadPosition;
}

bool MessageBuffer::Enlarge()
{
    char *tempBuffer = new char[m_iBufferSize * 2];
    if (tempBuffer == NULL)
    {
        return false;
    }
    memcpy(tempBuffer, m_pBuffer, m_iBufferSize);
    delete[] m_pBuffer;
    m_pBuffer = tempBuffer;
    m_iBufferSize *= 2;
    return true;
}

bool MessageBuffer::isEmpty()
{
    return (m_iWritePosition <= m_iReadPosition);
}

bool MessageBuffer::isFull()
{
    return (m_iWritePosition == m_iBufferSize );
}

void MessageBuffer::IncrReadPosition(uint32_t iReadPosition)
{
    m_iReadPosition += iReadPosition;
}

void MessageBuffer::IncrWritePosition(uint32_t iWritePosition)
{
    m_iWritePosition += iWritePosition;
}

MessageBuffer::~MessageBuffer()
{
    if (m_pBuffer != NULL)
    {
        delete[] m_pBuffer;
        m_pBuffer = NULL;
    }
}
void MessageBuffer::SetWritePosition(uint32_t iWritePostion)
{
    m_iWritePosition = iWritePostion;
}

MessageBuffer *MessageBuffer::Split()
{
    log_debug("MessageBuffer::%s, split buffer read pos %u, write pos %u", __FUNCTION__, m_iReadPosition, m_iWritePosition );
    MessageBuffer *pBuffer = new MessageBuffer();
    if (pBuffer == NULL)
    {
        return NULL;
    }
    if (!pBuffer->Init(m_iBufferSize))
    {
        delete pBuffer;
        return NULL;
    }
    memcpy(pBuffer->WritePos(), &m_pBuffer[m_iReadPosition], m_iWritePosition-m_iReadPosition);
    pBuffer->SetWritePosition(m_iWritePosition - m_iReadPosition);
    return pBuffer;
}
