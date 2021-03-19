/*
 * =====================================================================================
 *
 *       Filename:  fence.h
 *
 *    Description:  fence class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __BARRIER_H__
#define __BARRIER_H__

#include <list.h>
#include <lqueue.h>

class TaskRequest;
class BarrierUnit;
class Barrier;

class Barrier : public ListObject<Barrier>,
				public LinkQueue<TaskRequest *>
{
public:
	friend class BarrierUnit;

	inline Barrier(LinkQueue<TaskRequest *>::allocator *a = NULL) : LinkQueue<TaskRequest *>(a), key(0)
	{
	}
	inline ~Barrier(){};

	inline unsigned long Key() const { return key; }
	inline void set_key(unsigned long k) { key = k; }

private:
	unsigned long key;
};

#endif
