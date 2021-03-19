/*
 * =====================================================================================
 *
 *       Filename:  lock_free_queue_imp.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include "LockFreeQueue.h"
#include <sched.h>

template <typename ELEM_T, uint32_t Q_SIZE>
LockFreeQueue<ELEM_T, Q_SIZE>::LockFreeQueue() : m_readIndex(0),
												 m_writeIndex(0),
												 m_maximumReadIndex(0)
{

	m_queueSize = 65536;
	if (Q_SIZE > 65536)
	{
		do
		{
			m_queueSize = m_queueSize << 1;
		} while (m_queueSize < Q_SIZE);
		m_queueSize = m_queueSize >> 1;
	}
	m_theQueue = new ELEM_T[m_queueSize];
}

template <typename ELEM_T, uint32_t Q_SIZE>
LockFreeQueue<ELEM_T, Q_SIZE>::~LockFreeQueue()
{
	delete[] m_theQueue;
}

template <typename ELEM_T, uint32_t Q_SIZE>
uint32_t LockFreeQueue<ELEM_T, Q_SIZE>::count_to_index(uint32_t count)
{
	return (count % m_queueSize);
}

template <typename ELEM_T, uint32_t Q_SIZE>
uint32_t LockFreeQueue<ELEM_T, Q_SIZE>::Size()
{
	uint32_t m_currentWriteIndex = m_writeIndex;
	uint32_t m_currentReadIndex = m_readIndex;
	if (m_currentWriteIndex >= m_currentReadIndex)
	{
		return (m_currentWriteIndex - m_currentReadIndex);
	}
	else
	{
		return (m_queueSize + m_currentWriteIndex - m_currentReadIndex);
	}
}

template <typename ELEM_T, uint32_t Q_SIZE>
uint32_t LockFreeQueue<ELEM_T, Q_SIZE>::queue_size()
{
	return m_queueSize;
}

template <typename ELEM_T, uint32_t Q_SIZE>
bool LockFreeQueue<ELEM_T, Q_SIZE>::en_queue(const ELEM_T &data)
{
	uint32_t currentReadIndex;
	uint32_t currentWriteIndex;
	do
	{
		currentReadIndex = m_readIndex;
		currentWriteIndex = m_writeIndex;
		if (count_to_index(currentWriteIndex + 1) == count_to_index(currentReadIndex))
			return false;
	} while (false == CAS(&m_writeIndex, currentWriteIndex, currentWriteIndex + 1));
	m_theQueue[count_to_index(currentWriteIndex)] = data;
	while (false == CAS(&m_maximumReadIndex, currentWriteIndex, currentWriteIndex + 1))
	{
		sched_yield();
	}
	return true;
}

template <typename ELEM_T, uint32_t Q_SIZE>
bool LockFreeQueue<ELEM_T, Q_SIZE>::de_queue(ELEM_T &data)
{
	uint32_t currentMaximumReadIndex;
	uint32_t currentReadIndex;
	do
	{
		currentMaximumReadIndex = m_maximumReadIndex;
		currentReadIndex = m_readIndex;
		if (count_to_index(currentMaximumReadIndex) == count_to_index(currentReadIndex))
			return false;
		data = m_theQueue[count_to_index(currentReadIndex)];
		if (true == CAS(&m_readIndex, currentReadIndex, currentReadIndex + 1))
			return true;
	} while (1);
}
