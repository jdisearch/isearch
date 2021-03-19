/*
 * =====================================================================================
 *
 *       Filename:  lock_free_queue.h
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
#ifndef LOCK_FREE_QUEUE_H
#define LOCK_FREE_QUEUE_H

#include <stdint.h>

#define LOCK_FREE_QUEUE_DEFAULT_SIZE 65536
#define CAS(a_ptr, a_oldVal, a_newVal) __sync_bool_compare_and_swap(a_ptr, a_oldVal, a_newVal)

template <typename ELEM_T, uint32_t Q_SIZE = LOCK_FREE_QUEUE_DEFAULT_SIZE>
class LockFreeQueue
{
public:
	LockFreeQueue();
	~LockFreeQueue();
	bool en_queue(const ELEM_T &data);
	bool de_queue(ELEM_T &data);
	uint32_t Size();
	uint32_t queue_size();

private:
	ELEM_T *m_theQueue;
	uint32_t m_queueSize;
	volatile uint32_t m_readIndex;
	volatile uint32_t m_writeIndex;
	volatile uint32_t m_maximumReadIndex;
	inline uint32_t count_to_index(uint32_t count);
};

#include "LockFreeQueue_Imp.h"

#endif
