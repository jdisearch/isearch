/*
 * =====================================================================================
 *
 *       Filename:  packet_base.cc
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
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <new>

#include "packet.h"
#include "protocol.h"

#include "log.h"
int Packet::Send(int fd)
{
	if (nv <= 0)
		return SendResultDone;

	struct msghdr msgh;
	msgh.msg_name = NULL;
	msgh.msg_namelen = 0;
	msgh.msg_iov = v;
	msgh.msg_iovlen = nv;
	msgh.msg_control = NULL;
	msgh.msg_controllen = 0;
	msgh.msg_flags = 0;

	int rv = sendmsg(fd, &msgh, MSG_DONTWAIT | MSG_NOSIGNAL);

	if (rv < 0)
	{
		if (errno == EINTR || errno == EAGAIN || errno == EINPROGRESS)
			return SendResultMoreData;
		return SendResultError;
	}
	if (rv == 0)
		return SendResultMoreData;
	bytes -= rv;
	if (bytes == 0)
	{
		nv = 0;
		return SendResultDone;
	}
	while (nv > 0 && rv >= (int64_t)v->iov_len)
	{
		rv -= v->iov_len;
		v++;
		nv--;
	}
	if (rv > 0)
	{
		v->iov_base = (char *)v->iov_base + rv;
		v->iov_len -= rv;
	}
	return nv == 0 ? SendResultDone : SendResultMoreData;
}

int Packet::send_to(int fd, void *addr, int len)
{
	if (nv <= 0)
		return SendResultDone;
	struct msghdr msgh;
	msgh.msg_name = addr;
	msgh.msg_namelen = len;
	msgh.msg_iov = v;
	msgh.msg_iovlen = nv;
	msgh.msg_control = NULL;
	msgh.msg_controllen = 0;
	msgh.msg_flags = 0;

	int rv = sendmsg(fd, &msgh, MSG_DONTWAIT | MSG_NOSIGNAL);

	if (rv < 0)
	{
#if 0
	    if(errno==EINTR || errno==EAGAIN || errno==EINPROGRESS)
		return SendResultMoreData;
#endif
		return SendResultError;
	}
	if (rv == 0)
		return SendResultError;

	bytes -= rv;
	if (bytes == 0)
	{
		nv = 0;
		return SendResultDone;
	}
	return SendResultError;
}

int Packet::encode_header(PacketHeader &header)
{
	int len = sizeof(header);
	for (int i = 0; i < DRequest::Section::Total; i++)
	{
		len += header.len[i];
#if __BYTE_ORDER == __BIG_ENDIAN
		header.len[i] = bswap_32(header.len[i]);
#endif
	}
	return len;
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
int Packet::encode_header(const PacketHeader &header)
{
	int len = sizeof(header);
	for (int i = 0; i < DRequest::Section::Total; i++)
	{
		len += header.len[i];
	}
	return len;
}
#endif
