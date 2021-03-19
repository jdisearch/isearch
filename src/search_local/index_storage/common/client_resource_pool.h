/*
 * =====================================================================================
 *
 *       Filename:  client_resource_pool.h
 *
 *    Description:  client resource pool.
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
#ifndef CLIENT_RESOURCE_POOL_H
#define CLIENT_RESOURCE_POOL_H

#include <stdint.h>

class TaskRequest;
class Packet;
class ClientResourceSlot
{
public:
	ClientResourceSlot() : freenext(0),
						   freeprev(0),
						   task(NULL),
						   packet(NULL),
						   seq(0)
	{
	}
	~ClientResourceSlot();
	int freenext;
	/*only used if prev slot free too*/
	int freeprev;
	TaskRequest *task;
	Packet *packet;
	uint32_t seq;
};

/* automaticaly change size according to usage status */
/* resource from pool need */
class ClientResourcePool
{
public:
	ClientResourcePool() : total(0),
						   used(0),
						   freehead(0),
						   freetail(0),
						   taskslot(NULL)
	{
	}
	~ClientResourcePool();
	int Init();
	inline ClientResourceSlot *Slot(unsigned int id) { return &taskslot[id]; }
	/* clean resource allocated */
	int Alloc(unsigned int &id, uint32_t &seq);
	int Fill(unsigned int id);
	void Clean(unsigned int id);
	/* free, should clean first */
	void Free(unsigned int id, uint32_t seq);

private:
	unsigned int total;
	unsigned int used;
	unsigned int freehead;
	unsigned int freetail;
	ClientResourceSlot *taskslot;

	int Enlarge();
	void Shrink();
	inline int half_use()
	{
		return used <= total / 2 ? 1 : 0;
	}
};

#endif
