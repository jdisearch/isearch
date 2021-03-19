/*
 * =====================================================================================
 *
 *       Filename:  client_resource_pool.cc
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
#include <stdio.h>
#include <stdlib.h>

#include "client_resource_pool.h"
#include "packet.h"
#include "task_request.h"
#include "log.h"

ClientResourceSlot::~ClientResourceSlot()
{
    DELETE(task);
    DELETE(packet);
}

ClientResourcePool::~ClientResourcePool()
{
}

/* rules: */
/* head free slot: freeprev == 0
 * tail free slot: freenext == 0
 * used slot: freeprev == 0, freenext == 0
 * non-empty list: freehead, freetail not 0
 * empty list: freehead, freetail == 0
 * */
int ClientResourcePool::Init()
{
    taskslot = (ClientResourceSlot *)CALLOC(256, sizeof(ClientResourceSlot));
    if (taskslot == NULL)
        return -1;

    total = 256;
    used = 1;
    freehead = 1;
    freetail = total - 1;

    /* 1(freehead), 2..., total - 1(freetail) */
    taskslot[1].freenext = 2;
    taskslot[1].freeprev = 0;
    if ((taskslot[1].task = new TaskRequest) == NULL)
        return -1;
    if ((taskslot[1].packet = new Packet) == NULL)
        return -1;
    taskslot[1].seq = 0;

    for (unsigned int i = 2; i < total - 1; i++)
    {
        taskslot[i].freenext = i + 1;
        taskslot[i].freeprev = i - 1;
        if ((taskslot[i].task = new TaskRequest) == NULL)
            return -1;
        if ((taskslot[i].packet = new Packet) == NULL)
            return -1;
        taskslot[i].seq = 0;
    }

    taskslot[total - 1].freenext = 0;
    taskslot[total - 1].freeprev = total - 2;
    if ((taskslot[total - 1].task = new TaskRequest) == NULL)
        return -1;
    if ((taskslot[total - 1].packet = new Packet) == NULL)
        return -1;
    taskslot[total - 1].seq = 0;

    return 0;
}

int ClientResourcePool::Alloc(unsigned int &id, uint32_t &seq)
{
    if (freehead == 0)
    {
        if (Enlarge() < 0)
            return -1;
    }

    used++;
    id = freehead;
    freehead = taskslot[id].freenext;
    if (freehead == 0)
        freetail = 0;
    taskslot[id].freenext = 0;
    if (freetail != 0)
        taskslot[freehead].freeprev = 0;

    if (taskslot[id].task == NULL)
    {
        if ((taskslot[id].task = new TaskRequest) == NULL)
        {
            used--;
            if (freetail == 0)
                freetail = id;
            taskslot[id].freenext = freehead;
            taskslot[freehead].freeprev = id;
            freehead = id;
            return -1;
        }
    }
    if (taskslot[id].packet == NULL)
    {
        if ((taskslot[id].packet = new Packet) == NULL)
        {
            used--;
            if (freetail == 0)
                freetail = id;
            taskslot[id].freenext = freehead;
            taskslot[freehead].freeprev = id;
            freehead = id;
            return -1;
        }
    }

    taskslot[id].seq++;
    seq = taskslot[id].seq;

    return 0;
}

void ClientResourcePool::Clean(unsigned int id)
{
    if (taskslot[id].task)
        taskslot[id].task->Clean();
    if (taskslot[id].packet)
        taskslot[id].packet->Clean();
}

void ClientResourcePool::Free(unsigned int id, uint32_t seq)
{
    if (taskslot[id].seq != seq)
    {
        log_crit("client resource seq not right: %u, %u, not free", taskslot[id].seq, seq);
        return;
    }

    used--;
    taskslot[freehead].freeprev = id;
    taskslot[id].freenext = freehead;
    freehead = id;
    if (freetail == 0)
        freetail = id;

}

int ClientResourcePool::Fill(unsigned int id)
{
    if (taskslot[id].task && taskslot[id].packet)
        return 0;

    if ((taskslot[id].task == NULL) && ((taskslot[id].task = new TaskRequest) == NULL))
        return -1;
    if ((taskslot[id].packet == NULL) && ((taskslot[id].packet = new Packet) == NULL))
        return -1;

    return 0;
}

/* enlarge when freehead == 0, empty list */
int ClientResourcePool::Enlarge()
{
    void *p;
    unsigned int old = total;

    p = REALLOC(taskslot, sizeof(ClientResourceSlot) * 2 * total);
    if (!p)
    {
        log_warning("enlarge taskslot failed, current CTaskRequestSlot %u", total);
        return -1;
    }

    total *= 2;
    freehead = old;
    freetail = total - 1;

    taskslot[old].freenext = old + 1;
    taskslot[old].freeprev = 0;
    if ((taskslot[old].task = new TaskRequest) == NULL)
    {
        log_warning("no mem for new task slot, current CTaskRequestSlot %u", total);
        return -1;
    }
    if ((taskslot[old].packet = new Packet) == NULL)
    {
        log_warning("no mem for new task slot, current CTaskRequestSlot %u", total);
        return -1;
    }
    taskslot[old].seq = 0;

    for (unsigned int i = old + 1; i < total - 1; i++)
    {
        taskslot[i].freenext = i + 1;
        taskslot[i].freeprev = i - 1;
        if ((taskslot[i].task = new TaskRequest) == NULL)
        {
            log_warning("no mem for new task slot, current CTaskRequestSlot %u", total);
            return -1;
        }
        if ((taskslot[i].packet = new Packet) == NULL)
        {
            log_warning("no mem for new task slot, current CTaskRequestSlot %u", total);
            return -1;
        }
        taskslot[i].seq = 0;
    }

    taskslot[total - 1].freenext = 0;
    taskslot[total - 1].freeprev = total - 2;
    if (!(taskslot[total - 1].task = new TaskRequest))
    {
        log_warning("no mem for new task slot, current CTaskRequestSlot %u", total);
        return -1;
    }
    if (!(taskslot[total - 1].packet = new Packet))
    {
        log_warning("no mem for new task slot, current CTaskRequestSlot %u", total);
        return -1;
    }
    taskslot[total - 1].seq = 0;

    return 0;
}

void ClientResourcePool::Shrink()
{
    int shrink_cnt = (total - used) / 2;
    int slot = freetail;

    if (freetail == 0)
        return;

    for (int i = 0; i < shrink_cnt; i++)
    {
        if (taskslot[slot].task)
        {
            delete taskslot[slot].task;
            taskslot[slot].task = NULL;
        }
        if (taskslot[slot].packet)
        {
            delete taskslot[slot].packet;
            taskslot[slot].packet = NULL;
        }

        slot = taskslot[slot].freeprev;
        if (slot == 0)
            break;
    }
}
