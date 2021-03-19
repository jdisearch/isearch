/*
 * =====================================================================================
 *
 *       Filename:  pooler.cc
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
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <fcntl.h>
#include "mem_check.h"

#include "poll_thread.h"
#include "poller.h"
#include "log.h"

PollerObject::~PollerObject()
{
	if (ownerUnit && epslot)
		ownerUnit->free_epoll_slot(epslot);

	if (netfd > 0)
	{
		log_debug("%d fd been closed!", netfd);
		close(netfd);
		netfd = 0;
	}
	if (eventSlot)
	{
		eventSlot->poller = NULL;
		eventSlot = NULL;
	}
}

int PollerObject::attach_poller(PollerUnit *unit)
{
	if (unit)
	{
		if (ownerUnit == NULL)
			ownerUnit = unit;
		else
			return -1;
	}
	if (netfd < 0)
		return -1;

	if (epslot <= 0)
	{
		if (!(epslot = ownerUnit->alloc_epoll_slot()))
			return -1;
		struct EpollSlot *slot = ownerUnit->get_slot(epslot);
		slot->poller = this;

		int flag = fcntl(netfd, F_GETFL);
		fcntl(netfd, F_SETFL, O_NONBLOCK | flag);
		struct epoll_event ev;
		memset(&ev, 0x0, sizeof(ev));
		ev.events = newEvents;
		slot->seq++;
		ev.data.u64 = ((unsigned long long)slot->seq << 32) + epslot;
		if (ownerUnit->Epctl(EPOLL_CTL_ADD, netfd, &ev) == 0)
			oldEvents = newEvents;
		else
		{
			ownerUnit->free_epoll_slot(epslot);
			log_warning("Epctl: %m");
			return -1;
		}
		return 0;
	}
	return apply_events();
}

int PollerObject::detach_poller()
{
	if (epslot)
	{
		struct epoll_event ev;
		memset(&ev, 0x0, sizeof(ev));
		if (ownerUnit->Epctl(EPOLL_CTL_DEL, netfd, &ev) == 0)
			oldEvents = newEvents;
		else
		{
			log_warning("Epctl: %m");
			return -1;
		}
		ownerUnit->free_epoll_slot(epslot);
		epslot = 0;
	}
	return 0;
}

int PollerObject::apply_events()
{
	if (epslot <= 0 || oldEvents == newEvents)
		return 0;

	struct epoll_event ev;
	memset(&ev, 0x0, sizeof(ev));

	ev.events = newEvents;
	struct EpollSlot *slot = ownerUnit->get_slot(epslot);
	slot->seq++;
	ev.data.u64 = ((unsigned long long)slot->seq << 32) + epslot;
	if (ownerUnit->Epctl(EPOLL_CTL_MOD, netfd, &ev) == 0)
		oldEvents = newEvents;
	else
	{
		log_warning("Epctl: %m");
		return -1;
	}

	return 0;
}

int PollerObject::delay_apply_events()
{
	if (epslot <= 0 || oldEvents == newEvents)
		return 0;

	if (eventSlot)
		return 0;

	eventSlot = ownerUnit->add_delay_event_poller(this);
	if (eventSlot == NULL)
	{
		log_error("max events!!!!!!");
		struct epoll_event ev;

		ev.events = newEvents;
		struct EpollSlot *slot = ownerUnit->get_slot(epslot);
		slot->seq++;
		ev.data.u64 = ((unsigned long long)slot->seq << 32) + epslot;
		if (ownerUnit->Epctl(EPOLL_CTL_MOD, netfd, &ev) == 0)
			oldEvents = newEvents;
		else
		{
			log_warning("Epctl: %m");
			return -1;
		}
	}

	return 0;
}

int PollerObject::check_link_status(void)
{
	char msg[1] = {0};
	int err = 0;

	err = recv(netfd, msg, sizeof(msg), MSG_DONTWAIT | MSG_PEEK);

	/* client already close connection. */
	if (err == 0 || (err < 0 && errno != EAGAIN))
		return -1;
	return 0;
}

void PollerObject::init_poll_fd(struct pollfd *pfd)
{
	pfd->fd = netfd;
	pfd->events = newEvents;
	pfd->revents = 0;
}

void PollerObject::input_notify(void)
{
	enable_input(false);
}

void PollerObject::output_notify(void)
{
	enable_output(false);
}

int PollerUnit::totalEventSlot = 40960;

void PollerObject::hangup_notify(void)
{
	delete this;
}

PollerUnit::PollerUnit(int mp)
{
	maxPollers = mp;

	eeSize = maxPollers > 1024 ? 1024 : maxPollers;
	epfd = -1;
	ep_events = NULL;
	pollerTable = NULL;
	freeSlotList = 0;
	usedPollers = 0;
	//not initailize eventCnt variable may crash, fix crash bug by linjinming 2014-05-18
	eventCnt = 0;
}

PollerUnit::~PollerUnit()
{
	// skip first one
	for (int i = 1; i < maxPollers; i++)
	{
		if (pollerTable[i].freeList)
			continue;
	}

	FREE_CLEAR(pollerTable);

	if (epfd != -1)
	{
		close(epfd);
		epfd = -1;
	}

	FREE_CLEAR(ep_events);
}

int PollerUnit::set_max_pollers(int mp)
{
	if (epfd >= 0)
		return -1;
	maxPollers = mp;
	return 0;
}

int PollerUnit::initialize_poller_unit(void)
{
	pollerTable = (struct EpollSlot *)CALLOC(maxPollers, sizeof(*pollerTable));

	if (!pollerTable)
	{
		log_error("calloc failed, num=%d, %m", maxPollers);
		return -1;
	}

	// already zero-ed
	for (int i = 1; i < maxPollers - 1; i++)
	{
		pollerTable[i].freeList = i + 1;
	}

	pollerTable[maxPollers - 1].freeList = 0;
	freeSlotList = 1;

	ep_events = (struct epoll_event *)CALLOC(eeSize, sizeof(struct epoll_event));

	if (!ep_events)
	{
		log_error("malloc failed, %m");
		return -1;
	}

	if ((epfd = epoll_create(maxPollers)) == -1)
	{
		log_warning("epoll_create failed, %m");
		return -1;
	}
	fcntl(epfd, F_SETFD, FD_CLOEXEC);
	return 0;
}

inline int PollerUnit::verify_events(struct epoll_event *ev)
{
	int idx = EPOLL_DATA_SLOT(ev);

	if ((idx >= maxPollers) || (EPOLL_DATA_SEQ(ev) != pollerTable[idx].seq))
	{
		return -1;
	}

	if (pollerTable[idx].poller == NULL || pollerTable[idx].freeList != 0)
	{
		log_notice("receive invalid epoll event. idx=%d seq=%d poller=%p freelist=%d event=%x",
				   idx, (int)EPOLL_DATA_SEQ(ev), pollerTable[idx].poller,
				   pollerTable[idx].freeList, ev->events);
		return -1;
	}
	return 0;
}

void PollerUnit::free_epoll_slot(int n)
{
	if (n <= 0)
		return;
	pollerTable[n].freeList = freeSlotList;
	freeSlotList = n;
	usedPollers--;
	pollerTable[n].seq++;
	pollerTable[n].poller = NULL;
}

int PollerUnit::alloc_epoll_slot()
{
	if (0 == freeSlotList)
	{
		log_crit("no free epoll slot, usedPollers = %d", usedPollers);
		return -1;
	}

	int n = freeSlotList;

	usedPollers++;
	freeSlotList = pollerTable[n].freeList;
	pollerTable[n].freeList = 0;

	return n;
}

int PollerUnit::Epctl(int op, int fd, struct epoll_event *events)
{
	if (epoll_ctl(epfd, op, fd, events) == -1)
	{
		log_warning("epoll_ctl error, epfd=%d, fd=%d", epfd, fd);

		return -1;
	}

	return 0;
}

int PollerUnit::wait_poller_events(int timeout)
{
	nrEvents = epoll_wait(epfd, ep_events, eeSize, timeout);
	return nrEvents;
}

void PollerUnit::process_poller_events(void)
{
	for (int i = 0; i < nrEvents; i++)
	{
		if (verify_events(ep_events + i) == -1)
		{
			log_notice("verify_events failed, ep_events[%d].data.u64 = %llu", i, (unsigned long long)ep_events[i].data.u64);
			continue;
		}

		EpollSlot *s = &pollerTable[EPOLL_DATA_SLOT(ep_events + i)];
		PollerObject *p = s->poller;

		p->newEvents = p->oldEvents;
		if (ep_events[i].events & (EPOLLHUP | EPOLLERR))
		{
			p->hangup_notify();
			continue;
		}

		if (ep_events[i].events & EPOLLIN)
			p->input_notify();

		s = &pollerTable[EPOLL_DATA_SLOT(ep_events + i)];
		if (s->poller == p && ep_events[i].events & EPOLLOUT)
			p->output_notify();

		s = &pollerTable[EPOLL_DATA_SLOT(ep_events + i)];
		if (s->poller == p)
			p->delay_apply_events();
	}
}

int PollerUnit::delay_apply_events()
{
	for (int i = 0; i < eventCnt; i++)
	{
		PollerObject *p = eventSlot[i].poller;
		if (p)
		{
			p->apply_events();
			eventSlot[i].poller = NULL;
			p->Cleardelay_apply_events();
		}
	}
	eventCnt = 0;
	return 0;
}
