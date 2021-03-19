/*
 * =====================================================================================
 *
 *       Filename:  proxy_receiver.h
 *
 *    Description:  agent receiver.
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
#ifndef __AGENT_RECEIVER_H__
#define __AGENT_RECEIVER_H__

#include <stdint.h>

#include "value.h"
#include "protocol.h"

#define AGENT_INIT_RECV_BUFF_SIZE 4096

typedef struct
{
	char *buff;
	int len;
	int pktCnt;
	int err;
} RecvedPacket;

class AgentReceiver
{
public:
	AgentReceiver(int f);
	virtual ~AgentReceiver();

	int Init();
	RecvedPacket Recv();

private:
	int fd;
	char *buffer;
	uint32_t offset;
	uint32_t buffSize;
	uint32_t pktTail;
	int pktCnt;

	int enlarge_buffer();
	bool is_need_enlarge_buffer();
	int recv_once();
	int real_recv();
	int recv_again();
	int decode_header(PacketHeader *header);
	void set_recved_info(RecvedPacket &packet);
	int count_packet();
};

#endif
