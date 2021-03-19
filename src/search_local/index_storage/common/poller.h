/*
 * =====================================================================================
 *
 *       Filename:  pooler.h
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
#ifndef __POLLER_H__
#define __POLLER_H__

#include <arpa/inet.h>
#include <sys/poll.h>
#include "myepoll.h"
#include "list.h"

#define EPOLL_DATA_SLOT(x) ((x)->data.u64 & 0xFFFFFFFF)
#define EPOLL_DATA_SEQ(x) ((x)->data.u64 >> 32)

class PollerUnit;
class PollerObject;

struct EpollSlot
{
	PollerObject *poller;
	uint32_t seq;
	uint32_t freeList;
};

struct EventSlot
{
	PollerObject *poller;
};

class PollerObject
{
public:
	PollerObject(PollerUnit *o = NULL, int fd = 0) : ownerUnit(o),
													 netfd(fd),
													 newEvents(0),
													 oldEvents(0),
													 epslot(0)
	{
	}

	virtual ~PollerObject();

	virtual void input_notify(void);
	virtual void output_notify(void);
	virtual void hangup_notify(void);

	void enable_input(void)
	{
		newEvents |= EPOLLIN;
	}
	void enable_output(void)
	{
		newEvents |= EPOLLOUT;
	}
	void DisableInput(void)
	{
		newEvents &= ~EPOLLIN;
	}
	void disable_output(void)
	{
		newEvents &= ~EPOLLOUT;
	}

	void enable_input(bool i)
	{
		if (i)
			newEvents |= EPOLLIN;
		else
			newEvents &= ~EPOLLIN;
	}
	void enable_output(bool o)
	{
		if (o)
			newEvents |= EPOLLOUT;
		else
			newEvents &= ~EPOLLOUT;
	}

	int attach_poller(PollerUnit *thread = NULL);
	int detach_poller(void);
	int apply_events();
	int delay_apply_events();
	void Cleardelay_apply_events() { eventSlot = NULL; }
	int check_link_status(void);

	void init_poll_fd(struct pollfd *);

	friend class PollerUnit;

protected:
	PollerUnit *ownerUnit;
	int netfd;
	int newEvents;
	int oldEvents;
	int epslot;
	struct EventSlot *eventSlot;
};

class PollerUnit
{
public:
	friend class PollerObject;
	PollerUnit(int mp);
	virtual ~PollerUnit();

	int set_max_pollers(int mp);
	int get_max_pollers(void) const { return maxPollers; }
	int initialize_poller_unit(void);
	int wait_poller_events(int);
	void process_poller_events(void);
	int get_fd(void) { return epfd; }
	int delay_apply_events();

private:
	int verify_events(struct epoll_event *);
	int Epctl(int op, int fd, struct epoll_event *events);
	struct EpollSlot *get_slot(int n) { return &pollerTable[n]; }
	const struct EpollSlot *get_slot(int n) const { return &pollerTable[n]; }

	void free_epoll_slot(int n);
	int alloc_epoll_slot(void);
	struct EventSlot *add_delay_event_poller(PollerObject *p)
	{
		if (eventCnt == totalEventSlot)
			return NULL;
		eventSlot[eventCnt++].poller = p;
		return &eventSlot[eventCnt - 1];
	}

private:
	struct EpollSlot *pollerTable;
	struct epoll_event *ep_events;
	int epfd;
	int eeSize;
	int freeSlotList;
	int maxPollers;
	int usedPollers;
	/* FIXME: maybe too small */
	static int totalEventSlot;
	struct EventSlot eventSlot[40960];
	int eventCnt;

protected:
	int nrEvents;
};

#endif
