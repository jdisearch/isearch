/*
 * =====================================================================================
 *
 *       Filename:  proxy_sender.cc
 *
 *    Description:  agent sender.
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
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "proxy_sender.h"
#include "packet.h"
#include "log.h"

AgentSender::AgentSender(int f) : fd(f),
                                  vec(NULL),
                                  totalVec(1024),
                                  currVec(0),
                                  packet(NULL),
                                  totalPacket(512),
                                  currPacket(0),
                                  totalLen(0),
                                  sended(0),
                                  leftLen(0),
                                  broken(0)
{
}

AgentSender::~AgentSender()
{
    /* fd will closed by ClientAgent */
    for (uint32_t j = 0; j < currPacket; j++)
    {
        if (packet[j])
        {
            packet[j]->free_result_buff();
            delete packet[j];
        }
    }
    if (vec)
        free(vec);
    if (packet)
        free(packet);
}

int AgentSender::Init()
{
    vec = (struct iovec *)malloc(1024 * sizeof(struct iovec));
    if (NULL == vec)
    {
        broken = 1;
        return -1;
    }

    if (vec)
    {
        packet = (Packet **)malloc(512 * sizeof(Packet *));
        if (NULL == packet)
        {
            broken = 1;
            return -1;
        }
    }

    return 0;
}

int AgentSender::add_packet(Packet *pkt)
{
    if (currVec == totalVec)
    {
        vec = (struct iovec *)realloc(vec, totalVec * 2 * sizeof(struct iovec));
        if (NULL == vec)
        {
            broken = 1;
            return -ENOMEM;
        }
        totalVec *= 2;
    }

    if (currPacket == totalPacket)
    {
        packet = (Packet **)realloc(packet, totalPacket * 2 * sizeof(Packet *));
        if (NULL == packet)
        {
            broken = 1;
            return -ENOMEM;
        }
        totalPacket *= 2;
    }

    for (int i = 0; i < pkt->vec_count(); i++)
    {
        vec[currVec++] = pkt->IOVec()[i];
    }
    packet[currPacket++] = pkt;
    leftLen += pkt->Bytes();

    return 0;
}

int AgentSender::Send()
{
    if (0 == leftLen)
        return 0;

    int sd;
    struct msghdr msgh;
    uint32_t cursor = 0;
    struct iovec *v = vec;
    uint32_t pcursor = 0;
    Packet **p = packet;

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = vec;
    msgh.msg_iovlen = currVec;
    msgh.msg_control = NULL;
    msgh.msg_controllen = 0;
    msgh.msg_flags = 0;

    sd = sendmsg(fd, &msgh, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (sd < 0)
    {
        if (EINTR == errno || EAGAIN == errno || EINPROGRESS == errno)
            return 0;
        log_error("agent sender send error. errno: %d, left len: %d, currVec: %d, IOV_MAX: %d", errno, leftLen, currVec, IOV_MAX);
        broken = 1;
        return -1;
    }

    totalLen = leftLen;
    sended = sd;
    leftLen -= sended;

    if (0 == sd)
        return 0;

    while (cursor < currVec && (uint32_t)sd >= v->iov_len)
    {
        sd -= v->iov_len;
        cursor++;
        v++;
        (*p)->send_done_one_vec();
        if ((*p)->is_send_done())
        {
            (*p)->free_result_buff();
            delete *p;
            pcursor++;
            p++;
        }
    }

    if (sd > 0)
    {
        v->iov_base = (char *)v->iov_base + sd;
        v->iov_len -= sd;
    }

    memmove((void *)vec, (void *)(vec + cursor), (currVec - cursor) * sizeof(struct iovec));
    currVec -= cursor;

    memmove((void *)packet, (void *)(packet + pcursor), (currPacket - pcursor) * sizeof(Packet *));
    currPacket -= pcursor;

    return 0;
}
